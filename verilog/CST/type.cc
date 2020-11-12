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
#include "common/util/logging.h"
#include "verilog/CST/identifier.h"
#include "verilog/CST/verilog_matchers.h"  // pragma IWYU: keep

namespace verilog {

using verible::Symbol;
using verible::SymbolPtr;
using verible::SyntaxTreeNode;

static SymbolPtr ReinterpretReferenceAsType(Symbol& reference) {
  SyntaxTreeNode& local_root(verible::GetSubtreeAsNode(
      reference, NodeEnum::kReference, 0, NodeEnum::kLocalRoot));
  auto& children(local_root.mutable_children());
  CHECK(!children.empty());
  // kLocalRoot has multiple constructions in verilog.y
  // TODO(fangism): reject the ones that are not plausible types as errors.
  return std::move(children[0]);
}

SymbolPtr ReinterpretReferenceCallBaseAsDataTypePackedDimensions(
    SymbolPtr& reference_call_base) {
  SyntaxTreeNode& base(verible::CheckSymbolAsNode(
      *ABSL_DIE_IF_NULL(reference_call_base), NodeEnum::kReferenceCallBase));
  auto& children(base.mutable_children());
  CHECK(!children.empty());

  Symbol& reference(*children.front());
  if (reference.Kind() != verible::SymbolKind::kNode ||
      !verible::SymbolCastToNode(reference).MatchesTag(NodeEnum::kReference)) {
    // function call -like syntax can never be interpreted as a type,
    // so return the whole subtree unmodified.
    return std::move(reference_call_base);
  }

  SymbolPtr packed_dimensions(
      verible::MakeTaggedNode(NodeEnum::kPackedDimensions));
  verible::SyntaxTreeNode& pdim_node(
      verible::SymbolCastToNode(*packed_dimensions));
  for (auto& child :
       verible::make_range(children.begin() + 1, children.end())) {
    // Each child could be a call-extension or an index (bit-select/slice).
    // Only [] indices are valid, any others are syntax errors.
    // We discard syntax errors for now, but in the future should retain these
    // error nodes for diagnostics.
    const auto tag = ABSL_DIE_IF_NULL(child)->Tag();
    if (tag.kind != verible::SymbolKind::kNode) continue;
    auto& node = verible::SymbolCastToNode(*child);
    if (node.MatchesTagAnyOf(
            {NodeEnum::kDimensionRange, NodeEnum::kDimensionScalar})) {
      pdim_node.AppendChild(std::move(child));
    }
    // TODO(fangism): instead of ignoring, retain non-tag-matched nodes as
    // syntax error nodes.
  }
  return MakeDataType(ReinterpretReferenceAsType(reference), packed_dimensions);
}

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

std::vector<verible::TreeSearchMatch> FindAllDataTypeImplicitIdDimensions(
    const verible::Symbol& root) {
  return verible::SearchSyntaxTree(root, NodekDataTypeImplicitIdDimensions());
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
  const auto* storage = GetBaseTypeFromDataType(symbol);
  return (storage != nullptr);
}

const verible::SyntaxTreeLeaf* GetIdentifierFromTypeDeclaration(
    const verible::Symbol& symbol) {
  // For enum, struct and union identifier is found at the same position
  const auto* identifier_symbol =
      verible::GetSubtreeAsSymbol(symbol, NodeEnum::kTypeDeclaration, 2);
  return AutoUnwrapIdentifier(*ABSL_DIE_IF_NULL(identifier_symbol));
}

const verible::Symbol* GetBaseTypeFromDataType(
    const verible::Symbol& data_type) {
  return verible::GetSubtreeAsSymbol(data_type, NodeEnum::kDataType, 1);
}

const verible::SyntaxTreeNode* GetPackedDimensionFromDataType(
    const verible::Symbol& data_type) {
  const auto* pdim =
      verible::GetSubtreeAsSymbol(data_type, NodeEnum::kDataType, 3);
  return verible::CheckOptionalSymbolAsNode(pdim, NodeEnum::kPackedDimensions);
}

static const verible::SyntaxTreeNode& GetDataTypeFromInstantiationType(
    const verible::Symbol& instantiation_type) {
  return verible::GetSubtreeAsNode(instantiation_type,
                                   NodeEnum::kInstantiationType, 0);
  // returned node could be kDataType or kInterfaceType
}

static const verible::SyntaxTreeNode& GetReferenceFromReferenceCallBase(
    const verible::Symbol& reference_call_base) {
  return verible::GetSubtreeAsNode(reference_call_base,
                                   NodeEnum::kReferenceCallBase, 0);
}

static const verible::SyntaxTreeNode& GetLocalRootFromReference(
    const verible::Symbol& reference) {
  return verible::GetSubtreeAsNode(reference, NodeEnum::kReference, 0);
}

const verible::Symbol& GetIdentifiersFromLocalRoot(
    const verible::Symbol& local_root) {
  return *ABSL_DIE_IF_NULL(
      verible::GetSubtreeAsSymbol(local_root, NodeEnum::kLocalRoot, 0));
}

const verible::SyntaxTreeNode& GetUnqualifiedIdFromReferenceCallBase(
    const verible::Symbol& reference_call_base) {
  const verible::SyntaxTreeNode& reference =
      GetReferenceFromReferenceCallBase(reference_call_base);
  const verible::SyntaxTreeNode& local_root =
      GetLocalRootFromReference(reference);
  return verible::SymbolCastToNode(GetIdentifiersFromLocalRoot(local_root));
}

const verible::SyntaxTreeNode* GetStructOrUnionOrEnumTypeFromDataType(
    const verible::Symbol& data_type) {
  const verible::Symbol* type = GetBaseTypeFromDataType(data_type);

  if (type == nullptr ||
      NodeEnum(type->Tag().tag) != NodeEnum::kDataTypePrimitive) {
    return nullptr;
  }

  const verible::Symbol* inner_type =
      verible::GetSubtreeAsSymbol(*type, NodeEnum::kDataTypePrimitive, 0);

  if (inner_type->Kind() != verible::SymbolKind::kNode) {
    return nullptr;
  }

  return &verible::SymbolCastToNode(*inner_type);
}

const verible::SyntaxTreeNode* GetStructOrUnionOrEnumTypeFromInstantiationType(
    const verible::Symbol& instantiation_type) {
  const verible::Symbol* type =
      &GetDataTypeFromInstantiationType(instantiation_type);
  if (type == nullptr || NodeEnum(type->Tag().tag) != NodeEnum::kDataType) {
    return nullptr;
  }
  return GetStructOrUnionOrEnumTypeFromDataType(*type);
}

const verible::Symbol* GetBaseTypeFromInstantiationType(
    const verible::Symbol& instantiation_type) {
  const verible::SyntaxTreeNode& data_type =
      GetDataTypeFromInstantiationType(instantiation_type);
  if (NodeEnum(data_type.Tag().tag) != NodeEnum::kDataType) {
    return nullptr;
  }
  return GetBaseTypeFromDataType(data_type);
}

const verible::SyntaxTreeNode* GetParamListFromUnqualifiedId(
    const verible::Symbol& unqualified_id) {
  const verible::SyntaxTreeNode& unqualified_id_node =
      verible::CheckSymbolAsNode(unqualified_id, NodeEnum::kUnqualifiedId);
  if (unqualified_id_node.children().size() < 2) {
    return nullptr;
  }
  const verible::Symbol* param_list = unqualified_id_node.children()[1].get();
  return verible::CheckOptionalSymbolAsNode(param_list,
                                            NodeEnum::kActualParameterList);
}

const verible::SyntaxTreeNode* GetParamListFromBaseType(
    const verible::Symbol& base_type) {
  if (base_type.Tag().kind != verible::SymbolKind::kNode) return nullptr;
  const verible::SyntaxTreeNode& node(verible::SymbolCastToNode(base_type));
  if (!node.MatchesTag(NodeEnum::kUnqualifiedId)) return nullptr;
  return GetParamListFromUnqualifiedId(node);
}

const verible::SyntaxTreeNode* GetParamListFromInstantiationType(
    const verible::Symbol& instantiation_type) {
  const verible::Symbol* base_type =
      GetBaseTypeFromInstantiationType(instantiation_type);
  if (base_type == nullptr) {
    return nullptr;
  }
  return GetParamListFromBaseType(*base_type);
}

std::pair<const verible::SyntaxTreeLeaf*, int>
GetSymbolIdentifierFromDataTypeImplicitIdDimensions(
    const verible::Symbol& struct_union_member) {
  // The Identifier can be at index 1 or 2.
  const verible::Symbol* identifier = verible::GetSubtreeAsSymbol(
      struct_union_member, NodeEnum::kDataTypeImplicitIdDimensions, 2);
  if (identifier != nullptr &&
      identifier->Kind() == verible::SymbolKind::kLeaf) {
    return {&verible::SymbolCastToLeaf(*identifier), 2};
  }
  return {&verible::GetSubtreeAsLeaf(
              struct_union_member, NodeEnum::kDataTypeImplicitIdDimensions, 1),
          1};
}

const verible::SyntaxTreeLeaf* GetNonprimitiveTypeOfDataTypeImplicitDimensions(
    const verible::Symbol& data_type_implicit_id_dimensions) {
  const verible::SyntaxTreeNode& type_node =
      verible::GetSubtreeAsNode(data_type_implicit_id_dimensions,
                                NodeEnum::kDataTypeImplicitIdDimensions, 0);
  const verible::Symbol* identifier = GetBaseTypeFromDataType(type_node);
  if (identifier == nullptr ||
      identifier->Kind() != verible::SymbolKind::kLeaf) {
    return nullptr;
  }
  return &verible::SymbolCastToLeaf(*identifier);
}

const verible::SyntaxTreeNode* GetReferencedTypeOfTypeDeclaration(
    const verible::Symbol& type_declaration) {
  // Could be a kForwardTypeDeclaration, which could be empty.
  return &verible::GetSubtreeAsNode(type_declaration,
                                    NodeEnum::kTypeDeclaration, 1);
}

const verible::SyntaxTreeLeaf& GetSymbolIdentifierFromEnumName(
    const verible::Symbol& enum_name) {
  return verible::GetSubtreeAsLeaf(enum_name, NodeEnum::kEnumName, 0);
}

const verible::SyntaxTreeLeaf& GetTypeIdentifierFromInterfaceType(
    const verible::Symbol& interface_type) {
  return verible::GetSubtreeAsLeaf(interface_type, NodeEnum::kInterfaceType, 2);
}

const verible::Symbol* GetTypeIdentifierFromInstantiationType(
    const verible::Symbol& instantiation_type) {
  const verible::SyntaxTreeNode& data_type =
      GetDataTypeFromInstantiationType(instantiation_type);
  if (NodeEnum(data_type.Tag().tag) == NodeEnum::kDataType) {
    return GetTypeIdentifierFromDataType(data_type);
  }
  if (NodeEnum(data_type.Tag().tag) == NodeEnum::kInterfaceType) {
    return &GetTypeIdentifierFromInterfaceType(data_type);
  }
  return nullptr;
}

const verible::SyntaxTreeNode* GetTypeIdentifierFromDataType(
    const verible::Symbol& data_type) {
  const verible::SyntaxTreeNode& data_type_node =
      verible::SymbolCastToNode(data_type);
  if (!data_type_node.MatchesTag(NodeEnum::kDataType)) return nullptr;
  // TODO(fangism): remove this check after fixing this bug:
  //   x = 1;
  // This is the whole test case.
  // This is a global, implicit-type data declaration, initialized.
  // See https://github.com/google/verible/issues/549
  if (data_type_node.children().empty()) {
    return nullptr;
  }
  const verible::Symbol* base_type = GetBaseTypeFromDataType(data_type);
  if (base_type == nullptr) return nullptr;
  return GetTypeIdentifierFromBaseType(*base_type);
}

const verible::SyntaxTreeNode* GetTypeIdentifierFromBaseType(
    const verible::Symbol& base_type) {
  const auto tag = static_cast<NodeEnum>(base_type.Tag().tag);
  if (tag == NodeEnum::kUnqualifiedId || tag == NodeEnum::kQualifiedId) {
    return &verible::SymbolCastToNode(base_type);
  }
  return nullptr;
}

}  // namespace verilog
