// Copyright 2017-2019 The Verible Authors.
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

// Unit tests for module-related concrete-syntax-tree functions.
//
// Testing strategy:
// The point of these tests is to validate the structure that is assumed
// about module declaration nodes and the structure that is actually
// created by the parser, so test *should* use the parser-generated
// syntax trees, as opposed to hand-crafted/mocked syntax trees.

#include "verilog/CST/module.h"

#include <memory>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "common/text/concrete_syntax_tree.h"
#include "common/text/symbol.h"
#include "common/text/text_structure.h"
#include "common/text/token_info.h"
#include "common/util/casts.h"
#include "common/util/logging.h"
#include "verilog/analysis/verilog_analyzer.h"

#undef EXPECT_OK
#define EXPECT_OK(value) EXPECT_TRUE((value).ok())

namespace verilog {
namespace {

using verible::down_cast;
using verible::SyntaxTreeNode;

TEST(FindAllModuleDeclarationsTest, EmptySource) {
  VerilogAnalyzer analyzer("", "");
  EXPECT_OK(analyzer.Analyze());
  const auto& root = analyzer.Data().SyntaxTree();
  const auto module_declarations =
      FindAllModuleDeclarations(*ABSL_DIE_IF_NULL(root));
  EXPECT_TRUE(module_declarations.empty());
}

TEST(FindAllModuleDeclarationsTest, NonModule) {
  VerilogAnalyzer analyzer("class foo; endclass", "");
  EXPECT_OK(analyzer.Analyze());
  const auto& root = analyzer.Data().SyntaxTree();
  const auto module_declarations =
      FindAllModuleDeclarations(*ABSL_DIE_IF_NULL(root));
  EXPECT_TRUE(module_declarations.empty());
}

TEST(FindAllModuleDeclarationsTest, OneModule) {
  VerilogAnalyzer analyzer("module mod; endmodule", "");
  EXPECT_OK(analyzer.Analyze());
  const auto& root = analyzer.Data().SyntaxTree();
  const auto module_declarations =
      FindAllModuleDeclarations(*ABSL_DIE_IF_NULL(root));
  EXPECT_EQ(module_declarations.size(), 1);
}

TEST(FindAllModuleDeclarationsTest, MultiModules) {
  VerilogAnalyzer analyzer(R"(
module mod1;
endmodule
package p;
endpackage
module mod2(input foo);
endmodule
class c;
endclass
)",
                           "");
  EXPECT_OK(analyzer.Analyze());
  const auto& root = analyzer.Data().SyntaxTree();
  const auto module_declarations =
      FindAllModuleDeclarations(*ABSL_DIE_IF_NULL(root));
  EXPECT_EQ(module_declarations.size(), 2);
}

TEST(GetModuleNameTokenTest, RootIsNotAModule) {
  VerilogAnalyzer analyzer("module foo; endmodule", "");
  EXPECT_OK(analyzer.Analyze());
  const auto& root = analyzer.Data().SyntaxTree();
  // CHECK should fail: root node is a description list, not a module.
  // If this happens, it is a programmer error, not user error.
  EXPECT_DEATH(GetModuleNameToken(*ABSL_DIE_IF_NULL(root)),
               "kDescriptionList vs. kModuleDeclaration");
}

TEST(GetModuleNameTokenTest, ValidModule) {
  VerilogAnalyzer analyzer("module foo; endmodule", "");
  EXPECT_OK(analyzer.Analyze());
  const auto& root = analyzer.Data().SyntaxTree();
  const auto module_declarations = FindAllModuleDeclarations(*root);
  EXPECT_EQ(module_declarations.size(), 1);
  const auto& module_node =
      down_cast<const SyntaxTreeNode&>(*module_declarations.front().match);
  // Root node is a description list, not a module.
  const auto& token = GetModuleNameToken(module_node);
  EXPECT_EQ(token.text, "foo");
}

}  // namespace
}  // namespace verilog
