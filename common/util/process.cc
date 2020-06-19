// Copyright 2017-2020 The Verible Authors.
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

#include "common/util/process.h"

#include <array>
#include <cstdio>  // for fgets(), popen(), pclose()
#include <memory>

namespace verible {

SubprocessOutput ExecSubprocess(const char* command) {
  constexpr int kBufferSize = 256;
  SubprocessOutput result;
  // Capture the exit code from pclose() to result.
  auto closer = [&result](FILE* f) {
    if (f != nullptr) {
      result.exit_code = pclose(f);
    }
  };
  {
    const std::unique_ptr<FILE, decltype(closer)>  //
        pipe(popen(command, "r"), closer);
    if (pipe == nullptr) {
      result.exit_code = -1;
      return result;
    }
    char buffer[kBufferSize];
    while (fgets(buffer, sizeof(buffer), pipe.get()) != nullptr) {
      // fgets() guarantees that buffer is null-terminated
      result.output += buffer;
    }
  }
  return result;
}

}  // namespace verible
