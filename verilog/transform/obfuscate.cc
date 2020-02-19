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

#include "verilog/transform/obfuscate.h"

#include <iostream>

#include "absl/strings/string_view.h"
#include "common/strings/obfuscator.h"
#include "common/text/token_info.h"
#include "verilog/parser/verilog_lexer.h"
#include "verilog/parser/verilog_token_enum.h"

namespace verilog {

using verible::IdentifierObfuscator;

// TODO(fangism): single-char identifiers don't need to be obfuscated.
// or use a shuffle/permutation to guarantee collision-free reversibility.

void ObfuscateVerilogCode(absl::string_view content, std::ostream* output,
                          IdentifierObfuscator* subst) {
  verilog::VerilogLexer lexer(content);
  while (true) {
    const verible::TokenInfo& token(lexer.DoNextToken());
    if (token.isEOF()) break;
    switch (token.token_enum) {
      case verilog_tokentype::SymbolIdentifier:
      case verilog_tokentype::PP_Identifier:
        *output << (*subst)(token.text);
        break;
        // The following identifier types start with a special character that
        // needs to be preserved.
      case verilog_tokentype::SystemTFIdentifier:
      case verilog_tokentype::MacroIdentifier:
      case verilog_tokentype::MacroCallId:
        // TODO(fangism): verilog_tokentype::EscapedIdentifier
        *output << token.text[0] << (*subst)(token.text.substr(1));
        break;
      // The following tokens are un-lexed, so they need to be lexed
      // recursively.
      case verilog_tokentype::MacroArg:
      case verilog_tokentype::PP_define_body:
        ObfuscateVerilogCode(token.text, output, subst);
        break;
      default:
        *output << token.text;
    }
  }
}

}  // namespace verilog
