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

#include "verible/verilog/CST/type.h"

#include <utility>
#include <vector>

#include "verible/common/analysis/syntax-tree-search.h"
#include "verible/common/text/concrete-syntax-leaf.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/tree-utils.h"
#include "verible/common/util/logging.h"
#include "verible/verilog/CST/identifier.h"
#include "verible/verilog/CST/verilog-matchers.h"  // pragma IWYU: keep
#include "verible/verilog/CST/verilog-nonterminals.h"

namespace verilog {

using verible::Symbol;
using verible::SymbolPtr;

static SymbolPtr ReinterpretLocalRootAsType(Symbol &local_root) {  // NOLINT
  auto &node{verible::SymbolCastToNode(local_root)};
  CHECK(!node.empty());
  return std::move(node.front());
}

SymbolPtr ReinterpretReferenceAsDataTypePackedDimensions(
    SymbolPtr &reference_call_base) {
  if (reference_call_base->Tag().tag ==
      static_cast<int>(NodeEnum::kMacroCall)) {
    return std::move(reference_call_base);
  }
  verible::SyntaxTreeNode &base(verible::CheckSymbolAsNode(
      *ABSL_DIE_IF_NULL(reference_call_base), NodeEnum::kReference));
  CHECK(!base.empty());

  Symbol &local_root(*base.front());
  if (local_root.Kind() != verible::SymbolKind::kNode ||
      verible::SymbolCastToNode(*base.back())
          .MatchesTag(NodeEnum::kHierarchyExtension)) {
    // function call -like syntax can never be interpreted as a type,
    // so return the whole subtree unmodified.
    return std::move(reference_call_base);
  }

  SymbolPtr packed_dimensions(
      verible::MakeTaggedNode(NodeEnum::kPackedDimensions));
  verible::SyntaxTreeNode &pdim_node(
      verible::SymbolCastToNode(*packed_dimensions));

  SymbolPtr local_root_with_extension(
      verible::MakeTaggedNode(NodeEnum::kLocalRoot));

  verible::SyntaxTreeNode &local_root_with_extension_node(
      verible::SymbolCastToNode(*local_root_with_extension));
  local_root_with_extension_node.AppendChild(
      ReinterpretLocalRootAsType(*base.front()));

  bool is_first = true;
  for (auto &child : base.mutable_children()) {
    if (is_first) {
      is_first = false;
      continue;
    }
    // Each child could be a call-extension or an index (bit-select/slice).
    // Only [] indices are valid, any others are syntax errors.
    // We discard syntax errors for now, but in the future should retain these
    // error nodes for diagnostics.

    if (!child) continue;

    if (child->Kind() == verible::SymbolKind::kNode &&
        verible::SymbolCastToNode(*child).MatchesTag(
            NodeEnum::kHierarchyExtension)) {
      local_root_with_extension_node.AppendChild(std::move(child));
      continue;
    }

    const auto tag = child->Tag();
    if (tag.kind != verible::SymbolKind::kNode) {
      pdim_node.AppendChild(std::move(child));
      continue;
    }

    auto &node = verible::SymbolCastToNode(*child);
    if (node.MatchesTagAnyOf(
            {NodeEnum::kDimensionRange, NodeEnum::kDimensionScalar})) {
      pdim_node.AppendChild(std::move(child));
    }
    // TODO(fangism): instead of ignoring, retain non-tag-matched nodes as
    // syntax error nodes.
  }
  return MakeDataType(local_root_with_extension, packed_dimensions);
}

std::vector<verible::TreeSearchMatch> FindAllDataTypeDeclarations(
    const verible::Symbol &root) {
  return verible::SearchSyntaxTree(root, NodekDataType());
}

std::vector<verible::TreeSearchMatch> FindAllEnumNames(
    const verible::Symbol &root) {
  return verible::SearchSyntaxTree(root, NodekEnumName());
}

std::vector<verible::TreeSearchMatch> FindAllDataTypePrimitive(
    const verible::Symbol &root) {
  return verible::SearchSyntaxTree(root, NodekDataTypePrimitive());
}

std::vector<verible::TreeSearchMatch> FindAllTypeDeclarations(
    const verible::Symbol &root) {
  return verible::SearchSyntaxTree(root, NodekTypeDeclaration());
}

std::vector<verible::TreeSearchMatch> FindAllEnumTypes(
    const verible::Symbol &root) {
  return verible::SearchSyntaxTree(root, NodekEnumType());
}

std::vector<verible::TreeSearchMatch> FindAllStructTypes(
    const verible::Symbol &root) {
  return verible::SearchSyntaxTree(root, NodekStructType());
}

std::vector<verible::TreeSearchMatch> FindAllDataTypeImplicitIdDimensions(
    const verible::Symbol &root) {
  return verible::SearchSyntaxTree(root, NodekDataTypeImplicitIdDimensions());
}

std::vector<verible::TreeSearchMatch> FindAllUnionTypes(
    const verible::Symbol &root) {
  return verible::SearchSyntaxTree(root, NodekUnionType());
}

std::vector<verible::TreeSearchMatch> FindAllInterfaceTypes(
    const verible::Symbol &root) {
  return verible::SearchSyntaxTree(root, NodekInterfaceType());
}

bool IsStorageTypeOfDataTypeSpecified(const verible::Symbol &symbol) {
  const auto *storage = GetBaseTypeFromDataType(symbol);
  return (storage != nullptr);
}

const verible::SyntaxTreeLeaf *GetIdentifierFromTypeDeclaration(
    const verible::Symbol &symbol) {
  // For enum, struct and union identifier is found at the same position
  const auto *identifier_symbol =
      verible::GetSubtreeAsSymbol(symbol, NodeEnum::kTypeDeclaration, 2);
  return identifier_symbol ? AutoUnwrapIdentifier(*identifier_symbol) : nullptr;
}

const verible::Symbol *GetBaseTypeFromDataType(
    const verible::Symbol &data_type) {
  const auto *local_root =
      verible::GetSubtreeAsNode(data_type, NodeEnum::kDataType, 1);
  if (!local_root) return nullptr;
  if (local_root->Tag().tag != (int)NodeEnum::kLocalRoot) return local_root;

  CHECK(!local_root->empty());
  const auto &children = local_root->children();
  verible::Symbol *last_child = nullptr;
  for (auto &child : children) {
    if (child != nullptr && child->Kind() == verible::SymbolKind::kNode) {
      last_child = child.get();
    }
  }
  if (!last_child) return nullptr;
  if (verible::SymbolCastToNode(*last_child)
          .MatchesTag(NodeEnum::kHierarchyExtension)) {
    // FIXME(jbylicki): This really should return something logical
    return nullptr;
  }
  return last_child;
}

const verible::SyntaxTreeNode *GetPackedDimensionFromDataType(
    const verible::Symbol &data_type) {
  const auto *pdim =
      verible::GetSubtreeAsSymbol(data_type, NodeEnum::kDataType, 3);
  return verible::CheckOptionalSymbolAsNode(pdim, NodeEnum::kPackedDimensions);
}

static const verible::SyntaxTreeNode *GetDataTypeFromInstantiationType(
    const verible::Symbol &instantiation_type) {
  return verible::GetSubtreeAsNode(instantiation_type,
                                   NodeEnum::kInstantiationType, 0);
  // returned node could be kDataType or kInterfaceType
}

static const verible::SyntaxTreeNode *GetReferenceFromReferenceCallBase(
    const verible::Symbol &reference_call_base) {
  return verible::GetSubtreeAsNode(reference_call_base,
                                   NodeEnum::kReferenceCallBase, 0);
}

const verible::SyntaxTreeNode *GetLocalRootFromReference(
    const verible::Symbol &reference) {
  return verible::GetSubtreeAsNode(reference, NodeEnum::kReference, 0);
}

const verible::Symbol *GetIdentifiersFromLocalRoot(
    const verible::Symbol &local_root) {
  return verible::GetSubtreeAsSymbol(local_root, NodeEnum::kLocalRoot, 0);
}
const verible::Symbol *GetIdentifiersFromDataType(
    const verible::Symbol &data_type) {
  return verible::GetSubtreeAsSymbol(data_type, NodeEnum::kDataType, 1);
}

const verible::SyntaxTreeNode *GetUnqualifiedIdFromReferenceCallBase(
    const verible::Symbol &reference_call_base) {
  const verible::SyntaxTreeNode *reference =
      GetReferenceFromReferenceCallBase(reference_call_base);
  if (!reference) return nullptr;
  const verible::SyntaxTreeNode *local_root =
      GetLocalRootFromReference(*reference);
  if (!local_root) return nullptr;
  const verible::Symbol *identifiers = GetIdentifiersFromLocalRoot(*local_root);
  return identifiers ? &verible::SymbolCastToNode(*identifiers) : nullptr;
}

const verible::SyntaxTreeNode *GetStructOrUnionOrEnumTypeFromDataType(
    const verible::Symbol &data_type) {
  const verible::Symbol *type = GetBaseTypeFromDataType(data_type);

  if (type == nullptr ||
      (NodeEnum(type->Tag().tag) != NodeEnum::kDataTypePrimitive &&
       NodeEnum(type->Tag().tag) != NodeEnum::kLocalRoot)) {
    return nullptr;
  }
  const verible::Symbol *inner_type =
      verible::GetSubtreeAsSymbol(*type, NodeEnum::kDataTypePrimitive, 0);

  if (inner_type->Kind() != verible::SymbolKind::kNode) {
    return nullptr;
  }

  return &verible::SymbolCastToNode(*inner_type);
}

const verible::SyntaxTreeNode *GetStructOrUnionOrEnumTypeFromInstantiationType(
    const verible::Symbol &instantiation_type) {
  const verible::Symbol *type =
      GetDataTypeFromInstantiationType(instantiation_type);
  if (type == nullptr || NodeEnum(type->Tag().tag) != NodeEnum::kDataType) {
    return nullptr;
  }
  return GetStructOrUnionOrEnumTypeFromDataType(*type);
}

const verible::Symbol *GetBaseTypeFromInstantiationType(
    const verible::Symbol &instantiation_type) {
  const verible::SyntaxTreeNode *data_type =
      GetDataTypeFromInstantiationType(instantiation_type);
  if (!data_type) return nullptr;
  if (NodeEnum(data_type->Tag().tag) != NodeEnum::kDataType) {
    return nullptr;
  }
  return GetBaseTypeFromDataType(*data_type);
}

const verible::SyntaxTreeNode *GetParamListFromUnqualifiedId(
    const verible::Symbol &unqualified_id) {
  const verible::SyntaxTreeNode &unqualified_id_node =
      verible::CheckSymbolAsNode(unqualified_id, NodeEnum::kUnqualifiedId);
  if (unqualified_id_node.size() < 2) {
    return nullptr;
  }
  const verible::Symbol *param_list = unqualified_id_node[1].get();
  return verible::CheckOptionalSymbolAsNode(param_list,
                                            NodeEnum::kActualParameterList);
}

const verible::SyntaxTreeNode *GetParamListFromBaseType(
    const verible::Symbol &base_type) {
  if (base_type.Tag().kind != verible::SymbolKind::kNode) return nullptr;
  const verible::SyntaxTreeNode &node(verible::SymbolCastToNode(base_type));
  if (!node.MatchesTag(NodeEnum::kUnqualifiedId)) return nullptr;
  return GetParamListFromUnqualifiedId(node);
}

const verible::SyntaxTreeNode *GetParamListFromInstantiationType(
    const verible::Symbol &instantiation_type) {
  const verible::Symbol *base_type =
      GetBaseTypeFromInstantiationType(instantiation_type);
  if (base_type == nullptr) {
    return nullptr;
  }
  return GetParamListFromBaseType(*base_type);
}

std::pair<const verible::SyntaxTreeLeaf *, int>
GetSymbolIdentifierFromDataTypeImplicitIdDimensions(
    const verible::Symbol &struct_union_member) {
  // The Identifier can be at index 1 or 2.
  const verible::Symbol *identifier = verible::GetSubtreeAsSymbol(
      struct_union_member, NodeEnum::kDataTypeImplicitIdDimensions, 2);
  if (identifier != nullptr &&
      identifier->Kind() == verible::SymbolKind::kLeaf) {
    return {&verible::SymbolCastToLeaf(*identifier), 2};
  }
  return {verible::GetSubtreeAsLeaf(struct_union_member,
                                    NodeEnum::kDataTypeImplicitIdDimensions, 1),
          1};
}

const verible::SyntaxTreeLeaf *GetNonprimitiveTypeOfDataTypeImplicitDimensions(
    const verible::Symbol &data_type_implicit_id_dimensions) {
  const verible::SyntaxTreeNode *type_node =
      verible::GetSubtreeAsNode(data_type_implicit_id_dimensions,
                                NodeEnum::kDataTypeImplicitIdDimensions, 0);
  if (!type_node) return nullptr;
  const verible::Symbol *base_type = GetBaseTypeFromDataType(*type_node);
  if (!base_type) return nullptr;
  const verible::Symbol *type_id = GetTypeIdentifierFromBaseType(*base_type);
  if (!type_id) return nullptr;
  const verible::Symbol *identifier = verible::GetLeftmostLeaf(*type_id);
  if (identifier == nullptr ||
      identifier->Kind() != verible::SymbolKind::kLeaf) {
    return nullptr;
  }
  return &verible::SymbolCastToLeaf(*identifier);
}

const verible::SyntaxTreeNode *GetReferencedTypeOfTypeDeclaration(
    const verible::Symbol &type_declaration) {
  // Could be a kForwardTypeDeclaration, which could be empty.
  return verible::GetSubtreeAsNode(type_declaration, NodeEnum::kTypeDeclaration,
                                   1);
}

const verible::SyntaxTreeLeaf *GetSymbolIdentifierFromEnumName(
    const verible::Symbol &enum_name) {
  return verible::GetSubtreeAsLeaf(enum_name, NodeEnum::kEnumName, 0);
}

const verible::SyntaxTreeLeaf *GetTypeIdentifierFromInterfaceType(
    const verible::Symbol &interface_type) {
  return verible::GetSubtreeAsLeaf(interface_type, NodeEnum::kInterfaceType, 2);
}

const verible::Symbol *GetTypeIdentifierFromInstantiationType(
    const verible::Symbol &instantiation_type) {
  const verible::SyntaxTreeNode *data_type =
      GetDataTypeFromInstantiationType(instantiation_type);
  if (!data_type) return nullptr;
  if (NodeEnum(data_type->Tag().tag) == NodeEnum::kDataType) {
    return GetTypeIdentifierFromDataType(*data_type);
  }
  if (NodeEnum(data_type->Tag().tag) == NodeEnum::kInterfaceType) {
    return GetTypeIdentifierFromInterfaceType(*data_type);
  }
  // if (NodeEnum(data_type->Tag().tag) == NodeEnum::kReference) {
  //   const verible::SyntaxTreeNode& data_type_node =
  //   verible::SymbolCastToNode(
  //       *verible::SymbolCastToNode(*data_type).children()[0].get());
  //   return GetTypeIdentifierFromBaseType(data_type_node);
  // }
  return nullptr;
}

const verible::SyntaxTreeNode *GetTypeIdentifierFromDataType(
    const verible::Symbol &data_type) {
  const verible::SyntaxTreeNode &data_type_node =
      verible::SymbolCastToNode(data_type);
  if (!data_type_node.MatchesTag(NodeEnum::kDataType)) return nullptr;
  // TODO(fangism): remove this check after fixing this bug:
  //   x = 1;
  // This is the whole test case.
  // This is a global, implicit-type data declaration, initialized.
  // See https://github.com/chipsalliance/verible/issues/549
  if (data_type_node.empty()) {
    return nullptr;
  }
  const verible::Symbol *base_type = GetBaseTypeFromDataType(data_type);
  if (base_type == nullptr) return nullptr;
  return GetTypeIdentifierFromBaseType(*base_type);
}

const verible::SyntaxTreeNode *GetTypeIdentifierFromBaseType(
    const verible::Symbol &base_type) {
  const auto tag = static_cast<NodeEnum>(base_type.Tag().tag);
  if (tag == NodeEnum::kLocalRoot) {
    return verible::GetSubtreeAsNode(base_type, NodeEnum::kLocalRoot, 0);
  }
  if (tag == NodeEnum::kUnqualifiedId || tag == NodeEnum::kQualifiedId) {
    return &verible::SymbolCastToNode(base_type);
  }
  return nullptr;
}

}  // namespace verilog
