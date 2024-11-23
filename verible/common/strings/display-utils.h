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

#ifndef VERIBLE_COMMON_STRINGS_DISPLAY_UTILS_H_
#define VERIBLE_COMMON_STRINGS_DISPLAY_UTILS_H_

#include <iosfwd>

#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"

namespace verible {

// Stream printable object that limits the length of text printed.
// Applications: debugging potentially long strings, where only the head and
// tail are sufficient to comprehend the text being referenced.
//
// usage: stream << AutoTruncate{text, limit};
//
// example output (limit: 9): "abc...xyz"
struct AutoTruncate {
  const absl::string_view text;
  // Maximum number of characters to show, including "..."
  const int max_chars;
};

std::ostream &operator<<(std::ostream &, const AutoTruncate &trunc);

// To help visualize strings with non-alphanumeric characters, this stream
// adapter prints special characters escaped using C escape sequences, without
// modifying or copying the original string.
//
// usage: stream << EscapeString(text);
struct EscapeString {
  const absl::string_view text;  // original text to be printed

  explicit EscapeString(absl::string_view text) : text(text) {}
};

std::ostream &operator<<(std::ostream &, const EscapeString &vis);

// To help visualize strings that consist of whitespace, this stream adapter
// prints spaces, tabs, and newlines with alternate text, without modifying or
// copying the original string.
//
// usage: stream << VisualizeWhitespace(text_with_lots_of_spaces);
struct VisualizeWhitespace {
  const absl::string_view text;  // original text to be printed

  const char space_alt;                 // spaces replacement
  const absl::string_view newline_alt;  // newline replacement
  const absl::string_view tab_alt;      // tab replacement

  // constructor needed for C++11
  explicit VisualizeWhitespace(absl::string_view text, char space_alt = '.',
                               absl::string_view newline_alt = "\\\n",
                               absl::string_view tab_alt = "#")
      : text(text),
        space_alt(space_alt),
        newline_alt(newline_alt),
        tab_alt(tab_alt) {}
};

std::ostream &operator<<(std::ostream &, const VisualizeWhitespace &vis);

// TODO(fangism): once C++17 becomes the minimum standard for building
// push the following block into an internal namespace, and use auto
// return types instead of directly naming these types.
// namespace internal {

// Helper struct for bundling parameters to absl::StrJoin.
// This is useful for contructing printer adapters for types that
// are typedefs/aliases of standard containers, and not their own class.
// For example, not every std::vector<int> wants to be formatted the same way.
// Be careful not to define std::ostream& operator<< for such types, as you
// accidentally create conflicting definitions, and violate ODR.
template <class T>
struct SequenceStreamFormatter {
  const T &sequence;  // binds to object that is to be printed
  absl::string_view separator;
  absl::string_view prefix;
  absl::string_view suffix;
  // TODO(fangism): pass in custom formatter object, and be able to nest
  // multiple levels of formatters.
};

// Redirects stream printing to abs::StrJoin wrapped in a single object.
template <class T>
std::ostream &operator<<(std::ostream &stream,
                         const SequenceStreamFormatter<T> &t) {
  return stream << t.prefix
                << absl::StrJoin(t.sequence.begin(), t.sequence.end(),
                                 t.separator, absl::StreamFormatter())
                << t.suffix;
}

// }  // namespace internal

// SequenceFormatter helps create custom formatters (pretty-printers) for
// standard container types, when providing a plain std::ostream& operator<<
// overload would be ill-advised.
// This is the next best alternative, even if it requires the caller to wrap
// plain container objects.
//
// Example usage (define the following for your specific container type):
// Suppose MySequenceType is a typedef to a container like std::list<int>.
// Define a forwarding function:
//
// [pre C++17]
// SequenceStreamFormatter<MySequenceType> MySequenceFormatter(
//     const MySequenceType& t) {
//   return verible::SequenceFormatter(t, " | ", "< ", " >");
// }
//
// [C++17 and higher, supporting auto return type]:
// auto MySequenceFormatter(const MySequenceType& t) {
//   return verible::SequenceFormatter(t, " | ", "< ", " >");
// }
//
// and call it:
//   stream << MySequenceFormatter(sequence_obj) << ...;
//
// to consistently produce text like:
//   "< 1 | 2 | 3 | ... >"
//
template <class T>
SequenceStreamFormatter<T> SequenceFormatter(const T &t,
                                             absl::string_view sep = ", ",
                                             absl::string_view prefix = "",
                                             absl::string_view suffix = "") {
  return SequenceStreamFormatter<T>{t, sep, prefix, suffix};
}

}  // namespace verible

#endif  // VERIBLE_COMMON_STRINGS_DISPLAY_UTILS_H_
