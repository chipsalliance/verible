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

#ifndef VERIBLE_COMMON_STRINGS_SPLIT_H_
#define VERIBLE_COMMON_STRINGS_SPLIT_H_

#include <cstddef>
#include <functional>
#include <string_view>
#include <vector>

namespace verible {
namespace internal {
inline size_t DelimiterSize(std::string_view str) { return str.length(); }

inline size_t DelimiterSize(char c) { return 1; }
}  // namespace internal

// Generator function that returns substrings between delimiters.
// Behaves like a std::function<std::string_view(DelimiterType)>.
// The delimiter could be different with each call.
// This can serve as a quick lexer or tokenizer for some applications.
// Compared to absl::StrSplit, the space needed to store results one at a time
// is determined by the caller, and could even be constant.
//
// Example usage:
//   StringSpliterator gen("some, text, ...");
//   do {
//     std::string_view token = gen(',');
//     // do something with token
//   } while (gen);
//
// See also MakeStringSpliterator().
class StringSpliterator {
 public:
  explicit StringSpliterator(std::string_view original)
      : remainder_(original) {}

  // Copy-able, movable, assignable.
  StringSpliterator(const StringSpliterator &) = default;
  StringSpliterator(StringSpliterator &&) = default;
  StringSpliterator &operator=(const StringSpliterator &) = default;
  StringSpliterator &operator=(StringSpliterator &&) = default;

  // Returns true if there is at least one result to come.
  operator bool() const {  // NOLINT(google-explicit-constructor)
    return !end_;
  }

  // Returns the substring up to the next occurrence of the delimiter,
  // and advances internal state to point to text after the delimiter.
  // If the delimiter is not found, return the remaining string.
  // Delimiter type D can be a string_view or char (overloads of
  // string_view::find()).
  // The heterogenous operator() lets objects of this type work as a
  // std::function<std::string_view(std::string_view)> and
  // std::function<std::string_view(char)>.
  template <class D>
  std::string_view operator()(const D delimiter) {
    const size_t pos = remainder_.find(delimiter);
    if (pos == std::string_view::npos) {
      // This is the last partition.
      // If the remainder_ was already empty, this will continue
      // to return empty strings.
      const std::string_view result(remainder_);
      remainder_.remove_prefix(remainder_.length());  // empty
      end_ = true;
      return result;
    }
    // More text follows after the next occurrence of the delimiter.
    // If the text ends with the delimiter, then the last string
    // returned before the end() will be empty.
    const std::string_view result(remainder_.substr(0, pos));
    // Skip over the delimiter.
    remainder_.remove_prefix(pos + internal::DelimiterSize(delimiter));
    return result;
  }

  // Returns the un-scanned portion of text.
  std::string_view Remainder() const { return remainder_; }

 private:
  // The remaining substring that has not been consumed.
  // With each call to operator(), this shrinks from the front.
  std::string_view remainder_;

  // A split that fails to find a delimiter still returns one element,
  // the original string, thus end_ should always be initialized to false.
  bool end_ = false;
};

// Convenience function that returns a string_view generator using
// StringSpliterator with the same delimiter on every split.
template <class D>
std::function<std::string_view()> MakeStringSpliterator(
    std::string_view original, D delimiter) {
  // note: in-lambda initializers require c++14
  auto splitter = StringSpliterator(original);
  return [=]() mutable /* splitter */ { return splitter(delimiter); };
}

// Returns line-based view of original text.
// Each line in the returned vector excludes the trailing \n.
// If original text did not terminate with a \n, interpret the final partial
// line as a whole line.
std::vector<std::string_view> SplitLines(std::string_view text);

// Returns line-based view of original text. Keeps the trailing \n.
// Lines in the returned vector include the trailing \n.
// If original text did not terminate with a \n, interpret the final partial
// line as a whole line.
std::vector<std::string_view> SplitLinesKeepLineTerminator(
    std::string_view text);

}  // namespace verible

#endif  // VERIBLE_COMMON_STRINGS_SPLIT_H_
