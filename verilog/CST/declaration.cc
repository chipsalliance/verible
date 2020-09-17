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

#include "verilog/CST/declaration.h"

#include <map>
#include <memory>
#include <utility>

#include "common/text/concrete_syntax_leaf.h"
#include "common/text/concrete_syntax_tree.h"
#include "common/text/constants.h"
#include "common/text/symbol.h"
#include "common/text/token_info.h"
#include "common/text/tree_utils.h"
#include "common/util/container_util.h"
#include "verilog/CST/identifier.h"
#include "verilog/CST/verilog_matchers.h"
#include "verilog/CST/verilog_nonterminals.h"
#include "verilog/parser/verilog_token_enum.h"

namespace verilog {
using verible::Symbol;
using verible::SymbolPtr;
using verible::SyntaxTreeNode;
using verible::container::FindWithDefault;

SymbolPtr RepackReturnTypeId(SymbolPtr type_id_tuple) {
  auto& node = CheckSymbolAsNode(*type_id_tuple,
                                 NodeEnum::kDataTypeImplicitBasicIdDimensions);
  return verible::MakeNode(std::move(node[0]) /* type */,
                           std::move(node[1]) /* id */);
  // Discard unpacked dimensions node[2], should be nullptr, and not
  // syntactically valid.
}

NodeEnum DeclarationKeywordToNodeEnum(const Symbol& symbol) {
  static const auto* node_map = new std::map<verilog_tokentype, NodeEnum>{
      {TK_module, NodeEnum::kModuleDeclaration},
      {TK_macromodule, NodeEnum::kMacroModuleDeclaration},
      {TK_program, NodeEnum::kProgramDeclaration},
      {TK_interface, NodeEnum::kInterfaceDeclaration},
  };
  return FindWithDefault(
      *node_map,
      verilog_tokentype(verible::SymbolCastToLeaf(symbol).get().token_enum()),
      NodeEnum(verible::kUntagged));
}

std::vector<verible::TreeSearchMatch> FindAllDataDeclarations(
    const Symbol& root) {
  return SearchSyntaxTree(root, NodekDataDeclaration());
}

std::vector<verible::TreeSearchMatch> FindAllNetVariables(const Symbol& root) {
  return SearchSyntaxTree(root, NodekNetVariable());
}

std::vector<verible::TreeSearchMatch> FindAllRegisterVariables(
    const Symbol& root) {
  return SearchSyntaxTree(root, NodekRegisterVariable());
}

std::vector<verible::TreeSearchMatch> FindAllGateInstances(const Symbol& root) {
  return SearchSyntaxTree(root, NodekGateInstance());
}

std::vector<verible::TreeSearchMatch> FindAllVariableDeclarationAssign(
    const verible::Symbol& root) {
  return SearchSyntaxTree(root, NodekVariableDeclarationAssignment());
}

// Don't want to expose kInstantiationBase because it is an artificial grouping.
static const SyntaxTreeNode& GetInstantiationBaseFromDataDeclaration(
    const Symbol& data_declaration) {
  return GetSubtreeAsNode(data_declaration, NodeEnum::kDataDeclaration, 1,
                          NodeEnum::kInstantiationBase);
}

const SyntaxTreeNode* GetQualifiersOfDataDeclaration(
    const Symbol& data_declaration) {
  const auto* quals =
      GetSubtreeAsSymbol(data_declaration, NodeEnum::kDataDeclaration, 0);
  return verible::CheckOptionalSymbolAsNode(quals, NodeEnum::kQualifierList);
}

const SyntaxTreeNode& GetTypeOfDataDeclaration(const Symbol& data_declaration) {
  return GetSubtreeAsNode(
      GetInstantiationBaseFromDataDeclaration(data_declaration),
      NodeEnum::kInstantiationBase, 0);
}

const SyntaxTreeNode& GetInstanceListFromDataDeclaration(
    const Symbol& data_declaration) {
  return GetSubtreeAsNode(
      GetInstantiationBaseFromDataDeclaration(data_declaration),
      NodeEnum::kInstantiationBase, 1);
}

const SyntaxTreeNode& GetReferenceCallBaseFromInstantiationType(
    const Symbol& instantiation_type) {
  return GetSubtreeAsNode(instantiation_type, NodeEnum::kInstantiationType, 0);
}

const SyntaxTreeNode& GetReferenceFromReferenceCallBase(
    const Symbol& reference_call_base) {
  return GetSubtreeAsNode(reference_call_base, NodeEnum::kReferenceCallBase, 0);
}

const SyntaxTreeNode& GetLocalRootFromReference(const Symbol& reference) {
  return GetSubtreeAsNode(reference, NodeEnum::kReference, 0);
}

const SyntaxTreeNode& GetUnqualifiedIdFromLocalRoot(const Symbol& local_root) {
  return GetSubtreeAsNode(local_root, NodeEnum::kLocalRoot, 0);
}

const verible::TokenInfo& GetTypeTokenInfoFromDataDeclaration(
    const verible::Symbol& data_declaration) {
  const SyntaxTreeNode& instantiation_type =
      GetTypeOfDataDeclaration(data_declaration);
  const SyntaxTreeNode& reference_call_base =
      GetReferenceCallBaseFromInstantiationType(instantiation_type);
  const SyntaxTreeNode& reference =
      GetReferenceFromReferenceCallBase(reference_call_base);
  const SyntaxTreeNode& local_root = GetLocalRootFromReference(reference);
  const SyntaxTreeNode& unqualified_id =
      GetUnqualifiedIdFromLocalRoot(local_root);
  const verible::SyntaxTreeLeaf* instance_symbol_identifier =
      GetIdentifier(unqualified_id);
  return instance_symbol_identifier->get();
}

const verible::TokenInfo& GetModuleInstanceNameTokenInfoFromGateInstance(
    const verible::Symbol& gate_instance) {
  const verible::SyntaxTreeLeaf& instance_name =
      GetSubtreeAsLeaf(gate_instance, NodeEnum::kGateInstance, 0);
  return instance_name.get();
}

const verible::TokenInfo& GetInstanceNameTokenInfoFromRegisterVariable(
    const verible::Symbol& regiseter_variable) {
  const verible::SyntaxTreeLeaf& instance_name =
      GetSubtreeAsLeaf(regiseter_variable, NodeEnum::kRegisterVariable, 0);
  return instance_name.get();
}

const verible::SyntaxTreeNode& GetParenGroupFromModuleInstantiation(
    const verible::Symbol& gate_instance) {
  return GetSubtreeAsNode(gate_instance, NodeEnum::kGateInstance, 2,
                          NodeEnum::kParenGroup);
}

const verible::SyntaxTreeLeaf& GetUnqualifiedIdFromVariableDeclaratioAssign(
    const verible::Symbol& variable_declaration_assign) {
  return *AutoUnwrapIdentifier(*ABSL_DIE_IF_NULL(
      GetSubtreeAsSymbol(variable_declaration_assign,
                         NodeEnum::kVariableDeclarationAssignment, 0)));
}

}  // namespace verilog
