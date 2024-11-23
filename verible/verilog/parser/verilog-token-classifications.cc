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

#include "verible/verilog/parser/verilog-token-classifications.h"

#include "verible/verilog/parser/verilog-token-enum.h"

namespace verilog {

bool IsWhitespace(verilog_tokentype token_type) {
  return (token_type == verilog_tokentype::TK_SPACE ||
          token_type == verilog_tokentype::TK_NEWLINE);
}

bool IsComment(verilog_tokentype token_type) {
  return (token_type == verilog_tokentype::TK_COMMENT_BLOCK ||
          token_type == verilog_tokentype::TK_EOL_COMMENT);
}

bool IsUnaryOperator(verilog_tokentype token_type) {
  switch (static_cast<int>(token_type)) {
    // See verilog/parser/verilog.y
    // TODO(fangism): find a way to generate this function automatically
    // from the yacc file, perhaps with extra annotations or metadata.
    case '+':
    case '-':
    case '~':
    case '&':
    case '!':
    case '|':
    case '^':
    case TK_NAND:
    case TK_NOR:
    case TK_NXOR:
    case TK_INCR:
    case TK_DECR:
      return true;
    default:
      return false;
  }
}

bool IsAssociativeOperator(verilog_tokentype op) {
  switch (static_cast<int>(op)) {
    case verilog_tokentype::TK_or:
    case verilog_tokentype::TK_and:
    case verilog_tokentype::TK_LAND:
    case verilog_tokentype::TK_LOR:
    case verilog_tokentype::TK_NXOR:
    case '+':
    case '*':
    case '^':
    case '|':
    case '&':
      return true;
    default:
      return false;
  }
}

bool IsTernaryOperator(verilog_tokentype token_type) {
  switch (static_cast<int>(token_type)) {
    case '?':
    case ':':
      return true;
    default:
      return false;
  }
}

bool IsPreprocessorControlFlow(verilog_tokentype token_type) {
  switch (token_type) {
    case verilog_tokentype::PP_ifdef:
    case verilog_tokentype::PP_ifndef:
    case verilog_tokentype::PP_elsif:
    case verilog_tokentype::PP_else:
    case verilog_tokentype::PP_endif:
      return true;
    default:
      return false;
  }
}

bool IsPreprocessorKeyword(verilog_tokentype token_type) {
  switch (token_type) {
    case verilog_tokentype::PP_include:
    case verilog_tokentype::PP_define:
    case verilog_tokentype::PP_ifdef:
    case verilog_tokentype::PP_ifndef:
    case verilog_tokentype::PP_else:
    case verilog_tokentype::PP_elsif:
    case verilog_tokentype::PP_endif:
    case verilog_tokentype::PP_undef:
      return true;
    default:
      return false;
  }
}

bool IsPreprocessorControlToken(verilog_tokentype token_type) {
  switch (token_type) {
    case verilog_tokentype::PP_Identifier:
    case verilog_tokentype::PP_include:
    case verilog_tokentype::PP_define:
    case verilog_tokentype::PP_define_body:
    case verilog_tokentype::PP_ifdef:
    case verilog_tokentype::PP_ifndef:
    case verilog_tokentype::PP_else:
    case verilog_tokentype::PP_elsif:
    case verilog_tokentype::PP_endif:
    case verilog_tokentype::PP_undef:
    case verilog_tokentype::PP_default_text:
      // Excludes macro call tokens.
      return true;
    default:
      return false;
  }
}

bool IsEndKeyword(verilog_tokentype token_type) {
  switch (token_type) {
    case verilog_tokentype::TK_end:
    case verilog_tokentype::TK_endcase:
    case verilog_tokentype::TK_endgroup:
    case verilog_tokentype::TK_endpackage:
    case verilog_tokentype::TK_endgenerate:
    case verilog_tokentype::TK_endinterface:
    case verilog_tokentype::TK_endfunction:
    case verilog_tokentype::TK_endtask:
    case verilog_tokentype::TK_endproperty:
    case verilog_tokentype::TK_endclocking:
    case verilog_tokentype::TK_endclass:
    case verilog_tokentype::TK_endmodule:
      // TODO(fangism): join and join* keywords?
      return true;
    default:
      return false;
  }
}

bool IsUnlexed(verilog_tokentype token_type) {
  switch (token_type) {
    case verilog_tokentype::MacroArg:
    case verilog_tokentype::PP_define_body:
      return true;
    default:
      return false;
  }
}

bool IsIdentifierLike(verilog_tokentype token_type) {
  switch (token_type) {
    case verilog_tokentype::SymbolIdentifier:
    case verilog_tokentype::PP_Identifier:
    case verilog_tokentype::MacroIdentifier:
    case verilog_tokentype::MacroIdItem:
    case verilog_tokentype::MacroCallId:
    case verilog_tokentype::SystemTFIdentifier:
    case verilog_tokentype::EscapedIdentifier:
      // specify block built-in functions
    case verilog_tokentype::TK_Srecrem:
    case verilog_tokentype::TK_Ssetuphold:
    case verilog_tokentype::TK_Speriod:
    case verilog_tokentype::TK_Shold:
    case verilog_tokentype::TK_Srecovery:
    case verilog_tokentype::TK_Sremoval:
    case verilog_tokentype::TK_Ssetup:
    case verilog_tokentype::TK_Sskew:
    case verilog_tokentype::TK_Stimeskew:
    case verilog_tokentype::TK_Swidth:
      // KeywordIdentifier tokens
    case verilog_tokentype::TK_access:
    case verilog_tokentype::TK_exclude:
    case verilog_tokentype::TK_flow:
    case verilog_tokentype::TK_from:
    case verilog_tokentype::TK_discrete:
    case verilog_tokentype::TK_sample:
    case verilog_tokentype::TK_infinite:
    case verilog_tokentype::TK_continuous:
      return true;
    default:
      break;
  }
  return false;
}

}  // namespace verilog
