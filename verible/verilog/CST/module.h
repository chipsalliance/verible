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

#ifndef VERIBLE_VERILOG_CST_MODULE_H_
#define VERIBLE_VERILOG_CST_MODULE_H_

#include <vector>

#include "verible/common/analysis/syntax-tree-search.h"
#include "verible/common/text/concrete-syntax-leaf.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/token-info.h"
#include "verible/common/text/tree-utils.h"
#include "verible/verilog/CST/verilog-nonterminals.h"

namespace verilog {

template <typename T0, typename T1, typename T2, typename T3, typename T4,
          typename T5, typename T6, typename T7>
verible::SymbolPtr MakeModuleHeader(T0 &&keyword, T1 &&lifetime, T2 &&id,
                                    T3 &&imports, T4 &&parameters, T5 &&ports,
                                    T6 &&attribute, T7 &&semi) {
  verible::SymbolCastToLeaf(*keyword);
  if (lifetime != nullptr) verible::SymbolCastToLeaf(*lifetime);
  verible::SymbolCastToLeaf(*id);  // SymbolIdentifier or other identifier
  verible::CheckOptionalSymbolAsNode(imports, NodeEnum::kPackageImportList);
  verible::CheckOptionalSymbolAsNode(parameters,
                                     NodeEnum::kFormalParameterListDeclaration);
  verible::CheckOptionalSymbolAsNode(ports, NodeEnum::kParenGroup);
  verible::CheckOptionalSymbolAsNode(attribute,
                                     NodeEnum::kModuleAttributeForeign);
  ExpectString(semi, ";");
  return verible::MakeTaggedNode(
      NodeEnum::kModuleHeader, std::forward<T0>(keyword),
      std::forward<T1>(lifetime), std::forward<T2>(id),
      std::forward<T3>(imports), std::forward<T4>(parameters),
      std::forward<T5>(ports), std::forward<T6>(attribute),
      std::forward<T7>(semi));
}

// Find all module declarations.
std::vector<verible::TreeSearchMatch> FindAllModuleDeclarations(
    const verible::Symbol &);

// Find all module headers.
std::vector<verible::TreeSearchMatch> FindAllModuleHeaders(
    const verible::Symbol &);

// Find all interface declarations.
std::vector<verible::TreeSearchMatch> FindAllInterfaceDeclarations(
    const verible::Symbol &);

// Find all program declarations.
std::vector<verible::TreeSearchMatch> FindAllProgramDeclarations(
    const verible::Symbol &root);

// Returns the full header of a module (params, ports, etc...).
// Works also with interfaces and programs.
const verible::SyntaxTreeNode *GetModuleHeader(const verible::Symbol &);

// Returns the full header of an interface (params, ports, etc...).
const verible::SyntaxTreeNode *GetInterfaceHeader(const verible::Symbol &);

// Extract the subnode of a module declaration that is the module name or
// nullptr if not found.
const verible::SyntaxTreeLeaf *GetModuleName(const verible::Symbol &);

// Extract the subnode of an interface declaration that is the module name.
const verible::TokenInfo *GetInterfaceNameToken(const verible::Symbol &);

// Returns the node spanning the module's port paren group, or nullptr.
// e.g. from "module foo(input x); endmodule", this returns the node that spans
// "(input x)", including parentheses.
const verible::SyntaxTreeNode *GetModulePortParenGroup(
    const verible::Symbol &module_declaration);

// Returns the node spanning module's port declarations list, or nullptr.
// e.g. from "module foo(input x); endmodule", this returns the node that spans
// PortDescriptionList
const verible::SyntaxTreeNode *GetModulePortDeclarationList(
    const verible::Symbol &module_declaration);

// Returns module name leaf after endmodule.
// e.g. from "module foo(); endmodule: foo" returns the second "foo".
const verible::SyntaxTreeLeaf *GetModuleEndLabel(const verible::Symbol &);

// Returns the node spanning module's Item list.
const verible::SyntaxTreeNode *GetModuleItemList(
    const verible::Symbol &module_declaration);

// Extract the subnode of a param declaration list from module decalration.
// e.g module m#(parameter x = 2) return the node spanning "#(parameter x = 2)".
const verible::SyntaxTreeNode *GetParamDeclarationListFromModuleDeclaration(
    const verible::Symbol &);

// Extract the subnode of a param declaration list from interface decalration.
// e.g interface m#(parameter x = 2) return the node spanning "#(parameter x =
// 2)".
const verible::SyntaxTreeNode *GetParamDeclarationListFromInterfaceDeclaration(
    const verible::Symbol &);

}  // namespace verilog

#endif  // VERIBLE_VERILOG_CST_MODULE_H_
