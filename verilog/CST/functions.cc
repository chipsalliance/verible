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

#include <vector>

#include "common/analysis/matcher/matcher.h"
#include "common/analysis/matcher/matcher_builders.h"
#include "common/analysis/syntax_tree_search.h"
#include "common/text/concrete_syntax_tree.h"
#include "common/text/symbol.h"
#include "common/text/tree_utils.h"
#include "verilog/CST/identifier.h"
#include "verilog/CST/verilog_matchers.h"  // pragma IWYU: keep

namespace verilog {

using verible::GetSubtreeAsNode;
using verible::GetSubtreeAsSymbol;
using verible::Symbol;
using verible::SyntaxTreeNode;

std::vector<verible::TreeSearchMatch> FindAllFunctionDeclarations(
    const Symbol& root) {
  return verible::SearchSyntaxTree(root, NodekFunctionDeclaration());
}

const verible::SyntaxTreeNode& GetFunctionHeader(const Symbol& function_decl) {
  return GetSubtreeAsNode(function_decl, NodeEnum::kFunctionDeclaration, 0,
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
  const auto& header = GetFunctionHeader(function_decl);
  return GetFunctionHeaderLifetime(header);
}

const Symbol* GetFunctionReturnType(const Symbol& function_decl) {
  const auto& header = GetFunctionHeader(function_decl);
  return GetFunctionHeaderReturnType(header);
}

const Symbol* GetFunctionId(const Symbol& function_decl) {
  const auto& header = GetFunctionHeader(function_decl);
  return GetFunctionHeaderId(header);
}

const Symbol* GetFunctionFormalPortsGroup(const Symbol& function_decl) {
  const auto& header = GetFunctionHeader(function_decl);
  return GetFunctionHeaderFormalPortsGroup(header);
}

const verible::TokenInfo& GetFunctionNameTokenInfo(
    const verible::Symbol& function_decl) {
  const auto& function_id = GetFunctionId(function_decl);
  return GetIdentifier(*function_id)->get();
}

const verible::TokenInfo& GetFunctionNameTokenInfoInFunctionCall(
    const verible::Symbol& function_call) {
  const auto& local_root = GetSubtreeAsNode(
      function_call, NodeEnum::kFunctionCall, 0, NodeEnum::kLocalRoot);

  const auto& unqualified_id = GetSubtreeAsNode(
      local_root, NodeEnum::kLocalRoot, 0, NodeEnum::kUnqualifiedId);

  return GetIdentifier(unqualified_id)->get();
}

const verible::SyntaxTreeNode& GetFunctionBlockStatmentList(
    const verible::Symbol& function_decl) {
  return verible::GetSubtreeAsNode(function_decl,
                                   NodeEnum::kFunctionDeclaration, 2,
                                   NodeEnum::kBlockItemStatementList);
}

}  // namespace verilog
