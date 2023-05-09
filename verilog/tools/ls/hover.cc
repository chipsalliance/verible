// Copyright 2023 The Verible Authors.
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
//

#include "verilog/tools/ls/hover.h"

#include "verilog/analysis/symbol_table.h"
#include "verilog/parser/verilog_token_enum.h"

namespace verilog {

namespace {

class HoverBuilder {
 public:
  HoverBuilder(SymbolTableHandler *symbol_table_handler,
               const BufferTrackerContainer &tracker_container)
      : symbol_table_handler(symbol_table_handler),
        tracker_container(tracker_container) {}

  verible::lsp::Hover Build(const verible::lsp::HoverParams &p) {
    std::optional<verible::TokenInfo> token =
        symbol_table_handler->GetTokenAtTextDocumentPosition(p,
                                                             tracker_container);
    if (!token) return {};
    verible::lsp::Hover response;
    switch (token->token_enum()) {
      case verilog_tokentype::TK_end:
        HoverInfoEndToken(&response, *token);
        break;
      default:
        HoverInfoIdentifier(&response, *token);
    }
    return response;
  }

 private:
  void HoverInfoEndToken(verible::lsp::Hover *response,
                         const verible::TokenInfo &token) {
    // TODO implement walking up to begin
  }
  void HoverInfoIdentifier(verible::lsp::Hover *response,
                           const verible::TokenInfo &token) {
    absl::string_view symbol = token.text();
    const SymbolTableNode *node =
        symbol_table_handler->FindDefinitionNode(symbol);
    if (!node) return;
    const SymbolInfo &info = node->Value();
    response->contents.value = absl::StrCat(
        "### ", SymbolMetaTypeAsString(info.metatype), " ", symbol, "\n\n");
    if (!info.declared_type.syntax_origin && info.declared_type.implicit) {
      absl::StrAppend(&response->contents.value,
                      "---\n\nType: (implicit)\n\n---");
    } else if (info.declared_type.syntax_origin) {
      absl::StrAppend(
          &response->contents.value, "---\n\n", "Type: ",
          verible::StringSpanOfSymbol(*info.declared_type.syntax_origin),
          "\n\n---");
    }
  }
  SymbolTableHandler *symbol_table_handler;
  const BufferTrackerContainer &tracker_container;
};

}  // namespace

verible::lsp::Hover CreateHoverInformation(
    SymbolTableHandler *symbol_table_handler,
    const BufferTrackerContainer &tracker, const verible::lsp::HoverParams &p) {
  verible::lsp::Hover response;
  HoverBuilder builder(symbol_table_handler, tracker);
  return builder.Build(p);
}

}  // namespace verilog
