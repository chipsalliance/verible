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

#include "common/text/tree_context_visitor.h"
#include "common/util/casts.h"
#include "verilog/CST/seq_block.h"
#include "verilog/CST/verilog_nonterminals.h"
#include "verilog/analysis/symbol_table.h"
#include "verilog/parser/verilog_token_enum.h"

namespace verilog {

namespace {

using verible::Symbol;
using verible::SyntaxTreeLeaf;
using verible::SyntaxTreeNode;
using verible::TreeContextVisitor;

// Finds names/labels of named blocks
class FindBeginLabel : public TreeContextVisitor {
 public:
  explicit FindBeginLabel() = default;

  absl::string_view LabelSearch(const verible::ConcreteSyntaxTree &tree,
                                absl::string_view substring, NodeEnum endtag,
                                NodeEnum begintag) {
    this->substring = substring;
    this->begintag = begintag;
    this->endtag = endtag;
    substring_found = false;
    finished = false;
    tree->Accept(this);
    return label;
  }

 private:
  void Visit(const SyntaxTreeLeaf &leaf) override {
    if (IsStringViewContained(leaf.get().text(), substring)) {
      substring_found = true;
    }
  }
  void Visit(const SyntaxTreeNode &node) override {
    if (finished) return;
    const std::unique_ptr<Symbol> *lastbegin = nullptr;
    for (const std::unique_ptr<Symbol> &child : node.children()) {
      if (!child) continue;
      if (child->Kind() == verible::SymbolKind::kLeaf &&
          node.Tag().tag == static_cast<int>(endtag)) {
        Visit(verible::down_cast<const SyntaxTreeLeaf &>(*child));
        if (substring_found) return;
      } else if (child->Tag().tag == static_cast<int>(begintag)) {
        lastbegin = &child;
      } else if (child->Kind() == verible::SymbolKind::kNode) {
        Visit(verible::down_cast<const SyntaxTreeNode &>(*child));
        if (!label.empty()) return;
        if (substring_found) {
          if (!lastbegin) {
            finished = true;
            return;
          }
          const verible::TokenInfo *info = GetBeginLabelTokenInfo(**lastbegin);
          finished = true;
          if (!info) return;
          label = info->text();
          return;
        }
      }
      if (finished) return;
    }
  }
  absl::string_view substring;
  NodeEnum endtag;
  NodeEnum begintag;
  absl::string_view label;
  bool substring_found;
  bool finished;
};

class HoverBuilder {
 public:
  HoverBuilder(SymbolTableHandler *symbol_table_handler,
               const BufferTrackerContainer &tracker_container,
               const verible::lsp::HoverParams &params)
      : symbol_table_handler(symbol_table_handler),
        tracker_container(tracker_container),
        params(params) {}

  verible::lsp::Hover Build() {
    std::optional<verible::TokenInfo> token =
        symbol_table_handler->GetTokenAtTextDocumentPosition(params,
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
    const verilog::BufferTracker *tracker =
        tracker_container.FindBufferTrackerOrNull(params.textDocument.uri);
    if (!tracker) return;
    std::shared_ptr<const ParsedBuffer> parsedbuffer = tracker->current();
    if (!parsedbuffer) return;
    const verible::ConcreteSyntaxTree &tree =
        parsedbuffer->parser().SyntaxTree();
    if (!tree) return;
    FindBeginLabel search;
    absl::string_view label = search.LabelSearch(
        tree, token.text(), NodeEnum::kEnd, NodeEnum::kBegin);
    if (label.empty()) return;
    response->contents.value = "### End of block\n\n";
    absl::StrAppend(&response->contents.value, "---\n\nName: ", label,
                    "\n\n---");
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
  const verible::lsp::HoverParams &params;
};

}  // namespace

verible::lsp::Hover CreateHoverInformation(
    SymbolTableHandler *symbol_table_handler,
    const BufferTrackerContainer &tracker, const verible::lsp::HoverParams &p) {
  HoverBuilder builder(symbol_table_handler, tracker, p);
  return builder.Build();
}

}  // namespace verilog
