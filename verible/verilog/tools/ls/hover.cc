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

#include "verible/verilog/tools/ls/hover.h"

#include <memory>
#include <optional>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "verible/common/lsp/lsp-protocol.h"
#include "verible/common/text/concrete-syntax-leaf.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/token-info.h"
#include "verible/common/text/tree-context-visitor.h"
#include "verible/common/text/tree-utils.h"
#include "verible/common/util/casts.h"
#include "verible/common/util/range.h"
#include "verible/verilog/CST/seq-block.h"
#include "verible/verilog/CST/verilog-nonterminals.h"
#include "verible/verilog/analysis/symbol-table.h"
#include "verible/verilog/parser/verilog-token-enum.h"
#include "verible/verilog/tools/ls/lsp-parse-buffer.h"
#include "verible/verilog/tools/ls/symbol-table-handler.h"

namespace verilog {

namespace {

using verible::Symbol;
using verible::SyntaxTreeLeaf;
using verible::SyntaxTreeNode;
using verible::TreeContextVisitor;

// Finds names/labels of named blocks
class FindBeginLabel : public TreeContextVisitor {
 public:
  // Performs search of the label for end entry, based on its location in
  // string and tags
  absl::string_view LabelSearch(const verible::ConcreteSyntaxTree &tree,
                                absl::string_view substring, NodeEnum endtag,
                                NodeEnum begintag) {
    substring_ = substring;
    begintag_ = begintag;
    endtag_ = endtag;
    substring_found_ = false;
    finished_ = false;
    tree->Accept(this);
    return label_;
  }

 private:
  void Visit(const SyntaxTreeLeaf &leaf) final {
    if (verible::IsSubRange(leaf.get().text(), substring_)) {
      substring_found_ = true;
    }
  }

  void Visit(const SyntaxTreeNode &node) final {
    if (finished_) return;
    const std::unique_ptr<Symbol> *lastbegin = nullptr;
    for (const std::unique_ptr<Symbol> &child : node.children()) {
      if (!child) continue;
      if (child->Kind() == verible::SymbolKind::kLeaf &&
          node.Tag().tag == static_cast<int>(endtag_)) {
        Visit(verible::down_cast<const SyntaxTreeLeaf &>(*child));
        if (substring_found_) return;
      } else if (child->Tag().tag == static_cast<int>(begintag_)) {
        lastbegin = &child;
      } else if (child->Kind() == verible::SymbolKind::kNode) {
        Visit(verible::down_cast<const SyntaxTreeNode &>(*child));
        if (!label_.empty()) return;
        if (substring_found_) {
          if (!lastbegin) {
            finished_ = true;
            return;
          }
          const verible::TokenInfo *info = GetBeginLabelTokenInfo(**lastbegin);
          finished_ = true;
          if (!info) return;
          label_ = info->text();
          return;
        }
      }
      if (finished_) return;
    }
  }

  absl::string_view substring_;
  NodeEnum endtag_;
  NodeEnum begintag_;
  absl::string_view label_;
  bool substring_found_;
  bool finished_;
};

// Constructs a Hover message for the given location
class HoverBuilder {
 public:
  HoverBuilder(SymbolTableHandler *symbol_table_handler,
               const BufferTrackerContainer &tracker_container,
               const verible::lsp::HoverParams &params)
      : symbol_table_handler_(symbol_table_handler),
        tracker_container_(tracker_container),
        params_(params) {}

  verible::lsp::Hover Build() {
    std::optional<verible::TokenInfo> token =
        symbol_table_handler_->GetTokenAtTextDocumentPosition(
            params_, tracker_container_);
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
        tracker_container_.FindBufferTrackerOrNull(params_.textDocument.uri);
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
        symbol_table_handler_->FindDefinitionNode(symbol);
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

  SymbolTableHandler *symbol_table_handler_;
  const BufferTrackerContainer &tracker_container_;
  const verible::lsp::HoverParams &params_;
};

}  // namespace

verible::lsp::Hover CreateHoverInformation(
    SymbolTableHandler *symbol_table_handler,
    const BufferTrackerContainer &tracker, const verible::lsp::HoverParams &p) {
  HoverBuilder builder(symbol_table_handler, tracker, p);
  return builder.Build();
}

}  // namespace verilog
