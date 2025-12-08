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

#ifndef VERIBLE_VERILOG_FORMATTING_TOKEN_ANNOTATOR_H_
#define VERIBLE_VERILOG_FORMATTING_TOKEN_ANNOTATOR_H_

#include <string_view>
#include <vector>

#include "verible/common/formatting/format-token.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/text-structure.h"
#include "verible/common/text/token-info.h"
#include "verible/verilog/formatting/format-style.h"

namespace verilog {
namespace formatter {

// Annotates inter-token information: spacing required between tokens,
// line-break penalties and decisions.
//   style: Verilog-specific configuration
//   tokens_begin, tokens_end: range of format tokens to be initialized.
// TODO(b/130091585): replace modifiable unwrapped line with a read-only
// struct and return separate annotations.
void AnnotateFormattingInformation(
    const FormatStyle &style, const verible::TextStructureView &text_structure,
    std::vector<verible::PreFormatToken> *format_tokens);

// This interface is only provided for testing, without requiring a
// TextStructureView.
//   buffer_start: start of the text buffer that is being formatted.
//   syntax_tree_root: syntax tree used for context-sensitive behavior.
//   eof_token: EOF token pointing to the end of the unformatted string.
void AnnotateFormattingInformation(
    const FormatStyle &style, std::string_view::const_iterator buffer_start,
    const verible::Symbol *syntax_tree_root,
    const verible::TokenInfo &eof_token,
    std::vector<verible::PreFormatToken> *format_tokens);

}  // namespace formatter
}  // namespace verilog

#endif  // VERIBLE_VERILOG_FORMATTING_TOKEN_ANNOTATOR_H_
