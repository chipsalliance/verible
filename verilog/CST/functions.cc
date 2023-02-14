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

#include "verilog/CST/functions.h"

#include <iostream>
#include <vector>

#include "common/analysis/matcher/matcher.h"
#include "common/analysis/matcher/matcher_builders.h"
#include "common/analysis/syntax_tree_search.h"
#include "common/text/concrete_syntax_tree.h"
#include "common/text/symbol.h"
#include "common/text/tree_utils.h"
#include "verilog/CST/identifier.h"
#include "verilog/CST/type.h"
#include "verilog/CST/verilog_matchers.h"  // pragma IWYU: keep

namespace verilog {

using verible::GetSubtreeAsNode;
using verible::GetSubtreeAsSymbol;
using verible::Symbol;
using verible::SymbolCastToNode;
using verible::SyntaxTreeNode;

std::vector<verible::TreeSearchMatch> FindAllFunctionDeclarations(
    const Symbol& root) {
  return verible::SearchSyntaxTree(root, NodekFunctionDeclaration());
}

std::vector<verible::TreeSearchMatch> FindAllFunctionPrototypes(
    const Symbol& root) {
  return verible::SearchSyntaxTree(root, NodekFunctionPrototype());
}

std::vector<verible::TreeSearchMatch> FindAllFunctionHeaders(
    const Symbol& root) {
  return verible::SearchSyntaxTree(root, NodekFunctionHeader());
}

std::vector<verible::TreeSearchMatch> FindAllFunctionOrTaskCalls(
    const Symbol& root) {
  std::vector<verible::TreeSearchMatch> names;
  auto calls = verible::SearchSyntaxTree(root, NodekFunctionCall());
  for (const auto& Call : calls) {
    names.emplace_back(Call);
  }
  // After anonymous instantiation was introduced, anonymous data declaration
  // and function call are indistinguishable, so under strict conditions it will
  // be allowed here; exactly: {call_extensions . } name (args_opt);
  auto data_declarations =
      verible::SearchSyntaxTree(root, NodekDataDeclaration());
  for (const auto& dec : data_declarations) {
    const verible::Symbol* instantiation_base = verible::GetSubtreeAsSymbol(
        *(dec.match), NodeEnum::kDataDeclaration, 1);
    const verible::Symbol* gate_instance_list = verible::GetSubtreeAsSymbol(
        *instantiation_base, NodeEnum::kInstantiationBase, 1);
    if (gate_instance_list->Tag().tag !=
        (int)NodeEnum::kGateInstanceRegisterVariableList) {
      continue;
    }
    if (verible::SymbolCastToNode(*gate_instance_list).children().size() != 1) {
      continue;
    }
    const verible::Symbol* gate_instance = verible::GetSubtreeAsSymbol(
        *gate_instance_list, NodeEnum::kGateInstanceRegisterVariableList, 0);
    if (gate_instance->Tag().tag != (int)NodeEnum::kGateInstance) {
      continue;
    }
    if (verible::SymbolCastToNode(*gate_instance).children()[0] != nullptr ||
        verible::SymbolCastToNode(*gate_instance).children()[1] != nullptr) {
      continue;
    }
    names.emplace_back(dec);
  }
  return names;
}

std::vector<verible::TreeSearchMatch> FindAllFunctionOrTaskCallsExtension(
    const Symbol& root) {
  auto calls = verible::SearchSyntaxTree(root, NodekFunctionCall());
  std::vector<verible::TreeSearchMatch> names;
  for (const auto& Call : calls) {
    std::vector<verible::TreeSearchMatch> names_ext =
        verible::SearchSyntaxTree(*Call.match, NodekHierarchyExtension());
    for (const auto& foo : names_ext) {
      names.emplace_back(foo);
    }
  }
  auto method_extensions =
      verible::SearchSyntaxTree(root, NodekMethodCallExtension());
  for (const auto& foo : method_extensions) names.emplace_back(foo);
  return names;
}

std::vector<verible::TreeSearchMatch> FindAllConstructorPrototypes(
    const Symbol& root) {
  return verible::SearchSyntaxTree(root, NodekClassConstructorPrototype());
}

const verible::SyntaxTreeNode* GetFunctionHeader(const Symbol& function_decl) {
  return GetSubtreeAsNode(function_decl, NodeEnum::kFunctionDeclaration, 0,
                          NodeEnum::kFunctionHeader);
}

const verible::SyntaxTreeNode* GetFunctionPrototypeHeader(
    const Symbol& function_decl) {
  return GetSubtreeAsNode(function_decl, NodeEnum::kFunctionPrototype, 0,
                          NodeEnum::kFunctionHeader);
}

const Symbol* GetFunctionHeaderLifetime(const Symbol& function_header) {
  return GetSubtreeAsSymbol(function_header, NodeEnum::kFunctionHeader, 2);
}

const Symbol* GetFunctionHeaderReturnType(const Symbol& function_header) {
  return GetSubtreeAsSymbol(function_header, NodeEnum::kFunctionHeader, 3);
}

const Symbol* GetFunctionHeaderId(const Symbol& function_header) {
  return GetSubtreeAsSymbol(function_header, NodeEnum::kFunctionHeader, 4);
}

const Symbol* GetFunctionHeaderFormalPortsGroup(const Symbol& function_header) {
  return GetSubtreeAsSymbol(function_header, NodeEnum::kFunctionHeader, 5);
}

const Symbol* GetFunctionLifetime(const Symbol& function_decl) {
  const auto* header = GetFunctionHeader(function_decl);
  return header ? GetFunctionHeaderLifetime(*header) : nullptr;
}

const Symbol* GetFunctionReturnType(const Symbol& function_decl) {
  const auto* header = GetFunctionHeader(function_decl);
  return header ? GetFunctionHeaderReturnType(*header) : nullptr;
}

const Symbol* GetFunctionId(const Symbol& function_decl) {
  const auto* header = GetFunctionHeader(function_decl);
  return header ? GetFunctionHeaderId(*header) : nullptr;
}

const Symbol* GetFunctionFormalPortsGroup(const Symbol& function_decl) {
  const auto* header = GetFunctionHeader(function_decl);
  return header ? GetFunctionHeaderFormalPortsGroup(*header) : nullptr;
}

const verible::SyntaxTreeLeaf* GetFunctionName(
    const verible::Symbol& function_decl) {
  const auto* function_id = GetFunctionId(function_decl);
  if (!function_id) return nullptr;
  return GetIdentifier(*function_id);
}

const verible::SyntaxTreeNode* GetLocalRootFromFunctionCall(
    const verible::Symbol& function_call) {
  return GetSubtreeAsNode(function_call, NodeEnum::kFunctionCall, 0,
                          NodeEnum::kLocalRoot);
}

const verible::SyntaxTreeNode* GetIdentifiersFromFunctionCall(
    const verible::Symbol& function_call) {
  const verible::Symbol* reference = nullptr;
  const verible::Symbol* identifier = nullptr;
  if (function_call.Tag().tag == (int)NodeEnum::kFunctionCall) {
    reference = GetSubtreeAsSymbol(function_call, NodeEnum::kFunctionCall, 0);
    const verible::SyntaxTreeNode* local_root = GetSubtreeAsNode(
        *reference, NodeEnum::kReference, 0, NodeEnum::kLocalRoot);
    if (!local_root) return nullptr;
    identifier = GetIdentifiersFromLocalRoot(*local_root);
  } else if (function_call.Tag().tag == (int)NodeEnum::kDataDeclaration) {
    // here the reference is actually an instantiation base
    reference =
        GetSubtreeAsSymbol(function_call, NodeEnum::kDataDeclaration, 1);
    if (!reference) return nullptr;
    const verible::Symbol* instantiation_type =
        GetSubtreeAsSymbol(*reference, NodeEnum::kInstantiationBase, 0);
    if (!instantiation_type) return nullptr;
    const verible::SyntaxTreeNode* data_type =
        GetSubtreeAsNode(*instantiation_type, NodeEnum::kInstantiationType, 0,
                         NodeEnum::kDataType);
    if (!data_type) return nullptr;
    identifier = GetIdentifiersFromDataType(*data_type);
  }
  if (!identifier) return nullptr;
  if (identifier->Kind() != verible::SymbolKind::kNode) return nullptr;
  return &verible::SymbolCastToNode(*identifier);
}

const verible::SyntaxTreeLeaf* GetFunctionCallName(
    const verible::Symbol& function_call) {
  const auto* local_root = GetLocalRootFromFunctionCall(function_call);
  if (!local_root) return nullptr;
  const auto* unqualified_id = GetSubtreeAsNode(
      *local_root, NodeEnum::kLocalRoot, 0, NodeEnum::kUnqualifiedId);
  if (!unqualified_id) return nullptr;

  return GetIdentifier(*unqualified_id);
}

const verible::SyntaxTreeLeaf* GetFunctionCallNameFromCallExtension(
    const verible::Symbol& function_call) {
  const auto* unqualified_id =
      GetSubtreeAsNode(function_call, NodeEnum::kHierarchyExtension, 1,
                       NodeEnum::kUnqualifiedId);
  if (!unqualified_id) return nullptr;
  return GetIdentifier(*unqualified_id);
}

const verible::SyntaxTreeNode* GetFunctionBlockStatementList(
    const verible::Symbol& function_decl) {
  return verible::GetSubtreeAsNode(function_decl,
                                   NodeEnum::kFunctionDeclaration, 2);
}

const verible::SyntaxTreeNode* GetParenGroupFromCall(
    const verible::Symbol& function_call) {
  if (function_call.Tag().tag == (int)NodeEnum::kFunctionCall) {
    return verible::GetSubtreeAsNode(function_call, NodeEnum::kFunctionCall, 1,
                                     NodeEnum::kParenGroup);
  } else if (function_call.Tag().tag == (int)NodeEnum::kDataDeclaration) {
    const verible::Symbol* instantiation_base = verible::GetSubtreeAsSymbol(
        function_call, NodeEnum::kDataDeclaration, 1);
    const verible::Symbol* gate_instance_list = verible::GetSubtreeAsSymbol(
        *instantiation_base, NodeEnum::kInstantiationBase, 1);
    const verible::Symbol* gate_instance = verible::GetSubtreeAsSymbol(
        *gate_instance_list, NodeEnum::kGateInstanceRegisterVariableList, 0);
    return verible::GetSubtreeAsNode(*gate_instance, NodeEnum::kGateInstance, 2,
                                     NodeEnum::kParenGroup);
  }
}

const verible::SyntaxTreeNode* GetParenGroupFromCallExtension(
    const verible::Symbol& function_call) {
  // if(function_call.Tag().tag == (int) NodeEnum::kMethodCallExtension)
  //   return verible::GetSubtreeAsNode(
  //       function_call, NodeEnum::kMethodCallExtension, 2,
  //       NodeEnum::kParenGroup);
  return verible::GetSubtreeAsNode(
      function_call, NodeEnum::kMethodCallExtension, 2, NodeEnum::kParenGroup);
}

const verible::SyntaxTreeLeaf* GetConstructorPrototypeNewKeyword(
    const verible::Symbol& constructor_prototype) {
  return GetSubtreeAsLeaf(constructor_prototype,
                          NodeEnum::kClassConstructorPrototype, 1);
}

}  // namespace verilog
