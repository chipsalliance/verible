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

// Unit tests for net-related concrete-syntax-tree functions.
//
// Testing strategy:
// The point of these tests is to validate the structure that is assumed
// about net declaration nodes and the structure that is actually
// created by the parser, so test *should* use the parser-generated
// syntax trees, as opposed to hand-crafted/mocked syntax trees.
#include "verible/verilog/CST/data.h"

#include <vector>

#include "gtest/gtest.h"
#include "verible/common/text/text-structure.h"
#include "verible/common/util/logging.h"
#include "verible/verilog/analysis/verilog-analyzer.h"

#undef ASSERT_OK
#define ASSERT_OK(value) ASSERT_TRUE((value).ok())

namespace verilog {
namespace {

TEST(GetIdentifiersFromDataDeclarationTest, EmptySource) {
  VerilogAnalyzer analyzer("", "");
  ASSERT_OK(analyzer.Analyze());
  const auto &root = analyzer.Data().SyntaxTree();
  const auto data_declarations =
      GetIdentifiersFromDataDeclaration(*ABSL_DIE_IF_NULL(root));
  EXPECT_TRUE(data_declarations.empty());
}

TEST(GetIdentifiersFromDataDeclarationTest, NoData) {
  VerilogAnalyzer analyzer("module foo; endmodule", "");
  ASSERT_OK(analyzer.Analyze());
  const auto &root = analyzer.Data().SyntaxTree();
  const auto data_declarations =
      GetIdentifiersFromDataDeclaration(*ABSL_DIE_IF_NULL(root));
  EXPECT_TRUE(data_declarations.empty());
}

TEST(GetIdentifiersFromDataDeclarationTest, OneVariable) {
  VerilogAnalyzer analyzer("module foo; logic v; endmodule", "");
  ASSERT_OK(analyzer.Analyze());
  const auto &root = analyzer.Data().SyntaxTree();
  const auto data_declarations =
      GetIdentifiersFromDataDeclaration(*ABSL_DIE_IF_NULL(root));
  ASSERT_EQ(data_declarations.size(), 1);
  EXPECT_EQ(data_declarations[0]->text(), "v");
}

TEST(GetIdentifiersFromDataDeclarationTest, MultipleVariables) {
  VerilogAnalyzer analyzer("module foo; logic x; logic y; endmodule", "");
  ASSERT_OK(analyzer.Analyze());
  const auto &root = analyzer.Data().SyntaxTree();
  const auto data_declarations =
      GetIdentifiersFromDataDeclaration(*ABSL_DIE_IF_NULL(root));
  ASSERT_EQ(data_declarations.size(), 2);
  EXPECT_EQ(data_declarations[0]->text(), "x");
  EXPECT_EQ(data_declarations[1]->text(), "y");
}

TEST(GetIdentifiersFromDataDeclarationTest, MultipleInlineVariables) {
  VerilogAnalyzer analyzer("module foo; logic x, y, z; endmodule", "");
  ASSERT_OK(analyzer.Analyze());
  const auto &root = analyzer.Data().SyntaxTree();
  const auto data_declarations =
      GetIdentifiersFromDataDeclaration(*ABSL_DIE_IF_NULL(root));
  ASSERT_EQ(data_declarations.size(), 3);
  EXPECT_EQ(data_declarations[0]->text(), "x");
  EXPECT_EQ(data_declarations[1]->text(), "y");
  EXPECT_EQ(data_declarations[2]->text(), "z");
}

TEST(GetIdentifiersFromDataDeclarationTest, MultipleMixedVariables) {
  VerilogAnalyzer analyzer("module foo; logic x, y, z; logic a; endmodule", "");
  ASSERT_OK(analyzer.Analyze());
  const auto &root = analyzer.Data().SyntaxTree();
  const auto data_declarations =
      GetIdentifiersFromDataDeclaration(*ABSL_DIE_IF_NULL(root));
  ASSERT_EQ(data_declarations.size(), 4);
  EXPECT_EQ(data_declarations[0]->text(), "x");
  EXPECT_EQ(data_declarations[1]->text(), "y");
  EXPECT_EQ(data_declarations[2]->text(), "z");
  EXPECT_EQ(data_declarations[3]->text(), "a");
}

TEST(GetIdentifiersFromDataDeclarationTest, OneObjectVariable) {
  VerilogAnalyzer analyzer("module top; foo baz(0); endmodule", "");
  ASSERT_OK(analyzer.Analyze());
  const auto &root = analyzer.Data().SyntaxTree();
  const auto data_declarations =
      GetIdentifiersFromDataDeclaration(*ABSL_DIE_IF_NULL(root));
  ASSERT_EQ(data_declarations.size(), 1);
  EXPECT_EQ(data_declarations[0]->text(), "baz");
}

TEST(GetIdentifiersFromDataDeclarationTest, MultipleObjectVariables) {
  VerilogAnalyzer analyzer("module top; foo baz(0); foo bay(1); endmodule", "");
  ASSERT_OK(analyzer.Analyze());
  const auto &root = analyzer.Data().SyntaxTree();
  const auto data_declarations =
      GetIdentifiersFromDataDeclaration(*ABSL_DIE_IF_NULL(root));
  ASSERT_EQ(data_declarations.size(), 2);
  EXPECT_EQ(data_declarations[0]->text(), "baz");
  EXPECT_EQ(data_declarations[1]->text(), "bay");
}

TEST(GetIdentifiersFromDataDeclarationTest, MultipleInlineObjectVariables) {
  VerilogAnalyzer analyzer("module top; foo baz(0), bay(1); endmodule", "");
  ASSERT_OK(analyzer.Analyze());
  const auto &root = analyzer.Data().SyntaxTree();
  const auto data_declarations =
      GetIdentifiersFromDataDeclaration(*ABSL_DIE_IF_NULL(root));
  ASSERT_EQ(data_declarations.size(), 2);
  EXPECT_EQ(data_declarations[0]->text(), "baz");
  EXPECT_EQ(data_declarations[1]->text(), "bay");
}

TEST(GetIdentifiersFromDataDeclarationTest, CompleteMixOfVariables) {
  VerilogAnalyzer analyzer(R"(
module foo;
foo bax(0);
foo baz(0), bay(1);
logic a;
logic b, c;
endmodule)",
                           "");
  ASSERT_OK(analyzer.Analyze());
  const auto &root = analyzer.Data().SyntaxTree();
  const auto data_declarations =
      GetIdentifiersFromDataDeclaration(*ABSL_DIE_IF_NULL(root));
  ASSERT_EQ(data_declarations.size(), 6);
  EXPECT_EQ(data_declarations[0]->text(), "bax");
  EXPECT_EQ(data_declarations[1]->text(), "baz");
  EXPECT_EQ(data_declarations[2]->text(), "bay");
  EXPECT_EQ(data_declarations[3]->text(), "a");
  EXPECT_EQ(data_declarations[4]->text(), "b");
  EXPECT_EQ(data_declarations[5]->text(), "c");
}

TEST(GetIdentifiersFromDataDeclarationTest, DoNotMatchArrayDeclarations) {
  VerilogAnalyzer analyzer("module top; logic v[M:N]; endmodule", "");
  ASSERT_OK(analyzer.Analyze());
  const auto &root = analyzer.Data().SyntaxTree();
  const auto data_declarations =
      GetIdentifiersFromDataDeclaration(*ABSL_DIE_IF_NULL(root));
  ASSERT_EQ(data_declarations.size(), 1);
  EXPECT_EQ(data_declarations[0]->text(), "v");
}

TEST(GetIdentifiersFromDataDeclarationTest, DoNotMatchAssignedVariables) {
  VerilogAnalyzer analyzer("module top; logic v = z; endmodule", "");
  ASSERT_OK(analyzer.Analyze());
  const auto &root = analyzer.Data().SyntaxTree();
  const auto data_declarations =
      GetIdentifiersFromDataDeclaration(*ABSL_DIE_IF_NULL(root));
  ASSERT_EQ(data_declarations.size(), 1);
  EXPECT_EQ(data_declarations[0]->text(), "v");
}

}  // namespace
}  // namespace verilog
