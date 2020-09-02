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

// Unit tests for module-related concrete-syntax-tree functions.
//
// Testing strategy:
// The point of these tests is to validate the structure that is assumed
// about module declaration nodes and the structure that is actually
// created by the parser, so test *should* use the parser-generated
// syntax trees, as opposed to hand-crafted/mocked syntax trees.

#include "verilog/CST/class.h"

#include <memory>
#include <vector>

#include "common/text/concrete_syntax_tree.h"
#include "common/text/symbol.h"
#include "common/text/text_structure.h"
#include "common/text/token_info.h"
#include "common/text/token_info_test_util.h"
#include "common/util/casts.h"
#include "common/util/logging.h"
#include "common/util/range.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "verilog/analysis/verilog_analyzer.h"

#undef EXPECT_OK
#define EXPECT_OK(value) EXPECT_TRUE((value).ok())

#undef ASSERT_OK
#define ASSERT_OK(value) ASSERT_TRUE((value).ok())

namespace verilog {
namespace {

using verible::down_cast;
using verible::SyntaxTreeNode;
using verible::TokenInfoTestData;

TEST(GetClassNameTokenTest, ClassName) {
  VerilogAnalyzer analyzer("class foo; endclass", "");
  EXPECT_OK(analyzer.Analyze());
  const auto& root = analyzer.Data().SyntaxTree();
  const auto class_declarations = FindAllClassDeclarations(*root);
  EXPECT_EQ(class_declarations.size(), 1);
  const auto& class_node =
      down_cast<const SyntaxTreeNode&>(*class_declarations.front().match);
  // Root node is a description list, not a module.
  const auto& token = GetClassNameToken(class_node);
  EXPECT_EQ(token.text(), "foo");
}

TEST(GetClassNameTokenTest, InnerClassName) {
  VerilogAnalyzer analyzer("module m();\n class foo;\n endclass\n endmodule: m",
                           "");
  EXPECT_OK(analyzer.Analyze());
  const auto& root = analyzer.Data().SyntaxTree();
  const auto class_declarations = FindAllClassDeclarations(*root);
  EXPECT_EQ(class_declarations.size(), 1);
  const auto& class_node =
      down_cast<const SyntaxTreeNode&>(*class_declarations.front().match);
  // Root node is a description list, not a module.
  const auto& token = GetClassNameToken(class_node);
  EXPECT_EQ(token.text(), "foo");
}

TEST(GetClassNameTokenTest, ClassEndLabel) {
  VerilogAnalyzer analyzer("class foo; endclass: foo", "");
  EXPECT_OK(analyzer.Analyze());
  const auto& root = analyzer.Data().SyntaxTree();
  const auto class_declarations = FindAllClassDeclarations(*root);
  EXPECT_EQ(class_declarations.size(), 1);
  const auto& class_node =
      down_cast<const SyntaxTreeNode&>(*class_declarations.front().match);
  // Root node is a description list, not a module.
  const auto& token = *GetClassEndLabel(class_node);
  EXPECT_EQ(token.text(), "foo");
}

TEST(GetClassNameTokenTest, InnerClassEndLabel) {
  VerilogAnalyzer analyzer("module m();\n class foo;\n endclass: foo\n endmodule: m",
                           "");
  EXPECT_OK(analyzer.Analyze());
  const auto& root = analyzer.Data().SyntaxTree();
  const auto class_declarations = FindAllClassDeclarations(*root);
  EXPECT_EQ(class_declarations.size(), 1);
  const auto& class_node =
      down_cast<const SyntaxTreeNode&>(*class_declarations.front().match);
  // Root node is a description list, not a module.
  const auto& token = *GetClassEndLabel(class_node);
  EXPECT_EQ(token.text(), "foo");
}

}  // namespace
}  // namespace verilog
