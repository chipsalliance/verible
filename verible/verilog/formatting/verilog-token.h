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

#ifndef VERIBLE_VERILOG_FORMATTING_VERILOG_TOKEN_H_
#define VERIBLE_VERILOG_FORMATTING_VERILOG_TOKEN_H_

#include "verible/verilog/parser/verilog-token-enum.h"

namespace verilog {
namespace formatter {

// Classification of token types into categories useful for formatting decisions
enum FormatTokenType {
  unknown = 0,
  identifier,
  keyword,
  numeric_base,  // e.g. 'd, 'b, 'h
  numeric_literal,
  string_literal,  // "foo"
  unary_operator,
  binary_operator,
  open_group,   // ( { [ '{
  close_group,  // ) } ]
  hierarchy,    // :: .
  edge_descriptor,
  comment_block,  // /*comment*/
  eol_comment,    // // comment
};

// Converts a leaf token enum into a FormatTokenType enum for categorizing
// FormatTokens. This is used for determining spaces between tokens.
// An unknown return value is an error condition the caller should handle.
FormatTokenType GetFormatTokenType(verilog_tokentype e);

// Returns true if the FormatTokenType is a comment
bool IsComment(FormatTokenType token_type);

}  // namespace formatter
}  // namespace verilog

#endif  // VERIBLE_VERILOG_FORMATTING_VERILOG_TOKEN_H_
