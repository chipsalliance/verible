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
#include "verible/verilog/CST/net.h"

#include <vector>

#include "absl/strings/str_cat.h"
#include "gtest/gtest.h"
#include "verible/common/analysis/syntax-tree-search-test-utils.h"
#include "verible/common/analysis/syntax-tree-search.h"
#include "verible/common/text/syntax-tree-context.h"
#include "verible/common/text/text-structure.h"
#include "verible/common/util/logging.h"
#include "verible/verilog/CST/declaration.h"
#include "verible/verilog/CST/match-test-utils.h"
#include "verible/verilog/CST/verilog-nonterminals.h"
#include "verible/verilog/analysis/verilog-analyzer.h"

#undef EXPECT_OK
#define EXPECT_OK(value) EXPECT_TRUE((value).ok())
#undef ASSERT_OK
#define ASSERT_OK(value) ASSERT_TRUE((value).ok())

namespace verilog {
namespace {

using verible::TextStructureView;
using verible::TreeSearchMatch;

// Tests that no nets are found from an empty source.
TEST(FindAllNetDeclarationsTest, EmptySource) {
  VerilogAnalyzer analyzer("", "");
  EXPECT_OK(analyzer.Analyze());
  const auto &root = analyzer.Data().SyntaxTree();
  const auto net_declarations = FindAllNetDeclarations(*ABSL_DIE_IF_NULL(root));
  EXPECT_TRUE(net_declarations.empty());
}

// Tests that no nets are found from a source with only one class.
TEST(FindAllNetDeclarationsTest, NonNet) {
  VerilogAnalyzer analyzer("class foo; endclass", "");
  EXPECT_OK(analyzer.Analyze());
  const auto &root = analyzer.Data().SyntaxTree();
  const auto net_declarations = FindAllNetDeclarations(*ABSL_DIE_IF_NULL(root));
  EXPECT_TRUE(net_declarations.empty());
}

// Declarations of single nets, used across multiple tests.  Semicolon omitted.
static const char *kSingleDeclTestCases[] = {
    "wire w",     "wire [7:0] w",       "wire w [0:3]",
    "wire w [4]", "wire [7:0] w [0:3]",
};

// Tests that one net is found as a package item.
TEST(FindAllNetDeclarationsTest, OneWireNet) {
  for (auto test : kSingleDeclTestCases) {
    VerilogAnalyzer analyzer(absl::StrCat(test, ";"), "");
    EXPECT_TRUE(true) << "code: " << test;
    EXPECT_OK(analyzer.Analyze()) << "code: " << test;
    const auto &root = analyzer.Data().SyntaxTree();
    const auto net_declarations =
        FindAllNetDeclarations(*ABSL_DIE_IF_NULL(root));
    EXPECT_EQ(net_declarations.size(), 1);
    const auto &decl = net_declarations.front();
    EXPECT_FALSE(decl.context.IsInside(NodeEnum::kModuleDeclaration));
  }
}

// Tests that multiple package item level nets are found.
TEST(FindAllNetDeclarationsTest, MultiNets) {
  VerilogAnalyzer analyzer(R"(
wire w1;
package p;
endpackage
wire w2;
class c;
endclass
)",
                           "");
  EXPECT_OK(analyzer.Analyze());
  const auto &root = analyzer.Data().SyntaxTree();
  const auto net_declarations = FindAllNetDeclarations(*ABSL_DIE_IF_NULL(root));
  EXPECT_EQ(net_declarations.size(), 2);
}

// Test that one net with array dimensions is found.
TEST(FindAllNetDeclarationsTest, OneNetWithDimensions) {
  VerilogAnalyzer analyzer("wire [7:0] w [0:3];", "");
  EXPECT_OK(analyzer.Analyze());
  const auto &root = analyzer.Data().SyntaxTree();
  const auto net_declarations = FindAllNetDeclarations(*ABSL_DIE_IF_NULL(root));
  EXPECT_EQ(net_declarations.size(), 1);
}

// Tests that a port wire inside a module is not counted.
TEST(FindAllNetDeclarationsTest, OnePortInModule) {
  for (auto test : kSingleDeclTestCases) {
    VerilogAnalyzer analyzer(absl::StrCat("module m(", test, "); endmodule"),
                             "");
    EXPECT_OK(analyzer.Analyze());
    const auto &root = analyzer.Data().SyntaxTree();
    const auto net_declarations =
        FindAllNetDeclarations(*ABSL_DIE_IF_NULL(root));
    EXPECT_TRUE(net_declarations.empty());
  }
}

// Tests that a local wire inside a module is found.
TEST(FindAllNetDeclarationsTest, OneLocalNetInModule) {
  for (auto test : kSingleDeclTestCases) {
    VerilogAnalyzer analyzer(absl::StrCat("module m;", test, "; endmodule"),
                             "");
    ASSERT_OK(analyzer.Analyze());
    const auto &root = analyzer.Data().SyntaxTree();
    const auto net_declarations =
        FindAllNetDeclarations(*ABSL_DIE_IF_NULL(root));
    ASSERT_EQ(net_declarations.size(), 1);
    const auto &decl = net_declarations.front();
    EXPECT_TRUE(decl.context.IsInside(NodeEnum::kModuleDeclaration));
  }
}

TEST(GetIdentifiersFromNetDeclarationTest, EmptySource) {
  VerilogAnalyzer analyzer("", "");
  ASSERT_OK(analyzer.Analyze());
  const auto &root = analyzer.Data().SyntaxTree();
  const auto net_declarations =
      GetIdentifiersFromNetDeclaration(*ABSL_DIE_IF_NULL(root));
  EXPECT_TRUE(net_declarations.empty());
}

TEST(GetIdentifiersFromNetDeclarationTest, NoNet) {
  VerilogAnalyzer analyzer("module foo; endmodule", "");
  ASSERT_OK(analyzer.Analyze());
  const auto &root = analyzer.Data().SyntaxTree();
  const auto net_declarations =
      GetIdentifiersFromNetDeclaration(*ABSL_DIE_IF_NULL(root));
  EXPECT_TRUE(net_declarations.empty());
}

TEST(GetIdentifiersFromNetDeclarationTest, OneVariable) {
  VerilogAnalyzer analyzer("module foo; wire v; endmodule", "");
  ASSERT_OK(analyzer.Analyze());
  const auto &root = analyzer.Data().SyntaxTree();
  const auto net_declarations =
      GetIdentifiersFromNetDeclaration(*ABSL_DIE_IF_NULL(root));
  ASSERT_EQ(net_declarations.size(), 1);
  EXPECT_EQ(net_declarations[0]->text(), "v");
}

TEST(GetIdentifiersFromNetDeclarationTest, MultipleVariables) {
  VerilogAnalyzer analyzer("module foo; wire x; wire y; endmodule", "");
  ASSERT_OK(analyzer.Analyze());
  const auto &root = analyzer.Data().SyntaxTree();
  const auto net_declarations =
      GetIdentifiersFromNetDeclaration(*ABSL_DIE_IF_NULL(root));
  ASSERT_EQ(net_declarations.size(), 2);
  ASSERT_EQ(net_declarations[0]->text(), "x");
  ASSERT_EQ(net_declarations[1]->text(), "y");
}

TEST(GetIdentifiersFromNetDeclarationTest, MultipleInlineVariables) {
  VerilogAnalyzer analyzer("module foo; wire x, y, z; endmodule", "");
  ASSERT_OK(analyzer.Analyze());
  const auto &root = analyzer.Data().SyntaxTree();
  const auto net_declarations =
      GetIdentifiersFromNetDeclaration(*ABSL_DIE_IF_NULL(root));
  ASSERT_EQ(net_declarations.size(), 3);
  EXPECT_EQ(net_declarations[0]->text(), "x");
  EXPECT_EQ(net_declarations[1]->text(), "y");
  EXPECT_EQ(net_declarations[2]->text(), "z");
}

TEST(GetIdentifiersFromNetDeclarationTest, MultipleMixedVariables) {
  VerilogAnalyzer analyzer("module foo; wire x, y, z; wire a; endmodule", "");
  ASSERT_OK(analyzer.Analyze());
  const auto &root = analyzer.Data().SyntaxTree();
  const auto net_declarations =
      GetIdentifiersFromNetDeclaration(*ABSL_DIE_IF_NULL(root));
  ASSERT_EQ(net_declarations.size(), 4);
  EXPECT_EQ(net_declarations[0]->text(), "x");
  EXPECT_EQ(net_declarations[1]->text(), "y");
  EXPECT_EQ(net_declarations[2]->text(), "z");
  EXPECT_EQ(net_declarations[3]->text(), "a");
}

TEST(GetIdentifiersFromNetDeclarationTest, DoNotMatchArrayDeclarations) {
  VerilogAnalyzer analyzer("module top; wire v[M:N]; endmodule", "");
  ASSERT_OK(analyzer.Analyze());
  const auto &root = analyzer.Data().SyntaxTree();
  const auto net_declarations =
      GetIdentifiersFromNetDeclaration(*ABSL_DIE_IF_NULL(root));
  ASSERT_EQ(net_declarations.size(), 1);
  EXPECT_EQ(net_declarations[0]->text(), "v");
}

TEST(GetIdentifiersFromNetDeclarationTest, DoNotMatchNetArrayDeclarations) {
  VerilogAnalyzer analyzer("module top; wire[X:Z] v[M:N]; endmodule", "");
  ASSERT_OK(analyzer.Analyze());
  const auto &root = analyzer.Data().SyntaxTree();
  const auto net_declarations =
      GetIdentifiersFromNetDeclaration(*ABSL_DIE_IF_NULL(root));
  ASSERT_EQ(net_declarations.size(), 1);
  EXPECT_EQ(net_declarations[0]->text(), "v");
}

TEST(GetIdentifiersFromNetDeclarationTest, DoNotMatchNetType) {
  VerilogAnalyzer analyzer("module top; wire othertype v; endmodule", "");
  ASSERT_OK(analyzer.Analyze());
  const auto &root = analyzer.Data().SyntaxTree();
  const auto net_declarations =
      GetIdentifiersFromNetDeclaration(*ABSL_DIE_IF_NULL(root));
  ASSERT_EQ(net_declarations.size(), 1);
  EXPECT_EQ(net_declarations[0]->text(), "v");
}

TEST(GetIdentifiersFromNetDeclarationTest, DoNotMatchAssignedVariables) {
  VerilogAnalyzer analyzer("module top; wire v = z; endmodule", "");
  ASSERT_OK(analyzer.Analyze());
  const auto &root = analyzer.Data().SyntaxTree();
  const auto net_declarations =
      GetIdentifiersFromNetDeclaration(*ABSL_DIE_IF_NULL(root));
  ASSERT_EQ(net_declarations.size(), 1);
  EXPECT_EQ(net_declarations[0]->text(), "v");
}

TEST(GetNameLeafOfNetVariableTest, Various) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCases[] = {
      {""},
      {
          "module m;\n"
          "endmodule\n",
      },
      {
          "module m;\n"
          "  wire ",
          {kTag, "ww"},
          ";\n"
          "endmodule\n",
      },
      {
          "module m;\n"
          "  wire [2:0] ",
          {kTag, "ww"},
          "[0:1];\n"
          "endmodule\n",
      },
      {
          "module m;\n"
          "  wire ",
          {kTag, "ww"},
          ", ",
          {kTag, "xx"},
          ";\n"
          "endmodule\n",
      },
      {
          // port declarations are considered different
          "module m(\n"
          "  wire ww);\n"
          "endmodule\n",
      },
      {
          "module m;\n"
          "  if (1) begin\n"  // in conditional generate
          "    wire ",
          {kTag, "rr"},
          ";\n"
          "  end\n"
          "endmodule\n",
      },
  };
  for (const auto &test : kTestCases) {
    VLOG(1) << "code:\n" << test.code;
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          const auto &net_decls = FindAllNetVariables(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> net_ids;
          for (const auto &decl : net_decls) {
            const auto *net_name = GetNameLeafOfNetVariable(*decl.match);
            net_ids.emplace_back(
                TreeSearchMatch{net_name, {/* ignored context */}});
          }
          return net_ids;
        });
  }
}

TEST(GetNameLeafOfRegisterVariableTest, Various) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::SyntaxTreeSearchTestCase kTestCases[] = {
      {""},
      {
          "module m;\n"
          "endmodule\n",
      },
      {
          "module m;\n"
          "  logic ",
          {kTag, "ww"},
          ";\n"
          "endmodule\n",
      },
      {
          "module m;\n"
          "  logic [2:0] ",
          {kTag, "ww"},
          "[0:1];\n"
          "endmodule\n",
      },
      {
          "module m;\n"
          "  logic ",
          {kTag, "ww"},
          ", ",
          {kTag, "xx"},
          ";\n"
          "endmodule\n",
      },
      {
          // port declarations are considered different
          "module m(\n"
          "  logic ww);\n"
          "endmodule\n",
      },
      {
          "module m;\n"
          "  if (1) begin\n"  // in conditional generate
          "    logic ",
          {kTag, "rr"},
          ";\n"
          "  end\n"
          "endmodule\n",
      },
  };
  for (const auto &test : kTestCases) {
    VLOG(1) << "code:\n" << test.code;
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          const auto &net_decls =
              FindAllRegisterVariables(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> net_ids;
          for (const auto &decl : net_decls) {
            const auto *net_name = GetNameLeafOfRegisterVariable(*decl.match);
            net_ids.emplace_back(
                TreeSearchMatch{net_name, {/* ignored context */}});
          }
          return net_ids;
        });
  }
}

}  // namespace
}  // namespace verilog
