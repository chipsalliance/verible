// Copyright 2021 The Verible Authors.
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

#ifndef VERILOG_TOOLS_LS_DOCUMENT_SYMBOL_FILLER_H
#define VERILOG_TOOLS_LS_DOCUMENT_SYMBOL_FILLER_H

#include "common/lsp/lsp-protocol-enums.h"
#include "common/lsp/lsp-protocol.h"
#include "common/strings/line_column_map.h"
#include "common/util/value_saver.h"
#include "verilog/CST/class.h"
#include "verilog/CST/functions.h"
#include "verilog/CST/module.h"
#include "verilog/CST/package.h"
#include "verilog/CST/seq_block.h"

namespace verilog {
class DocumentSymbolFiller : public verible::SymbolVisitor {
 public:
  // Magic value to hint that we have to fill out the start range.
  static constexpr int kUninitializedStartLine = -1;

  DocumentSymbolFiller(bool kate_workaround,
                       const verible::TextStructureView &text,
                       verible::lsp::DocumentSymbol *toplevel)
      : kModuleSymbolKind(kate_workaround ? verible::lsp::SymbolKind::Method
                                          : verible::lsp::SymbolKind::Module),
        kBlockSymbolKind(kate_workaround ? verible::lsp::SymbolKind::Class
                                         : verible::lsp::SymbolKind::Namespace),
        text_view_(text),
        current_symbol_(toplevel) {
    toplevel->range.start = {.line = 0, .character = 0};
  }

  verible::lsp::Range RangeFromLeaf(const verible::SyntaxTreeLeaf &leaf) const {
    return RangeFromToken(leaf.get());
  }
  verible::lsp::Range RangeFromToken(const verible::TokenInfo &token) const {
    verible::LineColumn start =
        text_view_.GetLineColAtOffset(token.left(text_view_.Contents()));
    verible::LineColumn end =
        text_view_.GetLineColAtOffset(token.right(text_view_.Contents()));
    return {.start = {.line = start.line, .character = start.column},
            .end = {.line = end.line, .character = end.column}};
  }

  void Visit(const verible::SyntaxTreeLeaf &leaf) final {
    verible::lsp::Range range = RangeFromLeaf(leaf);
    if (current_symbol_->range.start.line == kUninitializedStartLine) {
      // We're the first concrete token with a position within our parent,
      // set the start position.
      current_symbol_->range.start = range.start;
    }
    // Update the end position with every token we see. The last one wins.
    current_symbol_->range.end = range.end;
  }

  void Visit(const verible::SyntaxTreeNode &node) final {
    verible::lsp::DocumentSymbol *parent = current_symbol_;

    // These things can probably be done easier with Matchers.
    verible::lsp::DocumentSymbol node_symbol;
    node_symbol.range.start.line = kUninitializedStartLine;
    bool is_visible_node = false;
    switch (static_cast<verilog::NodeEnum>(node.Tag().tag)) {
      case verilog::NodeEnum::kModuleDeclaration: {
        is_visible_node = true;
        node_symbol.kind = kModuleSymbolKind;
        const auto &name_leaf = verilog::GetModuleName(node);
        node_symbol.selectionRange = RangeFromLeaf(name_leaf);
        node_symbol.name = std::string(name_leaf.get().text());
        break;
      }
      case verilog::NodeEnum::kSeqBlock:
      case verilog::NodeEnum::kGenerateBlock:
        if (!node.children().empty()) {
          const auto &begin = node.children().front().get();
          if (begin) {
            if (const auto *token = GetBeginLabelTokenInfo(*begin); token) {
              is_visible_node = true;
              node_symbol.kind = kBlockSymbolKind;
              node_symbol.selectionRange = RangeFromToken(*token);
              node_symbol.name = std::string(token->text());
            }
          }
        }
        break;
      case verilog::NodeEnum::kClassDeclaration: {
        const auto &class_name_leaf = verilog::GetClassName(node);
        is_visible_node = true;
        node_symbol.kind = verible::lsp::SymbolKind::Class;
        node_symbol.selectionRange = RangeFromToken(class_name_leaf.get());
        node_symbol.name = std::string(class_name_leaf.get().text());
      } break;

      case verilog::NodeEnum::kPackageDeclaration: {
        const auto &package_name = verilog::GetPackageNameToken(node);
        is_visible_node = true;
        node_symbol.kind = verible::lsp::SymbolKind::Package;
        node_symbol.selectionRange = RangeFromToken(package_name);
        node_symbol.name = std::string(package_name.text());
      } break;
      case verilog::NodeEnum::kFunctionDeclaration: {
        const auto &function_name = verilog::GetFunctionName(node);
        if (function_name) {
          is_visible_node = true;
          node_symbol.kind = verible::lsp::SymbolKind::Function;
          node_symbol.selectionRange = RangeFromToken(function_name->get());
          node_symbol.name = std::string(function_name->get().text());
        }
      } break;
      default:
        is_visible_node = false;
        break;
    }

    // Independent of visible or not, we always descent to our children.
    if (is_visible_node) {
      const verible::ValueSaver<verible::lsp::DocumentSymbol *> value_saver(
          &current_symbol_, &node_symbol);
      for (const auto &child : node.children()) {
        if (child) child->Accept(this);
      }
      if (parent->children == nullptr) {
        if (parent->range.start.line == kUninitializedStartLine) {
          parent->range.start = node_symbol.range.start;
        }
        parent->children = nlohmann::json::array();
        parent->has_children = true;
      }
      parent->children.push_back(node_symbol);
      parent->range.end = node_symbol.range.end;
    } else {
      for (const auto &child : node.children()) {
        if (child) child->Accept(this);
      }
    }
  }

 private:
  const int kModuleSymbolKind;  // Might be different if kate-workaround.
  const int kBlockSymbolKind;
  const verible::TextStructureView &text_view_;
  verible::lsp::DocumentSymbol *current_symbol_;
};
}  // namespace verilog
#endif  // VERILOG_TOOLS_LS_DOCUMENT_SYMBOL_FILLER_H
