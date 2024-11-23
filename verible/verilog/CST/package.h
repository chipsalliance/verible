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
// package declaration nodes in the parser-generated concrete syntax tree.

#ifndef VERIBLE_VERILOG_CST_PACKAGE_H_
#define VERIBLE_VERILOG_CST_PACKAGE_H_

#include <vector>

#include "verible/common/analysis/syntax-tree-search.h"
#include "verible/common/text/concrete-syntax-leaf.h"  // IWYU pragma: export
#include "verible/common/text/concrete-syntax-tree.h"  // IWYU pragma: export
#include "verible/common/text/symbol.h"                // IWYU pragma: export
#include "verible/common/text/token-info.h"

namespace verilog {

// Find all package declarations.
std::vector<verible::TreeSearchMatch> FindAllPackageDeclarations(
    const verible::Symbol &);

// Find all package imports items.
std::vector<verible::TreeSearchMatch> FindAllPackageImportItems(
    const verible::Symbol &root);

// Extract the subnode of a package declaration that is the package name.
const verible::TokenInfo *GetPackageNameToken(const verible::Symbol &);

// Return the node spanning the name of the package.
const verible::SyntaxTreeLeaf *GetPackageNameLeaf(const verible::Symbol &s);

// Extracts the node that spans the name of the package after endpackage if
// exists.
const verible::SyntaxTreeLeaf *GetPackageNameEndLabel(
    const verible::Symbol &package_declaration);

// Extracts the node that spans the body of the package.
const verible::Symbol *GetPackageItemList(
    const verible::Symbol &package_declaration);

// Extracts the node spanning the ScopePrefix node within PackageImportItem
// node.
const verible::SyntaxTreeNode *GetScopePrefixFromPackageImportItem(
    const verible::Symbol &package_import_item);

// Extracts package name for package import (node tagged with
// kPackageImportItem).
// e.g import pkg::my_integer return the node spanning "pkg".
const verible::SyntaxTreeLeaf *GetImportedPackageName(
    const verible::Symbol &package_import_item);

// Extracts the symbol identifier from PackageImportItem if exits.
// e.g import pkg::my_integer return the node spanning "my_integer".
// return nullptr in case of import pkg::*.
const verible::SyntaxTreeLeaf *GeImportedItemNameFromPackageImportItem(
    const verible::Symbol &package_import_item);

}  // namespace verilog

#endif  // VERIBLE_VERILOG_CST_PACKAGE_H_
