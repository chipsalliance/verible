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

#ifndef VERIBLE_COMMON_STRINGS_COMMENT_UTILS_H_
#define VERIBLE_COMMON_STRINGS_COMMENT_UTILS_H_

#include <string_view>

namespace verible {

// Removes /* and */ from block comments and // from end-line comments.
// Removes excess wrapping characters such as /**** and ****/ and /////.
// If text is not a comment, it is returned unmodified, but in general, do not
// rely on any specific behavior for non-comments.
// Result is always a substring of the original (bounds may be equal).
std::string_view StripComment(std::string_view);

// Same as StripComment, but also removes leading and trailing whitespace.
std::string_view StripCommentAndSpacePadding(std::string_view);

}  // namespace verible

#endif  // VERIBLE_COMMON_STRINGS_COMMENT_UTILS_H_
