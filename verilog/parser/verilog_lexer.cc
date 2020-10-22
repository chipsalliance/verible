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

#include "verilog/parser/verilog_lexer.h"

#include "absl/strings/string_view.h"
#include "common/text/token_info.h"
#include "verilog/parser/verilog_token_enum.h"

namespace verilog {

using verible::TokenInfo;

VerilogLexer::VerilogLexer(const absl::string_view code)
    : parent_lexer_type(code),
      // These initial values should not matter, the lexer actions should
      // guarantee to set them before using them.
      balance_(0),
      macro_id_length_(0),
      macro_arg_length_(0) {}

void VerilogLexer::Restart(absl::string_view code) {
  parent_lexer_type::Restart(code);
  balance_ = 0;
  macro_id_length_ = 0;
  macro_arg_length_ = 0;
}

bool VerilogLexer::TokenIsError(const TokenInfo& token) const {
  // TODO(fangism): Distinguish different lexical errors by returning different
  // enums.
  return token.token_enum() == TK_OTHER;
}

bool VerilogLexer::KeepSyntaxTreeTokens(const TokenInfo& t) {
  switch (t.token_enum()) {
    case TK_COMMENT_BLOCK:  // fall-through
    case TK_EOL_COMMENT:    // fall-through
    case TK_ATTRIBUTE:      // fall-through
    case TK_SPACE:          // fall-through
    case TK_NEWLINE:
    case TK_LINE_CONT:
      // TODO(fangism): preserve newlines until after some preprocessing steps.
      return false;
    default:
      return true;
  }
}

void RecursiveLexText(absl::string_view text,
                      const std::function<void(const TokenInfo&)>& func) {
  VerilogLexer lexer(text);
  while (true) {
    const TokenInfo& subtoken(lexer.DoNextToken());
    if (subtoken.isEOF()) break;
    func(subtoken);
  }
}

}  // namespace verilog
