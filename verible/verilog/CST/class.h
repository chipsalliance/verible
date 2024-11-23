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
// class declaration nodes in the parser-generated concrete syntax tree.

#ifndef VERIBLE_VERILOG_CST_CLASS_H_
#define VERIBLE_VERILOG_CST_CLASS_H_

#include <vector>

#include "verible/common/analysis/syntax-tree-search.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/visitors.h"

namespace verilog {

// Find all class declarations.
std::vector<verible::TreeSearchMatch> FindAllClassDeclarations(
    const verible::Symbol &);

// Find all class constructors.
std::vector<verible::TreeSearchMatch> FindAllClassConstructors(
    const verible::Symbol &root);

// Find all hierarchy extensions.
std::vector<verible::TreeSearchMatch> FindAllHierarchyExtensions(
    const verible::Symbol &);

// Returns the full header of a class.
const verible::SyntaxTreeNode *GetClassHeader(const verible::Symbol &);

// Returns the leaf node for class name.
const verible::SyntaxTreeLeaf *GetClassName(const verible::Symbol &);

// Returns the node that spans the extended class name(if exists).
// e.g from "class my_class extends other_class;" return "other_class".
// e.g from "class my_class extends pkg::my_class2" => return the node that
// spans "pkg::my_class2". e.g "class my_class;" return "nullptr".
const verible::SyntaxTreeNode *GetExtendedClass(const verible::Symbol &);

// Returns class name token after endclass.
// e.g. from "class foo; endclass: foo" returns the second "foo".
const verible::SyntaxTreeLeaf *GetClassEndLabel(const verible::Symbol &);

// Returns the node spanning class's Item list.
const verible::SyntaxTreeNode *GetClassItemList(const verible::Symbol &);

// Returns the identifier from node tagged with kHierarchyExtension.
// e.g from "instance1.x" => return "x".
const verible::SyntaxTreeLeaf *GetUnqualifiedIdFromHierarchyExtension(
    const verible::Symbol &);

// Extract the subnode of a param declaration list from class decalration.
// e.g from "class m#(parameter x = 2)" return the node spanning "#(parameter x
// = 2)".
const verible::SyntaxTreeNode *GetParamDeclarationListFromClassDeclaration(
    const verible::Symbol &);

// Returns the node spanning the class constructor body (tagged with
// kStatementList) from node tagged with kClassConstructor.
const verible::SyntaxTreeNode *GetClassConstructorStatementList(
    const verible::Symbol &class_constructor);

// Returns the leaf spanning the "new" keyword from class constructor.
const verible::SyntaxTreeLeaf *GetNewKeywordFromClassConstructor(
    const verible::Symbol &class_constructor);

}  // namespace verilog

#endif  // VERIBLE_VERILOG_CST_CLASS_H_
