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

// Originally published at https://github.com/hzeller/ziplain
#ifndef VERIBLE_COMMON_UTIL_SIMPLE_ZIP_H_
#define VERIBLE_COMMON_UTIL_SIMPLE_ZIP_H_

#include <functional>
#include <memory>

#include "absl/strings/string_view.h"

namespace verible {
namespace zip {

// A ByteSource is a generator function that returns content, possibly chunked
// in multiple pieces.
//
// Each call to a ByteSink yields more content, non-empty string_views.
// End-of-data is signified by an empty string_view.
//
// The return values are only valid until the next call.
//
// The fact that ByteSource returns different results on each call implies that
// it has a state (e.g. successive read() calls); implementations need to make
// sure that even partial reads don't result in leaked resources.
using ByteSource = std::function<absl::string_view()>;

// A function that receives bytes. Consecutive calls concatenate. Return
// value 'true' indicates operation succeeded.
using ByteSink = std::function<bool(absl::string_view out)>;

// Utility function that wraps a string_view and provides a ByteSource. Use
// this if you have an in-memory representation of your content.
ByteSource MemoryByteSource(absl::string_view input);

// Utility function that reads the content of a file and provides a ByteSource.
// May return an empty function if file could not be opened
// (no other errors are reported. If you need error handling, write your own).
ByteSource FileByteSource(const char *filename);

// Encode a zip file. Call AddFile() 0..n times, then finalize with Finish()
// No more files can be added after Finish().
class Encoder {
 public:
  // Create a zip file encoder writing to ByteSink.
  // No compression on "compression_level" zero, otherwise deflate
  Encoder(int compression_level, ByteSink out);

  // Will also Finish() if not called already.
  ~Encoder();

  // Add a file with given filename and content from the generator function.
  bool AddFile(absl::string_view filename, const ByteSource &content_generator);

  // Finalize container.
  // After this, no new files can be added.
  // Note if your byte-sink is wrapping a file, you might need to close it
  // after Finish() returns.
  bool Finish();

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace zip
}  // namespace verible

#endif  // VERIBLE_COMMON_UTIL_SIMPLE_ZIP_H_
