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

#include "verible/verilog/tools/ls/document-symbol-filler.h"

#include <string>

#include "nlohmann/json.hpp"
#include "verible/common/lsp/lsp-protocol-enums.h"
#include "verible/common/lsp/lsp-protocol.h"
#include "verible/common/text/text-structure.h"
#include "verible/common/text/token-info.h"
#include "verible/common/text/tree-utils.h"
#include "verible/common/util/value-saver.h"
#include "verible/verilog/CST/class.h"
#include "verible/verilog/CST/functions.h"
#include "verible/verilog/CST/module.h"
#include "verible/verilog/CST/package.h"
#include "verible/verilog/CST/seq-block.h"
#include "verible/verilog/CST/verilog-nonterminals.h"
#include "verible/verilog/tools/ls/lsp-conversion.h"

// Magic value to hint that we have to fill out the start range.
static constexpr int kUninitializedStartLine = -1;

// verible::lsp::SymbolKind::Module is just shown as {} namespace symbol
// in vscode. 'Method' looks slightly nicer as little block. So emit a
// symbol in the document tree that has the nicer look.
// TODO(hzeller): This is hacky. We already have a mapping for kate. Looks
//  like it is a good idea to have some re-mapping per editor (which we then
//  identify via the initialization script. So we pass in a mapping instead
//  of a flag.
//  Well _ideally_ the editors would just show proper icons.
static constexpr verible::lsp::SymbolKind kVSCodeModule =
    verible::lsp::SymbolKind::kMethod;

namespace verilog {
DocumentSymbolFiller::DocumentSymbolFiller(
    bool kate_workaround, bool include_variables,
    const verible::TextStructureView &text,
    verible::lsp::DocumentSymbol *toplevel)
    : kModuleSymbolKind(kate_workaround ? verible::lsp::SymbolKind::kMethod
                                        : kVSCodeModule),
      kBlockSymbolKind(kate_workaround ? verible::lsp::SymbolKind::kClass
                                       : verible::lsp::SymbolKind::kNamespace),
      text_view_(text),
      current_symbol_(toplevel) {
  this->include_variables = include_variables;
  toplevel->range.start = {.line = 0, .character = 0};
}

void DocumentSymbolFiller::Visit(const verible::SyntaxTreeLeaf &leaf) {
  verible::lsp::Range range = RangeFromLeaf(leaf);
  if (current_symbol_->range.start.line == kUninitializedStartLine) {
    // We're the first concrete token with a position within our parent,
    // set the start position.
    current_symbol_->range.start = range.start;
  }
  // Update the end position with every token we see. The last one wins.
  current_symbol_->range.end = range.end;
}

void DocumentSymbolFiller::Visit(const verible::SyntaxTreeNode &node) {
  verible::lsp::DocumentSymbol *parent = current_symbol_;

  // These things can probably be done easier with Matchers.
  verible::lsp::DocumentSymbol node_symbol;
  node_symbol.range.start.line = kUninitializedStartLine;
  bool is_visible_node = false;
  switch (static_cast<verilog::NodeEnum>(node.Tag().tag)) {
    case verilog::NodeEnum::kModuleDeclaration: {
      const auto *name_leaf = verilog::GetModuleName(node);
      if (name_leaf) {
        is_visible_node = true;
        node_symbol.kind = kModuleSymbolKind;
        node_symbol.selectionRange = RangeFromLeaf(*name_leaf);
        node_symbol.name = std::string(name_leaf->get().text());
      }
      break;
    }

    case verilog::NodeEnum::kSeqBlock:
    case verilog::NodeEnum::kGenerateBlock:
      if (!node.empty()) {
        const auto &begin = node.front().get();
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
      const auto *class_name_leaf = verilog::GetClassName(node);
      if (class_name_leaf) {
        is_visible_node = true;
        node_symbol.kind = verible::lsp::SymbolKind::kClass;
        node_symbol.selectionRange = RangeFromToken(class_name_leaf->get());
        node_symbol.name = std::string(class_name_leaf->get().text());
      }
      break;
    }
    case verilog::NodeEnum::kRegisterVariable: {
      const auto *variable_name =
          GetSubtreeAsLeaf(node, NodeEnum::kRegisterVariable, 0);
      if (variable_name && this->include_variables) {
        is_visible_node = true;
        node_symbol.kind = verible::lsp::SymbolKind::kVariable;
        node_symbol.selectionRange = RangeFromToken(variable_name->get());
        node_symbol.name = std::string(variable_name->get().text());
      }
      break;
    }
    case verilog::NodeEnum::kGateInstance: {
      const auto *variable_name =
          GetSubtreeAsLeaf(node, NodeEnum::kGateInstance, 0);
      if (variable_name && this->include_variables) {
        is_visible_node = true;
        node_symbol.kind = verible::lsp::SymbolKind::kVariable;
        node_symbol.selectionRange = RangeFromToken(variable_name->get());
        node_symbol.name = std::string(variable_name->get().text());
      }
      break;
    }
    case verilog::NodeEnum::kPackageDeclaration: {
      const auto *package_name = verilog::GetPackageNameToken(node);
      if (package_name) {
        is_visible_node = true;
        node_symbol.kind = verible::lsp::SymbolKind::kPackage;
        node_symbol.selectionRange = RangeFromToken(*package_name);
        node_symbol.name = std::string(package_name->text());
      }
      break;
    }

    case verilog::NodeEnum::kFunctionDeclaration: {
      const auto &function_name = verilog::GetFunctionName(node);
      if (function_name) {
        is_visible_node = true;
        node_symbol.kind = verible::lsp::SymbolKind::kFunction;
        node_symbol.selectionRange = RangeFromToken(function_name->get());
        node_symbol.name = std::string(function_name->get().text());
      }
      break;
    }

    default:
      is_visible_node = false;
      break;
  }

  // Independent of visible or not, we always descend to our children.
  if (is_visible_node) {
    const verible::ValueSaver<verible::lsp::DocumentSymbol *> value_saver(
        &current_symbol_, &node_symbol);
    for (const auto &child : node.children()) {
      if (child) child->Accept(this);
    }
    // Update our parent with what we found
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

verible::lsp::Range DocumentSymbolFiller::RangeFromLeaf(
    const verible::SyntaxTreeLeaf &leaf) const {
  return RangeFromToken(leaf.get());
}

verible::lsp::Range DocumentSymbolFiller::RangeFromToken(
    const verible::TokenInfo &token) const {
  return RangeFromLineColumn(text_view_.GetRangeForToken(token));
}
}  // namespace verilog
