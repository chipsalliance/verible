// Copyright 2022 The Verible Authors.
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

#ifndef COMMON_STRINGS_MEM_BLOCK_H
#define COMMON_STRINGS_MEM_BLOCK_H

#include <string>
#include <string_view>

namespace verible {
// A representation of a block of (readonly) memory that is owned by MemBlock.
// Recommended use is to create it somewhere and pass a pointer in a
// std::unique_ptr<> to explicitly pass unique ownership in a move semantic or
// as std::shared_ptr<> if needed in multiple places.
class MemBlock {
 public:
  virtual ~MemBlock() = default;
  virtual std::string_view AsStringView() const = 0;

 protected:
  MemBlock() = default;

 public:
  MemBlock(const MemBlock &) = delete;
  MemBlock(MemBlock &&) = delete;
  MemBlock &operator=(const MemBlock &) = delete;
  MemBlock &operator=(MemBlock &&) = delete;
};

// An implementation of MemBlock backed by a std::string
class StringMemBlock final : public MemBlock {
 public:
  StringMemBlock() = default;
  explicit StringMemBlock(std::string &&move_from) : content_(move_from) {}
  explicit StringMemBlock(std::string_view copy_from)
      : content_(copy_from.begin(), copy_from.end()) {}

  // Assign/modify content. Use sparingly, ideally only in initialization
  // as the expectation of the MemBlock is that it won't change later.
  std::string *mutable_content() { return &content_; }

  std::string_view AsStringView() const final { return content_; }

 private:
  std::string content_;
};

// FYI common/util:file_util provides a memory mapping implementation.

}  // namespace verible
#endif  // COMMON_STRINGS_MEM_BLOCK_H
