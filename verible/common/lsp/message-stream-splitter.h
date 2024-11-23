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

#ifndef VERIBLE_COMMON_LSP_MESSAGE_STREAM_SPLITTER_H
#define VERIBLE_COMMON_LSP_MESSAGE_STREAM_SPLITTER_H

#include <cstddef>
#include <functional>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"

namespace verible {
namespace lsp {
// Splits messages that are formatted as header + body coming from some
// abstracted input stream and calls a handler for each complete message it
// receives.
//
// The MessageStreamSplitter does not read data directly from a source but
// gets handed a read function to get the data from. This allows to use this
// in different environments from testing response to different behavior of the
// read() to using it with a filedescriptor event dispatcher (select()).
//
// The simplest implementation of the "ReadFun" just wraps a system read() call.
//
// The header data MUST contain a Content-Length header.
class MessageStreamSplitter {
 public:
  // A function that reads from some source and writes up to "size" bytes
  // into the buffer. Returns the number of bytes read.
  // Blocks until there is content or returns '0' on end-of-file. Values
  // below zero indicate errors.
  // Only the amount of bytes available at the time of the call are filled
  // into the buffer, so the return value can be less than size.
  // So essentially: this behaves like the standard read() system call.
  using ReadFun = std::function<int(char *buf, int size)>;

  // Function called with each complete message that has been extracted from
  // the stream.
  using MessageProcessFun =
      std::function<void(absl::string_view header, absl::string_view body)>;

  // Optional parameter is "initial_read_buffer_size" for the initial
  // internal buffer size (will be realloc'ed when needed).
  explicit MessageStreamSplitter(size_t initial_read_buffer_size = 4096)
      : read_buffer_(initial_read_buffer_size) {}
  MessageStreamSplitter(const MessageStreamSplitter &) = delete;
  MessageStreamSplitter &operator=(const MessageStreamSplitter &) = delete;

  // Set the function that will receive extracted message bodies.
  void SetMessageProcessor(const MessageProcessFun &message_processor) {
    message_processor_ = message_processor;
  }

  // The passed "read_fun()" is called exactly _once_ to get
  // the next amount of data and calls the message processor for each complete
  // message found. Partial data received is retained to be re-considered on
  // the next call to PullFrom().
  //
  // Within the context of this method, the message processor might be
  // called zero to multiple times depending on how much data arrives from
  // the read.
  //
  // Note: The once-call behaviour allows to hook this into some file-descriptor
  // event dispatcher (e.g using select()).
  //
  // Returns with an ok status until EOF or some error occurs.
  // Code
  //  - kUnavailable     : regular EOF, no data pending. A 'good' non-ok status.
  //  - kDataloss        : got EOF, but still incomplete data pending.
  //  - kInvalidArgument : stream corrupted, couldn't read header.
  absl::Status PullFrom(const ReadFun &read_fun);

  // -- Statistical data

  size_t StatLargestBodySeen() const { return stats_largest_body_; }
  size_t StatTotalBytesRead() const { return stats_total_bytes_read_; }

 private:
  int ParseHeaderGetBodyOffset(absl::string_view data, int *body_size);
  absl::Status ProcessContainedMessages(absl::string_view *data);
  absl::Status ReadInput(const ReadFun &read_fun);

  std::vector<char> read_buffer_;
  absl::string_view pending_data_;

  MessageProcessFun message_processor_;

  size_t stats_largest_body_ = 0;
  size_t stats_total_bytes_read_ = 0;
};
}  // namespace lsp
}  // namespace verible
#endif  // VERIBLE_COMMON_LSP_MESSAGE_STREAM_SPLITTER_H
