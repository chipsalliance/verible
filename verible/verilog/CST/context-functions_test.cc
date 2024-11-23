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

#include "verible/verilog/CST/context-functions.h"

#include "gtest/gtest.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/syntax-tree-context.h"
#include "verible/common/text/tree-builder-test-util.h"
#include "verible/common/util/casts.h"
#include "verible/verilog/CST/verilog-nonterminals.h"

namespace verilog {
namespace analysis {
namespace {

using verible::down_cast;
using verible::SymbolPtr;
using verible::SyntaxTreeContext;
using verible::SyntaxTreeNode;
using verible::TNode;

const SyntaxTreeNode &CastAsNode(const SymbolPtr &p) {
  return down_cast<const SyntaxTreeNode &>(*p.get());
}

// Test that empty context is handled correctly.
TEST(ContextIsInsideClassTest, EmptyContext) {
  SyntaxTreeContext empty_context;
  EXPECT_FALSE(ContextIsInsideClass(empty_context));
}

// Test that matching context node tag is detected.
TEST(ContextIsInsideClassTest, ClassContextOnly) {
  const auto class_node = TNode(NodeEnum::kClassDeclaration);
  SyntaxTreeContext context;
  SyntaxTreeContext::AutoPop pop(&context, &CastAsNode(class_node));
  EXPECT_TRUE(ContextIsInsideClass(context));
}

// Test that non-matching context node tag is not detected.
TEST(ContextIsInsideClassTest, ModuleContextOnly) {
  const auto module_node = TNode(NodeEnum::kModuleDeclaration);
  SyntaxTreeContext context;
  SyntaxTreeContext::AutoPop pop(&context, &CastAsNode(module_node));
  EXPECT_FALSE(ContextIsInsideClass(context));
}

// Test that matching context ignores inner context.
TEST(ContextIsInsideClassTest, OtherInsideClass) {
  const auto class_node = TNode(NodeEnum::kClassDeclaration);
  const auto module_node = TNode(NodeEnum::kModuleDeclaration);
  SyntaxTreeContext context;
  SyntaxTreeContext::AutoPop pop1(&context, &CastAsNode(class_node));
  SyntaxTreeContext::AutoPop pop2(&context, &CastAsNode(module_node));
  EXPECT_TRUE(ContextIsInsideClass(context));
}

// Test that matching inner context works.
TEST(ContextIsInsideClassTest, ClassInsideOther) {
  const auto class_node = TNode(NodeEnum::kClassDeclaration);
  const auto module_node = TNode(NodeEnum::kModuleDeclaration);
  SyntaxTreeContext context;
  SyntaxTreeContext::AutoPop pop1(&context, &CastAsNode(module_node));
  SyntaxTreeContext::AutoPop pop2(&context, &CastAsNode(class_node));
  EXPECT_TRUE(ContextIsInsideClass(context));
}

// Test that non-matching context node tag is not detected.
TEST(ContextIsInsideModuleTest, ClassContextOnly) {
  const auto class_node = TNode(NodeEnum::kClassDeclaration);
  SyntaxTreeContext context;
  SyntaxTreeContext::AutoPop pop(&context, &CastAsNode(class_node));
  EXPECT_FALSE(ContextIsInsideModule(context));
}

// Test that matching context node tag is detected.
TEST(ContextIsInsideModuleTest, ModuleContextOnly) {
  const auto module_node = TNode(NodeEnum::kModuleDeclaration);
  SyntaxTreeContext context;
  SyntaxTreeContext::AutoPop pop(&context, &CastAsNode(module_node));
  EXPECT_TRUE(ContextIsInsideModule(context));
}

// Test that matching context ignores inner context.
TEST(ContextIsInsideModuleTest, OtherInsideModule) {
  const auto class_node = TNode(NodeEnum::kClassDeclaration);
  const auto module_node = TNode(NodeEnum::kModuleDeclaration);
  SyntaxTreeContext context;
  SyntaxTreeContext::AutoPop pop1(&context, &CastAsNode(class_node));
  SyntaxTreeContext::AutoPop pop2(&context, &CastAsNode(module_node));
  EXPECT_TRUE(ContextIsInsideModule(context));
}

// Test that matching inner context works.
TEST(ContextIsInsideModuleTest, ModuleInsideOther) {
  const auto class_node = TNode(NodeEnum::kClassDeclaration);
  const auto module_node = TNode(NodeEnum::kModuleDeclaration);
  SyntaxTreeContext context;
  SyntaxTreeContext::AutoPop pop1(&context, &CastAsNode(module_node));
  SyntaxTreeContext::AutoPop pop2(&context, &CastAsNode(class_node));
  EXPECT_TRUE(ContextIsInsideModule(context));
}

// Test that packed dimensions are found in context.
TEST(ContextIsInsidePackedDimensionsTest, PackedNotUnpacked) {
  const auto data_node = TNode(NodeEnum::kDataDeclaration);
  const auto dimensions_node = TNode(NodeEnum::kPackedDimensions);
  SyntaxTreeContext context;
  SyntaxTreeContext::AutoPop pop1(&context, &CastAsNode(data_node));
  SyntaxTreeContext::AutoPop pop2(&context, &CastAsNode(dimensions_node));
  EXPECT_TRUE(ContextIsInsidePackedDimensions(context));
  EXPECT_FALSE(ContextIsInsideUnpackedDimensions(context));
}

// Test that unpacked dimensions are found in context.
TEST(ContextIsInsidePackedDimensionsTest, UnpackedNotPacked) {
  const auto data_node = TNode(NodeEnum::kDataDeclaration);
  const auto dimensions_node = TNode(NodeEnum::kUnpackedDimensions);
  SyntaxTreeContext context;
  SyntaxTreeContext::AutoPop pop1(&context, &CastAsNode(data_node));
  SyntaxTreeContext::AutoPop pop2(&context, &CastAsNode(dimensions_node));
  EXPECT_TRUE(ContextIsInsideUnpackedDimensions(context));
  EXPECT_FALSE(ContextIsInsidePackedDimensions(context));
}

// Test that non-matching context node tag is not detected.
TEST(ContextIsInsidePackageTest, ClassContextOnly) {
  const auto class_node = TNode(NodeEnum::kClassDeclaration);
  SyntaxTreeContext context;
  SyntaxTreeContext::AutoPop pop(&context, &CastAsNode(class_node));
  EXPECT_FALSE(ContextIsInsidePackage(context));
}

// Test that matching context node tag is detected.
TEST(ContextIsInsidePackageTest, PackageContextOnly) {
  const auto package_node = TNode(NodeEnum::kPackageDeclaration);
  SyntaxTreeContext context;
  SyntaxTreeContext::AutoPop pop(&context, &CastAsNode(package_node));
  EXPECT_TRUE(ContextIsInsidePackage(context));
}

// Test that matching context ignores inner context.
TEST(ContextIsInsidePackageTest, OtherInsidePackage) {
  const auto package_node = TNode(NodeEnum::kPackageDeclaration);
  const auto module_node = TNode(NodeEnum::kModuleDeclaration);
  SyntaxTreeContext context;
  SyntaxTreeContext::AutoPop pop1(&context, &CastAsNode(package_node));
  SyntaxTreeContext::AutoPop pop2(&context, &CastAsNode(module_node));
  EXPECT_TRUE(ContextIsInsidePackage(context));
}

// Test that matching inner context works.
TEST(ContextIsInsidePackageTest, PackageInsideOther) {
  const auto package_node = TNode(NodeEnum::kPackageDeclaration);
  const auto module_node = TNode(NodeEnum::kModuleDeclaration);
  SyntaxTreeContext context;
  SyntaxTreeContext::AutoPop pop1(&context, &CastAsNode(module_node));
  SyntaxTreeContext::AutoPop pop2(&context, &CastAsNode(package_node));
  EXPECT_TRUE(ContextIsInsidePackage(context));
}

// Test that non-matching context node tag is not detected.
TEST(ContextIsInsideFormalParameterListTest, NotAContext) {
  const auto package_node = TNode(NodeEnum::kPackageDeclaration);
  const auto parameter_node = TNode(NodeEnum::kParamDeclaration);
  SyntaxTreeContext context;
  SyntaxTreeContext::AutoPop pop1(&context, &CastAsNode(package_node));
  SyntaxTreeContext::AutoPop pop2(&context, &CastAsNode(parameter_node));
  EXPECT_FALSE(ContextIsInsideFormalParameterList(context));
}

// Test that matching context node tag is detected.
TEST(ContextIsInsideFormalParameterListTest, FormalContext) {
  const auto module_node = TNode(NodeEnum::kModuleDeclaration);
  const auto formal_param_node = TNode(NodeEnum::kFormalParameterList);
  SyntaxTreeContext context;
  SyntaxTreeContext::AutoPop pop1(&context, &CastAsNode(module_node));
  SyntaxTreeContext::AutoPop pop2(&context, &CastAsNode(formal_param_node));
  EXPECT_TRUE(ContextIsInsideFormalParameterList(context));
}

// Testing that the matching context node tag is detected.
TEST(ContextIsInsideTaskFunctionPortListTest, PortListInsideFunction) {
  const auto port_list_node = TNode(NodeEnum::kPortList);
  const auto function_node = TNode(NodeEnum::kFunctionDeclaration);
  SyntaxTreeContext context;
  SyntaxTreeContext::AutoPop pop1(&context, &CastAsNode(function_node));
  EXPECT_FALSE(ContextIsInsideTaskFunctionPortList(context));
  SyntaxTreeContext::AutoPop pop2(&context, &CastAsNode(port_list_node));
  EXPECT_TRUE(ContextIsInsideTaskFunctionPortList(context));
}

// Testing that the matching context node tag is detected.
TEST(ContextIsInsideTaskFunctionPortListTest, PortListInsideTask) {
  const auto port_list_node = TNode(NodeEnum::kPortList);
  const auto task_node = TNode(NodeEnum::kTaskDeclaration);
  SyntaxTreeContext context;
  SyntaxTreeContext::AutoPop pop1(&context, &CastAsNode(task_node));
  EXPECT_FALSE(ContextIsInsideTaskFunctionPortList(context));
  SyntaxTreeContext::AutoPop pop2(&context, &CastAsNode(port_list_node));
  EXPECT_TRUE(ContextIsInsideTaskFunctionPortList(context));
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
