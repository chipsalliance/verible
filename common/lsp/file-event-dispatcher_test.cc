// Copyright 2021 The Verible Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "common/lsp/file-event-dispatcher.h"

#ifndef _WIN32
// Someone with knowledge of how to do something similar in Win32, please
// send a PR. In particular I don't know if it has the concept of a pipe
// that represents itself as two file-descriptors.

#include <unistd.h>

//
#include "absl/strings/string_view.h"
#include "gtest/gtest.h"

namespace verible {
namespace lsp {
TEST(FdMuxTest, IdleAndReadableCallHandled) {
  // To be able to do this test just in the main thread, we
  // register two handlers: one idle handler that is called after some short
  // timeout and a RunOnReadable handler waiting on the read-end of a pipe.
  //
  // Initially, the RunOnReadable handler will block as there is nothing
  // in the pipe, so the idle handler will eventually be called after the
  // timeout. When it is called, it will write into the pipe, which in turn
  // wakes up the reader.
  static constexpr absl::string_view kMessage = "Hello";

  FileEventDispatcher fdmux(42);  // Some wait time until idle is called.

  // Prepare a pipe so that we can send data to the waiting part.
  int read_write_pipe[2];
  ASSERT_EQ(pipe(read_write_pipe), 0);

  bool idle_was_called = false;
  bool read_was_called = false;

  const int read_fd = read_write_pipe[0];
  fdmux.RunOnReadable(read_fd, [read_fd, &read_was_called]() {
    char buffer[32];
    // We expect here that one read will contain the whole message.
    // Given that it is very short and written in one write-call, this is
    // a fair assumption.
    const int r = read(read_fd, buffer, sizeof(buffer));
    EXPECT_EQ(r, static_cast<int>(kMessage.length()));
    absl::string_view result(buffer, r);
    EXPECT_EQ(result, kMessage);
    read_was_called = true;
    return false;  // We only want to be called once
  });

  // Let's have the idle call write into the pipe, so that we wake up
  // the RunOnReadable(). That way, we can test two things in one go.
  const int write_fd = read_write_pipe[1];
  fdmux.RunOnIdle([write_fd, &idle_was_called]() {
    const int w = write(write_fd, kMessage.data(), kMessage.length());
    EXPECT_EQ(w, kMessage.length()) << "mmh, write() call failed";
    idle_was_called = true;
    return false;
  });

  fdmux.Loop();

  EXPECT_TRUE(idle_was_called);
  EXPECT_TRUE(read_was_called);
}
}  // namespace lsp
}  // namespace verible
#endif
