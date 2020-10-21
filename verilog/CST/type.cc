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

#include "verilog/CST/type.h"

#include <vector>

#include "common/analysis/matcher/matcher.h"
#include "common/analysis/matcher/matcher_builders.h"
#include "common/analysis/syntax_tree_search.h"
#include "common/text/concrete_syntax_leaf.h"
#include "common/text/concrete_syntax_tree.h"
#include "common/text/symbol.h"
#include "common/text/tree_utils.h"
#include "verilog/CST/identifier.h"
#include "verilog/CST/verilog_matchers.h"  // pragma IWYU: keep

namespace verilog {

std::vector<verible::TreeSearchMatch> FindAllDataTypeDeclarations(
    const verible::Symbol& root) {
  return verible::SearchSyntaxTree(root, NodekDataType());
}

std::vector<verible::TreeSearchMatch> FindAllEnumNames(
    const verible::Symbol& root) {
  return verible::SearchSyntaxTree(root, NodekEnumName());
}

std::vector<verible::TreeSearchMatch> FindAllDataTypePrimitive(
    const verible::Symbol& root) {
  return verible::SearchSyntaxTree(root, NodekDataTypePrimitive());
}

std::vector<verible::TreeSearchMatch> FindAllTypeDeclarations(
    const verible::Symbol& root) {
  return verible::SearchSyntaxTree(root, NodekTypeDeclaration());
}

std::vector<verible::TreeSearchMatch> FindAllEnumTypes(
    const verible::Symbol& root) {
  return verible::SearchSyntaxTree(root, NodekEnumType());
}

std::vector<verible::TreeSearchMatch> FindAllStructTypes(
    const verible::Symbol& root) {
  return verible::SearchSyntaxTree(root, NodekStructType());
}

std::vector<verible::TreeSearchMatch> FindAllUnionTypes(
    const verible::Symbol& root) {
  return verible::SearchSyntaxTree(root, NodekUnionType());
}

std::vector<verible::TreeSearchMatch> FindAllInterfaceTypes(
    const verible::Symbol& root) {
  return verible::SearchSyntaxTree(root, NodekInterfaceType());
}

bool IsStorageTypeOfDataTypeSpecified(const verible::Symbol& symbol) {
  const auto* storage =
      verible::GetSubtreeAsSymbol(symbol, NodeEnum::kDataType, 0);
  return (storage != nullptr);
}

const verible::SyntaxTreeLeaf* GetIdentifierFromTypeDeclaration(
    const verible::Symbol& symbol) {
  // For enum, struct and union identifier is found at the same position
  const auto* identifier_symbol =
      verible::GetSubtreeAsSymbol(symbol, NodeEnum::kTypeDeclaration, 3);
  return AutoUnwrapIdentifier(*ABSL_DIE_IF_NULL(identifier_symbol));
}

const verible::SyntaxTreeNode& GetPackedDimensionFromDataType(
    const verible::Symbol& data_type) {
  if (NodeEnum(data_type.Tag().tag) == NodeEnum::kDataType) {
    return verible::GetSubtreeAsNode(data_type, NodeEnum::kDataType, 1,
                                     NodeEnum::kPackedDimensions);
  }

  const verible::SyntaxTreeLeaf& leaf =
      verible::GetSubtreeAsLeaf(data_type, NodeEnum::kDataTypePrimitive, 0);
  if (leaf.get().token_enum() == verilog_tokentype::TK_string) {
    return verible::GetSubtreeAsNode(data_type, NodeEnum::kDataTypePrimitive, 1,
                                     NodeEnum::kPackedDimensions);
  }

  return verible::GetSubtreeAsNode(data_type, NodeEnum::kDataTypePrimitive, 2,
                                   NodeEnum::kPackedDimensions);
}

const verible::SyntaxTreeNode& GetReferenceCallBaseFromInstantiationType(
    const verible::Symbol& instantiation_type) {
  return verible::GetSubtreeAsNode(instantiation_type,
                                   NodeEnum::kInstantiationType, 0);
}

const verible::SyntaxTreeNode& GetReferenceFromReferenceCallBase(
    const verible::Symbol& reference_call_base) {
  return verible::GetSubtreeAsNode(reference_call_base,
                                   NodeEnum::kReferenceCallBase, 0);
}

const verible::SyntaxTreeNode& GetLocalRootFromReference(
    const verible::Symbol& reference) {
  return verible::GetSubtreeAsNode(reference, NodeEnum::kReference, 0);
}

const verible::SyntaxTreeNode& GetIdentifiersFromLocalRoot(
    const verible::Symbol& local_root) {
  return verible::GetSubtreeAsNode(local_root, NodeEnum::kLocalRoot, 0);
}

const verible::SyntaxTreeNode& GetUnqualifiedIdFromReferenceCallBase(
    const verible::Symbol& reference_call_base) {
  const verible::SyntaxTreeNode& reference =
      GetReferenceFromReferenceCallBase(reference_call_base);
  const verible::SyntaxTreeNode& local_root =
      GetLocalRootFromReference(reference);
  return GetIdentifiersFromLocalRoot(local_root);
}

const verible::SyntaxTreeNode& GetUnqualifiedIdFromInstantiationType(
    const verible::Symbol& instantiation_type) {
  const verible::SyntaxTreeNode& reference_call_base =
      GetReferenceCallBaseFromInstantiationType(instantiation_type);
  return GetUnqualifiedIdFromReferenceCallBase(reference_call_base);
}

const verible::SyntaxTreeNode* GetParamListFromUnqualifiedId(
    const verible::Symbol& unqualified_id) {
  const verible::Symbol* param_list =
      verible::GetSubtreeAsSymbol(unqualified_id, NodeEnum::kUnqualifiedId, 1);
  return verible::CheckOptionalSymbolAsNode(param_list,
                                            NodeEnum::kActualParameterList);
}

const verible::SyntaxTreeNode* GetParamListFromInstantiationType(
    const verible::Symbol& instantiation_type) {
  const verible::SyntaxTreeNode& unqualified_id =
      GetUnqualifiedIdFromInstantiationType(instantiation_type);
  return GetParamListFromUnqualifiedId(unqualified_id);
}

const verible::SyntaxTreeLeaf& GetSymbolIdentifierFromEnumName(
    const verible::Symbol& enum_name) {
  return verible::GetSubtreeAsLeaf(enum_name, NodeEnum::kEnumName, 0);
}

}  // namespace verilog
