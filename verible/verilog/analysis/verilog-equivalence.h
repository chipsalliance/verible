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

#ifndef VERIBLE_VERILOG_ANALYSIS_VERILOG_EQUIVALENCE_H_
#define VERIBLE_VERILOG_ANALYSIS_VERILOG_EQUIVALENCE_H_

#include <functional>
#include <iosfwd>

#include "absl/strings/string_view.h"
#include "verible/common/text/token-info.h"
#include "verible/common/text/token-stream-view.h"

namespace verilog {

// Encode different results of diff-ing two inputs.
enum class DiffStatus {
  kEquivalent,  // inputs are considered equivalent
  kDifferent,   // inputs are considered different
  kLeftError,   // some error processing left input
  kRightError,  // some error processing right input
};

std::ostream &operator<<(std::ostream &, DiffStatus);

// Compares two strings for equivalence.
// Returns a DiffStatus that captures 'equivalence' ignoring tokens filtered
// out by remove_predicate, and using the equal_comparator binary predicate.
// 'lexer' tokenizes a string into substrings (tokens), and is used to
// recursively lex expandable tokens as determined by recursion_predicate.
// (Some tokens like macro definition bodies are initially read as one large
// token by some lexers.)
// 'token_printer' reports diagnostics on token differences and errors.
// If errstream is provided, print detailed error message to that stream.
// TODO(fangism): move this to language-agnostic common/analysis library.
DiffStatus LexicallyEquivalent(
    absl::string_view left, absl::string_view right,
    const std::function<bool(absl::string_view, verible::TokenSequence *)>
        &lexer,
    const std::function<bool(const verible::TokenInfo &)> &recursion_predicate,
    const std::function<bool(const verible::TokenInfo &)> &remove_predicate,
    const std::function<bool(const verible::TokenInfo &,
                             const verible::TokenInfo &)> &equal_comparator,
    const std::function<void(const verible::TokenInfo &, std::ostream &)>
        &token_printer,
    std::ostream *errstream = nullptr);

// Returns a DiffStatus that captures 'equivalence' ignoring tokens filtered
// out by remove_predicate, and using the equal_comparator binary predicate.
// If errstream is provided, print detailed error message to that stream.
DiffStatus VerilogLexicallyEquivalent(
    absl::string_view left, absl::string_view right,
    const std::function<bool(const verible::TokenInfo &)> &remove_predicate,
    const std::function<bool(const verible::TokenInfo &,
                             const verible::TokenInfo &)> &equal_comparator,
    std::ostream *errstream = nullptr);

// Returns true if both token sequences are equivalent, ignoring whitespace.
// If errstream is provided, print detailed error message to that stream.
DiffStatus FormatEquivalent(absl::string_view left, absl::string_view right,
                            std::ostream *errstream = nullptr);

// Similar to FormatEquivalent except that:
//   1) whitespaces must match
//   2) identifiers only need to match in length and not string content to be
//      considered equal.
// Such equivalence is good for formatter test cases.
DiffStatus ObfuscationEquivalent(absl::string_view left,
                                 absl::string_view right,
                                 std::ostream *errstream = nullptr);

}  // namespace verilog

#endif  // VERIBLE_VERILOG_ANALYSIS_VERILOG_EQUIVALENCE_H_
