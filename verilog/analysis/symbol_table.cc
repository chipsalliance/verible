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

#include "verilog/analysis/symbol_table.h"

#include "absl/status/status.h"
#include "absl/strings/str_join.h"
#include "common/text/concrete_syntax_leaf.h"
#include "common/text/concrete_syntax_tree.h"
#include "common/text/visitors.h"
#include "common/util/iterator_adaptors.h"
#include "common/util/value_saver.h"
#include "verilog/CST/module.h"
#include "verilog/CST/verilog_nonterminals.h"
#include "verilog/parser/verilog_token_enum.h"

namespace verilog {

namespace {

using verible::SyntaxTreeLeaf;
using verible::SyntaxTreeNode;
using verible::ValueSaver;

class SymbolTableBuilder : public verible::SymbolVisitor {
 public:
  SymbolTableBuilder(const VerilogSourceFile& source, SymbolTable* symbol_table)
      : source_(&source),
        symbol_table_(symbol_table),
        current_scope_(&symbol_table_->MutableRoot()) {}

  std::vector<absl::Status> TakeDiagnostics() {
    return std::move(diagnostics_);
  }

 private:  // methods
  void Visit(const SyntaxTreeNode& node) final {
    const auto tag = static_cast<NodeEnum>(node.Tag().tag);
    switch (tag) {
      case NodeEnum::kModuleDeclaration:
        DeclareModule(node);
        break;
      case NodeEnum::kDataType:
      default:
        Descend(node);
        break;
    }
  }

  void Descend(const SyntaxTreeNode& node, SymbolTableNode& with_scope) {
    const ValueSaver<SymbolTableNode*> save_scope(&current_scope_, &with_scope);
    // TODO: same for current_binding_scope_.
    Descend(node);
  }

  void Descend(const SyntaxTreeNode& node) {
    for (const auto& child : node.children()) {
      if (child != nullptr) child->Accept(this);
    }
  }

  void Visit(const SyntaxTreeLeaf& leaf) final {}

  void DeclareModule(const SyntaxTreeNode& module) {
    const auto module_name = GetModuleName(module).get().text();
    const auto p = current_scope_->TryEmplace(
        module_name, SymbolInfo{.type = SymbolType::kModule,
                                .file_origin = source_,
                                .syntax_origin = &module});
    if (!p.second) {
      DiagnoseSymbolAlreadyExists(module_name);
    }
    SymbolTableNode& enter_scope(p.first->second);
    Descend(module, enter_scope);
  }

  void DiagnoseSymbolAlreadyExists(absl::string_view name) {
    diagnostics_.push_back(absl::AlreadyExistsError(
        absl::StrCat("Symbol \"", name, "\" is already defined in the ",
                     CurrentScopeFullPath(), " scope.")));
  }

  std::string CurrentScopeFullPath() const {
    // Root SymbolTableNode has no key, but we identify it as "$root"
    constexpr absl::string_view root("$root");
    std::vector<absl::string_view> reverse_scope_path_components;
    reverse_scope_path_components.reserve(current_scope_->NumAncestors() + 1);
    const SymbolTableNode* node = current_scope_;
    while (node->Parent() != nullptr) {
      reverse_scope_path_components.push_back(*current_scope_->Key());
      node = node->Parent();
    }
    reverse_scope_path_components.push_back(root);
    // concatenate in a single StrJoin/StrCat for efficiency
    return absl::StrJoin(verible::reversed_view(reverse_scope_path_components),
                         "::");
  }

 private:  // data
  // Points to the source file that is the origin of symbols.
  // This changes when opening preprocess-included files.
  // TODO(fangism): maintain a vector/stack of these for richer diagnostics
  const VerilogSourceFile* source_;

  // The symbol table to build.
  SymbolTable* symbol_table_;

  // never nullptr
  SymbolTableNode* current_scope_;

  // Collection of findings that might be considered compiler/tool errors in a
  // real toolchain.  For example: attempt to redefine symbol.
  std::vector<absl::Status> diagnostics_;
};

}  // namespace

std::vector<absl::Status> BuildSymbolTable(const VerilogSourceFile& source,
                                           SymbolTable* symbol_table) {
  const auto* text_structure = source.GetTextStructure();
  if (text_structure == nullptr) return std::vector<absl::Status>();
  const auto& syntax_tree = text_structure->SyntaxTree();
  if (syntax_tree == nullptr) return std::vector<absl::Status>();
  SymbolTableBuilder builder(source, symbol_table);
  syntax_tree->Accept(&builder);
  return builder.TakeDiagnostics();
}

}  // namespace verilog
