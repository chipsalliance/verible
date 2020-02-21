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
#include "common/text/token_stream_view.h"

namespace verilog {

// Returns true if both token sequences are equivalent, ignoring tokens filtered
// out by remove_predicate, and using the equal_comparator binary predicate.
// If errstream is provided, print detailed error message to that stream.
bool LexicallyEquivalent(
    const verible::TokenSequence& left, const verible::TokenSequence& right,
    std::function<bool(const verible::TokenInfo&)> remove_predicate,
    std::function<bool(const verible::TokenInfo&, const verible::TokenInfo&)>
        equal_comparator,
    std::ostream* errstream = nullptr);

// Returns true if both token sequences are equivalent, ignoring whitespace.
// If errstream is provided, print detailed error message to that stream.
bool FormatEquivalent(const verible::TokenSequence& left,
                      const verible::TokenSequence& right,
                      std::ostream* errstream = nullptr);

}  // namespace verilog

#endif  // VERIBLE_VERILOG_ANALYSIS_VERILOG_EQUIVALENCE_H_
