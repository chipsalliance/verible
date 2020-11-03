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

// This unit provides helper functions that pertain to SystemVerilog
// module declaration nodes in the parser-generated concrete syntax tree.

#include "verilog/CST/macro.h"

#include "common/text/tree_utils.h"
#include "verilog/CST/verilog_matchers.h"
#include "verilog/CST/verilog_nonterminals.h"

namespace verilog {

using verible::Symbol;
using verible::SyntaxTreeNode;
using verible::TokenInfo;

std::vector<verible::TreeSearchMatch> FindAllMacroDefinitions(
    const verible::Symbol& root) {
  return SearchSyntaxTree(root, NodekPreprocessorDefine());
}

std::vector<verible::TreeSearchMatch> FindAllPreprocessorInclude(
    const verible::Symbol& root) {
  return SearchSyntaxTree(root, NodekPreprocessorInclude());
}

std::vector<verible::TreeSearchMatch> FindAllMacroCalls(const Symbol& root) {
  return SearchSyntaxTree(root, NodekMacroCall());
}

std::vector<verible::TreeSearchMatch> FindAllMacroGenericItems(
    const Symbol& root) {
  return SearchSyntaxTree(root, NodekMacroGenericItem());
}

std::vector<verible::TreeSearchMatch> FindAllMacroDefinitionsArgs(
    const verible::Symbol& macro_definition) {
  return SearchSyntaxTree(macro_definition, NodekMacroFormalArg());
}

const TokenInfo& GetMacroCallId(const Symbol& s) {
  return GetSubtreeAsLeaf(s, NodeEnum::kMacroCall, 0).get();
}

const TokenInfo& GetMacroGenericItemId(const Symbol& s) {
  return GetSubtreeAsLeaf(s, NodeEnum::kMacroGenericItem, 0).get();
}

const SyntaxTreeNode& GetMacroCallParenGroup(const Symbol& s) {
  return GetSubtreeAsNode(s, NodeEnum::kMacroCall, 1, NodeEnum::kParenGroup);
}

const SyntaxTreeNode& GetMacroCallArgs(const Symbol& s) {
  // See structure of (CST) MakeParenGroup().
  return GetSubtreeAsNode(GetMacroCallParenGroup(s), NodeEnum::kParenGroup, 1,
                          NodeEnum::kMacroArgList);
}

bool MacroCallArgsIsEmpty(const SyntaxTreeNode& args) {
  const auto& sub = CheckNodeEnum(args, NodeEnum::kMacroArgList).children();
  // Empty macro args are always constructed with one nullptr child in
  // the semantic actions in verilog.y.
  if (sub.size() != 1) return false;
  return sub.front() == nullptr;
}

const verible::SyntaxTreeLeaf& GetMacroName(
    const verible::Symbol& preprocessor_define) {
  return GetSubtreeAsLeaf(preprocessor_define, NodeEnum::kPreprocessorDefine,
                          1);
}

const verible::SyntaxTreeLeaf& GetMacroArgName(
    const verible::Symbol& macro_formal_arg) {
  return GetSubtreeAsLeaf(macro_formal_arg, NodeEnum::kMacroFormalArg, 0);
}

const verible::SyntaxTreeLeaf* GetFileFromPreprocessorInclude(
    const verible::Symbol& preprocessor_include) {
  const verible::Symbol& included_filename =
      *ABSL_DIE_IF_NULL(verible::GetSubtreeAsSymbol(
          preprocessor_include, NodeEnum::kPreprocessorInclude, 1));
  // Terminate if this isn't a string literal.
  if (included_filename.Tag().tag != verilog_tokentype::TK_StringLiteral) {
    return nullptr;
  }

  return &verible::SymbolCastToLeaf(included_filename);
}

}  // namespace verilog
