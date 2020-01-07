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

#ifndef VERIBLE_VERILOG_FORMATTING_COMMENT_CONTROLS_H_
#define VERIBLE_VERILOG_FORMATTING_COMMENT_CONTROLS_H_

#include <vector>

#include "absl/strings/string_view.h"
#include "common/text/token_stream_view.h"

namespace verilog {
namespace formatter {

// Returns a representation of byte offsets where true (membership) means
// formatting is disabled.
// TODO(b/147247103): evaluate using a proper interval set data structure.
std::vector<bool> DisableFormattingRanges(absl::string_view text,
                                          const verible::TokenSequence& tokens);

// Sets every bit indexed [start, end).
// Automatically extends size to cover end, if needed.
// TODO(b/147247103): Replace with IntervalSet::Add().
void SetRange(std::vector<bool>* disable_set, int start, int end);

// Returns true if every bit in the [start, end) range is true/set.
// If start == end (empty), the degenerate case is that all interval sets
// contain all empty ranges, return true.
// If 'end' is past intervals.size(), then treat all out-of-range values
// as false, and hence, return false.
// TODO(b/147247103): Replace with IntervalSet::Contains().
bool ContainsRange(const std::vector<bool>& intervals, int start, int end);

}  // namespace formatter
}  // namespace verilog

#endif  // VERIBLE_VERILOG_FORMATTING_COMMENT_CONTROLS_H_
