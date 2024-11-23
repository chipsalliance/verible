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

#ifndef VERIBLE_VERILOG_FORMATTING_VERILOG_TOKEN_CLASSIFICATIONS_H_
#define VERIBLE_VERILOG_FORMATTING_VERILOG_TOKEN_CLASSIFICATIONS_H_

#include "verible/verilog/parser/verilog-token-enum.h"

namespace verilog {

// Returns true if the token type is whitespace (spaces, tabs, newlines).
bool IsWhitespace(verilog_tokentype token_type);

// Returns true if the verilog_tokentype is a comment
bool IsComment(verilog_tokentype token_type);

// Returns true if token enum *can* be a unary operator.
bool IsUnaryOperator(verilog_tokentype);

// Returns true if operator is an associative binary operator.
bool IsAssociativeOperator(verilog_tokentype);

// Returns true if token enum *can* be a ternary operator.
bool IsTernaryOperator(verilog_tokentype);

// Returns true for `ifdef, `else, etc.
bool IsPreprocessorControlFlow(verilog_tokentype);

// Returns true for `ifdef, `define, `include, `undef, etc.
bool IsPreprocessorKeyword(verilog_tokentype);

// Returns true for any preprocessing token, not just control flow.
bool IsPreprocessorControlToken(verilog_tokentype token_type);

// Returns true if token enum is 'end', 'endmodule', or 'end*'
bool IsEndKeyword(verilog_tokentype);

// Returns true if token is unlexed text that can be further expanded.
bool IsUnlexed(verilog_tokentype);

// Returns true if token is a type that corresponds to a user-written symbol
// name.  Includes regular identifiers, system-task identifiers, macro
// identifiers.
bool IsIdentifierLike(verilog_tokentype);

// TODO(fangism): Identify specially lexed tokens that require a newline after.
// e.g. MacroIdItem, TK_EOL_COMMENT, ...
// bool RequiresNewlineAfterToken(verilog_tokentype);

}  // namespace verilog

#endif  // VERIBLE_VERILOG_FORMATTING_VERILOG_TOKEN_CLASSIFICATIONS_H_
