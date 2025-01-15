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

#include <ostream>
#include <string_view>

#include "verible/common/strings/line-column-map.h"
#include "verible/common/strings/position.h"  // for ByteOffsetSet, LineNumberSet
#include "verible/common/text/token-stream-view.h"

namespace verilog {
namespace formatter {

// Returns a representation of byte offsets where true (membership) means
// formatting is disabled.
verible::ByteOffsetSet DisableFormattingRanges(
    std::string_view text, const verible::TokenSequence &tokens);

// TODO(fangism): Move these next functions into common/formatting.
// Same with the above types.

// Translates line numbers into a set of byte ranges to disable formatting.
verible::ByteOffsetSet EnabledLinesToDisabledByteRanges(
    const verible::LineNumberSet &line_numbers,
    const verible::LineColumnMap &line_column_map);

// Formats space between tokens while honoring formatting-disabled ranges.
// 'text_base' is the entire original text that was formatted.
// 'space_text' should be a slice of text inside 'text_base' containing only
// whitespace, including tabs/newlines.
// 'disabled_ranges' is the set of byte offsets in the original text to
// preserve.
// Output is written to 'stream'.
void FormatWhitespaceWithDisabledByteRanges(
    std::string_view text_base, std::string_view space_text,
    const verible::ByteOffsetSet &disabled_ranges, bool include_disabled_ranges,
    std::ostream &stream);

}  // namespace formatter
}  // namespace verilog

#endif  // VERIBLE_VERILOG_FORMATTING_COMMENT_CONTROLS_H_
