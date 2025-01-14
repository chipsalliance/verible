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

#include "verible/common/strings/comment-utils.h"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <string_view>

#include "absl/strings/ascii.h"
#include "verible/common/util/logging.h"
#include "verible/common/util/range.h"

namespace verible {

// Returns the number of occurences of a character c in the text's prefix.
static size_t CountLeadingChars(std::string_view text, char c) {
  const auto rpos = text.find_first_not_of(c);
  if (rpos == std::string_view::npos) return text.length();
  return rpos;
}

// Returns the number of occurences of a character c in the text's suffix.
static size_t CountTrailingChars(std::string_view text, char c) {
  const auto rpos = std::find_if(text.rbegin(), text.rend(),
                                 [=](const char ch) { return ch != c; });
  // if rpos == text.rend(), then return == text.length().
  return std::distance(text.rbegin(), rpos);
}

// Strips away leading /*** and trailing ***/ from block comments.
// Precondition: 'text' must begin with "/*" and end with "*/".
// TODO(fangism): strip away extra spaces and *s on each intermediate line,
// e.g.
//   /***
//    * keep this
//    * and this
//    **/
// Returns a substring within text, even if it is equivalent to empty.
static std::string_view StripBlockComment(std::string_view text) {
  // Adjust for multiple *'s like /**** and ****/ .
  // Strip off /* and */ first and then remove leading/trailing *'s.
  const size_t lpos = CountLeadingChars(text.substr(2), '*') + 2;
  auto text_slice = text;
  text_slice.remove_suffix(2);
  const size_t rtrim = CountTrailingChars(text_slice, '*') + 2;
  const size_t rpos = text.length() - rtrim;
  if (lpos > rpos) {
    // This can occur if comment looks like: /*******/
    if (lpos == 2 && rpos == 1) {
      // /*/ is not a valid block comment, so do not strip it.
      return text;
    }
    // Return an empty string_view ("") whose range is within text.
    return text.substr(2, 0);
  }
  return text.substr(lpos, rpos - lpos);
}

std::string_view StripComment(std::string_view text) {
  if (text.length() < 2) return text;  // cannot be an endline comment
  const std::string_view start = text.substr(0, 2);
  const std::string_view end = text.substr(text.length() - 2);
  if (start == "//") {
    const auto ltrim = CountLeadingChars(text.substr(2), '/') + 2;
    return text.substr(ltrim);
  }
  if (start == "/*" && end == "*/") {
    return StripBlockComment(text);
  }
  // else is not a well-formed comment
  return text;
}

std::string_view StripCommentAndSpacePadding(std::string_view text) {
  const auto stripped_text = StripComment(text);
  CHECK(verible::IsSubRange(stripped_text, text));
  const auto return_text = absl::StripAsciiWhitespace(stripped_text);
  CHECK(verible::IsSubRange(return_text, stripped_text));
  return return_text;
}

}  // namespace verible
