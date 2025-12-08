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

#include "verible/verilog/CST/parameters.h"

#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "gtest/gtest.h"
#include "verible/common/analysis/syntax-tree-search-test-utils.h"
#include "verible/common/analysis/syntax-tree-search.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/text-structure.h"
#include "verible/common/text/token-info.h"
#include "verible/common/util/casts.h"
#include "verible/common/util/logging.h"
#include "verible/verilog/CST/match-test-utils.h"
#include "verible/verilog/CST/verilog-nonterminals.h"
#include "verible/verilog/analysis/verilog-analyzer.h"
#include "verible/verilog/parser/verilog-token-enum.h"

#undef ASSERT_OK
#define ASSERT_OK(value) ASSERT_TRUE((value).ok())

namespace verilog {
namespace {

using verible::down_cast;
using verible::SyntaxTreeSearchTestCase;
using verible::TextStructureView;
using verible::TreeSearchMatch;

// Tests that the correct amount of kParameterDeclarations are found.
TEST(FindAllParamDeclarationsTest, BasicParams) {
  constexpr int kTag = 1;  // don't care
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {""},
      {"module foo; endmodule"},
      {"module foo (input bar); endmodule"},
      {"module foo; ", {kTag, "localparam Bar = 1;"}, " endmodule"},
      {"module foo; ", {kTag, "localparam int Bar = 1;"}, " endmodule"},
      {"module foo; ", {kTag, "parameter int Bar = 1;"}, " endmodule"},
      {"module foo #(", {kTag, "parameter int Bar = 1"}, "); endmodule"},
      {"module foo; ",
       {kTag, "localparam int Bar = 1;"},
       " ",
       {kTag, "localparam int BarSecond = 2;"},
       " endmodule"},
      {"class foo; ", {kTag, "localparam int Bar = 1;"}, " endclass"},
      {"class foo #(", {kTag, "parameter int Bar = 1"}, "); endclass"},
      {"package foo; ", {kTag, "parameter Bar = 1;"}, " endpackage"},
      {"package foo; ", {kTag, "parameter int Bar = 1;"}, " endpackage"},
      {{kTag, "parameter int Bar = 1;"}},
      {{kTag, "parameter Bar = 1;"}},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          return FindAllParamDeclarations(*ABSL_DIE_IF_NULL(root));
        });
  }
}

// Tests that GetParamKeyword correctly returns that the parameter type is
// localparam.
TEST(GetParamKeywordTest, LocalParamDeclared) {
  constexpr std::pair<std::string_view, int> kTestCases[] = {
      {"module foo; localparam int Bar = 1; endmodule", 1},
      {"class foo; localparam int Bar = 1; endclass", 1},
      {"module foo; localparam Bar = 1; endmodule", 1},
  };
  for (const auto &test : kTestCases) {
    VerilogAnalyzer analyzer(test.first, "");
    ASSERT_OK(analyzer.Analyze());
    const auto &root = analyzer.Data().SyntaxTree();
    const auto param_declarations = FindAllParamDeclarations(*root);
    ASSERT_EQ(param_declarations.size(), test.second);
    const auto &param_node = down_cast<const verible::SyntaxTreeNode &>(
        *param_declarations.front().match);
    const auto param_keyword = GetParamKeyword(param_node);
    EXPECT_EQ(param_keyword, TK_localparam);
  }
}

// Tests that GetParamKeyword correctly returns that the parameter type is
// parameter.
TEST(GetParamKeywordTest, ParameterDeclared) {
  constexpr std::pair<std::string_view, int> kTestCases[] = {
      {"module foo; parameter int Bar = 1; endmodule", 1},
      {"module foo #(parameter int Bar = 1); endmodule", 1},
      {"module foo #(int Bar = 1); endmodule", 1},
      {"class foo; parameter int Bar = 1; endclass", 1},
      {"class foo #(parameter int Bar = 1); endclass", 1},
      {"class foo #(int Bar = 1); endclass", 1},
      {"package foo; parameter int Bar = 1; endpackage", 1},
      {"package foo; parameter Bar = 1; endpackage", 1},
      {"parameter int Bar = 1;", 1},
      {"parameter Bar = 1;", 1},
  };
  for (const auto &test : kTestCases) {
    VerilogAnalyzer analyzer(test.first, "");
    ASSERT_OK(analyzer.Analyze());
    const auto &root = analyzer.Data().SyntaxTree();
    const auto param_declarations = FindAllParamDeclarations(*root);
    ASSERT_EQ(param_declarations.size(), test.second);
    const auto &param_node = down_cast<const verible::SyntaxTreeNode &>(
        *param_declarations.front().match);
    const auto param_keyword = GetParamKeyword(param_node);
    EXPECT_EQ(param_keyword, TK_parameter);
  }
}

// Tests that GetParamKeyword correctly returns the parameter type when multiple
// parameters are defined.
TEST(GetParamKeywordTest, MultipleParamsDeclared) {
  constexpr std::string_view kTestCases[] = {
      {"module foo; parameter int Bar = 1; localparam int Bar_2 = 2; "
       "endmodule"},
      {"class foo; parameter int Bar = 1; localparam int Bar_2 = 2; endclass"},
  };
  for (const auto &test : kTestCases) {
    VerilogAnalyzer analyzer(test, "");
    ASSERT_OK(analyzer.Analyze());
    const auto &root = analyzer.Data().SyntaxTree();
    const auto param_declarations = FindAllParamDeclarations(*root);

    // Make sure the first one is TK_parameter.
    const auto &param_node = down_cast<const verible::SyntaxTreeNode &>(
        *param_declarations[0].match);
    const auto param_keyword = GetParamKeyword(param_node);
    EXPECT_EQ(param_keyword, TK_parameter);

    // Make sure the second one is TK_localparam.
    const auto &localparam_node = down_cast<const verible::SyntaxTreeNode &>(
        *param_declarations[1].match);
    const auto localparam_keyword = GetParamKeyword(localparam_node);
    EXPECT_EQ(localparam_keyword, TK_localparam);
  }
}

// Tests that GetParameterToken correctly returns the token of the
// parameter.
TEST(GetParameterTokenTest, BasicTests) {
  constexpr std::pair<std::string_view, std::string_view> kTestCases[] = {
      {"module foo; parameter Bar = 1; endmodule", "parameter"},
      {"module foo; localparam Bar_1 = 1; endmodule", "localparam"},
      {"module foo; localparam int HelloWorld = 1; endmodule", "localparam"},
      {"module foo #(parameter int HelloWorld1 = 1); endmodule", "parameter"},
      {"class foo; parameter HelloWorld_1 = 1; endclass", "parameter"},
      {"class foo; localparam FooBar = 1; endclass", "localparam"},
      {"class foo; localparam int Bar_1_1 = 1; endclass", "localparam"},
      {"package foo; parameter BAR = 1; endpackage", "parameter"},
      {"package foo; parameter int HELLO_WORLD = 1; endpackage", "parameter"},
      {"package foo; localparam BAR = 1; endpackage", "localparam"},
      {"package foo; localparam int HELLO_WORLD = 1; endpackage", "localparam"},
      {"parameter int Bar = 1;", "parameter"},
  };
  for (const auto &test : kTestCases) {
    VerilogAnalyzer analyzer(test.first, "");
    ASSERT_OK(analyzer.Analyze());
    const auto &root = analyzer.Data().SyntaxTree();
    const auto param_declarations = FindAllParamDeclarations(*root);
    const auto name_token =
        GetParameterToken(*param_declarations.front().match);
    EXPECT_EQ(name_token->text(), test.second);
  }
}

// Tests that GetParamTypeSymbol correctly returns the kParamType node.
TEST(GetParamTypeSymbolTest, BasicTests) {
  constexpr std::string_view kTestCases[] = {
      {"module foo; parameter Bar = 1; endmodule"},
      {"module foo; parameter int Bar = 1; endmodule"},
      {"module foo #(parameter int Bar = 1); endmodule"},
      {"module foo; localparam int Bar = 1; endmodule"},
      {"class foo; parameter int Bar = 1; endclass"},
      {"class foo; localparam int Bar = 1; endclass"},
      {"package foo; parameter int Bar = 1; endpackage"},
      {"parameter int Bar = 1;"},
  };
  for (const auto &test : kTestCases) {
    VerilogAnalyzer analyzer(test, "");
    ASSERT_OK(analyzer.Analyze());
    const auto &root = analyzer.Data().SyntaxTree();
    const auto param_declarations = FindAllParamDeclarations(*root);
    const auto *param_type_symbol =
        GetParamTypeSymbol(*param_declarations.front().match);
    const auto t = param_type_symbol->Tag();
    EXPECT_EQ(t.kind, verible::SymbolKind::kNode);
    EXPECT_EQ(NodeEnum(t.tag), NodeEnum::kParamType);
  }
}

// Tests that GetParameterNameToken correctly returns the token of the
// parameter.
TEST(GetParameterNameTokenTest, BasicTests) {
  constexpr std::pair<std::string_view, std::string_view> kTestCases[] = {
      {"module foo; parameter Bar = 1; endmodule", "Bar"},
      {"module foo; localparam Bar_1 = 1; endmodule", "Bar_1"},
      {"module foo; localparam int HelloWorld = 1; endmodule", "HelloWorld"},
      {"module foo #(parameter int HelloWorld1 = 1); endmodule", "HelloWorld1"},
      {"class foo; parameter HelloWorld_1 = 1; endclass", "HelloWorld_1"},
      {"class foo; localparam FooBar = 1; endclass", "FooBar"},
      {"class foo; localparam int Bar_1_1 = 1; endclass", "Bar_1_1"},
      {"package foo; parameter BAR = 1; endpackage", "BAR"},
      {"package foo; parameter int HELLO_WORLD = 1; endpackage", "HELLO_WORLD"},
      {"parameter int Bar = 1;", "Bar"},
  };
  for (const auto &test : kTestCases) {
    VerilogAnalyzer analyzer(test.first, "");
    ASSERT_OK(analyzer.Analyze());
    const auto &root = analyzer.Data().SyntaxTree();
    const auto param_declarations = FindAllParamDeclarations(*root);
    const auto name_token =
        GetParameterNameToken(*param_declarations.front().match);
    EXPECT_EQ(name_token->text(), test.second);
  }
}

// Test that GetAllParameterNameTokens correctly returns all tokens
TEST(GetAllParameterNameTokensTest, BasicTests) {
  constexpr std::pair<std::string_view, int> kTestCases[] = {
      {"module foo; parameter Bar = 1; endmodule", 1},
      {"module foo; localparam Bar_1 = 1; endmodule", 1},
      {"module foo; localparam int HelloWorld = 1; endmodule", 1},
      {"module foo #(parameter int HelloWorld1 = 1); endmodule", 1},
      {"class foo; parameter HelloWorld_1 = 1; endclass", 1},
      {"class foo; localparam FooBar = 1; endclass", 1},
      {"class foo; localparam int Bar_1_1 = 1; endclass", 1},
      {"package foo; parameter BAR = 1; endpackage", 1},
      {"package foo; parameter int HELLO_WORLD = 1; endpackage", 1},
      {"parameter int Bar = 1;", 1},
      {"parameter int Bar = 1, Foo = 1;", 2},
      {"parameter int Bar = 1, Foo = 1, Baz = 1;", 3},
      {"module foo; parameter int Bar = 1; endmodule;", 1},
      {"module foo; parameter int Bar = 1, Foo = 1; endmodule;", 2},
      {"module foo; parameter int Bar = 1, Foo = 1, Baz = 1; endmodule;", 3},
  };

  for (const auto &test : kTestCases) {
    VerilogAnalyzer analyzer(test.first, "");
    ASSERT_OK(analyzer.Analyze());
    const auto &root = analyzer.Data().SyntaxTree();
    const auto param_declarations = FindAllParamDeclarations(*root);
    const auto name_tokens =
        GetAllParameterNameTokens(*param_declarations.front().match);
    EXPECT_EQ(name_tokens.size(), test.second);
  }
}

// Tests that GetAllAssignedParameterSymbols correctly returns all the
// symbols for each kParameterAssign node
TEST(GetAllAssignedParameterSymbolsTest, BasicTests) {
  constexpr std::pair<std::string_view, int> kTestCases[] = {
      {"module foo; parameter Bar = 1; endmodule", 0},
      {"module foo; localparam Bar_1 = 1; endmodule", 0},
      {"module foo; localparam int HelloWorld = 1; endmodule", 0},
      {"module foo #(parameter int HelloWorld1 = 1); endmodule", 0},
      {"class foo; parameter HelloWorld_1 = 1; endclass", 0},
      {"class foo; localparam FooBar = 1; endclass", 0},
      {"class foo; localparam int Bar_1_1 = 1; endclass", 0},
      {"package foo; parameter BAR = 1; endpackage", 0},
      {"package foo; parameter int HELLO_WORLD = 1; endpackage", 0},
      {"parameter int Bar = 1;", 0},
      {"parameter int Bar = 1, Foo = 1;", 1},
      {"parameter int Bar = 1, Foo = 1, Baz = 1;", 2},
      {"module foo; parameter int Bar = 1; endmodule;", 0},
      {"module foo; parameter int Bar = 1, Foo = 1; endmodule;", 1},
      {"module foo; parameter int Bar = 1, Foo = 1, Baz = 1; endmodule;", 2},
  };

  for (const auto &test : kTestCases) {
    VerilogAnalyzer analyzer(test.first, "");
    ASSERT_OK(analyzer.Analyze());
    const auto &root = analyzer.Data().SyntaxTree();
    const auto param_declarations = FindAllParamDeclarations(*root);
    const auto assigned_parameters =
        GetAllAssignedParameterSymbols(*param_declarations.front().match);
    EXPECT_EQ(assigned_parameters.size(), test.second);
  }
}

TEST(GetAssignedParameterNameToken, BasicTests) {
  constexpr std::pair<std::string_view, std::string_view> kTestCases[] = {
      {"parameter int Bar = 1, Foo = 1;", "Foo"},
      {"module foo; parameter int Bar = 1, Fox = 1; endmodule;", "Fox"},
  };

  for (const auto &test : kTestCases) {
    VerilogAnalyzer analyzer(test.first, "");
    ASSERT_OK(analyzer.Analyze());
    const auto &root = analyzer.Data().SyntaxTree();
    const auto param_declarations = FindAllParamDeclarations(*root);
    const auto assigned_parameters =
        GetAllAssignedParameterSymbols(*param_declarations.front().match);
    EXPECT_EQ(assigned_parameters.size(), 1);

    const auto name_token =
        GetAssignedParameterNameToken(*assigned_parameters.front());
    EXPECT_EQ(name_token->text(), test.second);
  }
}

// Tests that GetSymbolIdentifierFromParamDeclaration correctly returns the
// token of the symbol identifier.
TEST(GetSymbolIdentifierFromParamDeclarationTest, BasicTests) {
  constexpr std::pair<std::string_view, std::string_view> kTestCases[] = {
      {"module foo; parameter type Bar; endmodule", "Bar"},
      {"module foo; localparam type Bar_1; endmodule", "Bar_1"},
      {"module foo #(parameter type HelloWorld1); endmodule", "HelloWorld1"},
      {"class foo #(parameter type Bar); endclass", "Bar"},
      {"class foo; parameter type HelloWorld_1; endclass", "HelloWorld_1"},
      {"class foo; localparam type Bar_1_1; endclass", "Bar_1_1"},
      {"package foo; parameter type HELLO_WORLD; endpackage", "HELLO_WORLD"},
      {"parameter type Bar;", "Bar"},
  };
  for (const auto &test : kTestCases) {
    VerilogAnalyzer analyzer(test.first, "");
    ASSERT_OK(analyzer.Analyze());
    const auto &root = analyzer.Data().SyntaxTree();
    const auto param_declarations = FindAllParamDeclarations(*root);
    const auto name_token = GetSymbolIdentifierFromParamDeclaration(
        *param_declarations.front().match);
    EXPECT_EQ(name_token->text(), test.second);
  }
}

// Tests that IsParamTypeDeclaration correctly returns true if the parameter is
// a parameter type declaration.
TEST(IsParamTypeDeclarationTest, BasicTests) {
  constexpr std::pair<std::string_view, bool> kTestCases[] = {
      {"module foo; parameter type Bar; endmodule", true},
      {"module foo; localparam type Bar_1; endmodule", true},
      {"module foo #(parameter type HelloWorld1); endmodule", true},
      {"class foo #(parameter type Bar); endclass", true},
      {"class foo; parameter type HelloWorld_1; endclass", true},
      {"class foo; localparam type Bar_1_1; endclass", true},
      {"package foo; parameter type HELLO_WORLD; endpackage", true},
      {"parameter type Bar;", true},
      {"module foo; parameter Bar = 1; endmodule", false},
      {"module foo; localparam int HelloWorld = 1; endmodule", false},
      {"module foo #(parameter int HelloWorld1 = 1); endmodule", false},
      {"class foo; parameter HelloWorld_1 = 1; endclass", false},
      {"class foo; localparam FooBar = 1; endclass", false},
      {"package foo; parameter int HELLO_WORLD = 1; endpackage", false},
  };
  for (const auto &test : kTestCases) {
    VerilogAnalyzer analyzer(test.first, "");
    ASSERT_OK(analyzer.Analyze());
    const auto &root = analyzer.Data().SyntaxTree();
    const auto param_declarations = FindAllParamDeclarations(*root);
    const auto is_param_type =
        IsParamTypeDeclaration(*param_declarations.front().match);
    EXPECT_EQ(is_param_type, test.second);
  }
}

// Tests that GetTypeAssignmentFromParamDeclaration correctly returns the
// kTypeAssignment node.
TEST(GetTypeAssignmentFromParamDeclarationTests, BasicTests) {
  constexpr std::string_view kTestCases[] = {
      {"module foo; parameter type Bar = 1; endmodule"},
      {"module foo #(parameter type Bar = 1); endmodule"},
      {"module foo; localparam type Bar = 1; endmodule"},
      {"class foo; parameter type Bar = 1; endclass"},
      {"class foo; localparam type Bar = 1; endclass"},
      {"package foo; parameter type Bar = 1; endpackage"},
      {"parameter type Bar = 1;"},
      {"module m#(parameter type Bar)();\nendmodule"},
      {"module m#(parameter Bar)();\nendmodule"},
  };
  for (const auto &test : kTestCases) {
    VerilogAnalyzer analyzer(test, "");
    ASSERT_OK(analyzer.Analyze());
    const auto &root = analyzer.Data().SyntaxTree();
    const auto param_declarations = FindAllParamDeclarations(*root);
    const auto *type_assignment_symbol = GetTypeAssignmentFromParamDeclaration(
        *param_declarations.front().match);
    if (type_assignment_symbol == nullptr) {
      continue;
    }
    const auto t = type_assignment_symbol->Tag();
    EXPECT_EQ(t.kind, verible::SymbolKind::kNode);
    EXPECT_EQ(NodeEnum(t.tag), NodeEnum::kTypeAssignment);
  }
}

// Tests that GetIdentifierLeafFromTypeAssignment correctly returns the
// SyntaxTreeLeaf of the symbol identifier.
TEST(GetIdentifierLeafFromTypeAssignmentTest, BasicTests) {
  constexpr int kTag = 1;  // don't care
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {"module foo; parameter type ", {kTag, "Bar"}, "; endmodule"},
      {"module foo; localparam type ", {kTag, "Bar_1"}, "; endmodule"},
      {"module foo #(parameter type ", {kTag, "HelloWorld1"}, "); endmodule"},
      {"class foo #(parameter type ", {kTag, "Bar"}, "); endclass"},
      {"class foo; parameter type ", {kTag, "HelloWorld_1"}, "; endclass"},
      {"class foo; localparam type ", {kTag, "Bar_1_1"}, "; endclass"},
      {"package foo; parameter type ", {kTag, "HELLO_WORLD"}, "; endpackage"},
      {"parameter type ", {kTag, "Bar"}, ";"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          const auto param_declarations = FindAllParamDeclarations(*root);
          std::vector<TreeSearchMatch> ids;
          for (const auto &decl : param_declarations) {
            const auto *type_assignment_symbol =
                GetTypeAssignmentFromParamDeclaration(*decl.match);
            ids.push_back(TreeSearchMatch{
                GetIdentifierLeafFromTypeAssignment(*type_assignment_symbol),
                /* no context */});
          }
          return ids;
        });
  }
}

// Tests that GetParamTypeInfoSymbol correctly returns the kTypeInfo node.
TEST(GetParamTypeInfoSymbolTest, BasicTests) {
  constexpr std::string_view kTestCases[] = {
      {"module foo; parameter Bar = 1; endmodule"},
      {"module foo; parameter int Bar = 1; endmodule"},
      {"module foo #(parameter int Bar = 1); endmodule"},
      {"module foo; localparam int Bar = 1; endmodule"},
      {"class foo; parameter int Bar = 1; endclass"},
      {"class foo; localparam int Bar = 1; endclass"},
      {"package foo; parameter int Bar = 1; endpackage"},
      {"parameter int Bar = 1;"},
  };
  for (const auto &test : kTestCases) {
    VerilogAnalyzer analyzer(test, "");
    ASSERT_OK(analyzer.Analyze());
    const auto &root = analyzer.Data().SyntaxTree();
    const auto param_declarations = FindAllParamDeclarations(*root);
    const auto *type_info_symbol =
        GetParamTypeInfoSymbol(*param_declarations.front().match);
    const auto t = type_info_symbol->Tag();
    EXPECT_EQ(t.kind, verible::SymbolKind::kNode);
    EXPECT_EQ(NodeEnum(t.tag), NodeEnum::kTypeInfo);
  }
}

TEST(IsTypeInfoEmptyTest, EmptyTests) {
  constexpr std::string_view kTestCases[] = {
      {"module foo; parameter Bar = 1; endmodule"},
      {"module foo #(parameter Bar = 1); endmodule"},
      {"module foo; localparam Bar = 1; endmodule"},
      {"class foo; parameter Bar = 1; endclass"},
      {"class foo; localparam Bar = 1; endclass"},
      {"package foo; parameter Bar = 1; endpackage"},
      {"parameter Bar = 1;"},
  };
  for (const auto &test : kTestCases) {
    VerilogAnalyzer analyzer(test, "");
    ASSERT_OK(analyzer.Analyze());
    const auto &root = analyzer.Data().SyntaxTree();
    const auto param_declarations = FindAllParamDeclarations(*root);
    const auto *type_info_symbol =
        GetParamTypeInfoSymbol(*param_declarations.front().match);
    const auto t = type_info_symbol->Tag();
    EXPECT_EQ(t.kind, verible::SymbolKind::kNode);
    EXPECT_EQ(NodeEnum(t.tag), NodeEnum::kTypeInfo);
    EXPECT_TRUE(IsTypeInfoEmpty(*type_info_symbol));
  }
}

TEST(IsTypeInfoEmptyTest, NonEmptyTests) {
  constexpr std::string_view kTestCases[] = {
      {"module foo; localparam bit Bar = 1; endmodule"},
      {"module foo #(parameter int Bar = 1); endmodule"},
      {"module foo; parameter int Bar = 1; endmodule"},
      {"class foo; parameter string Bar = \"Bar\"; endclass"},
      {"class foo; localparam logic Bar = 1; endclass"},
      {"parameter int Bar = 1;"},
      {"parameter signed Bar = 1;"},
      {"parameter unsigned Bar = 1;"},
      {"parameter int unsigned Bar = 1;"},
      {"parameter Other_t Bar = other_t::kEnum;"},
      {"parameter pkg_p::Other_t Bar = other_t::kEnum;"},
      {"module foo; localparam int signed  Bar = 1; endmodule"},
      {"module foo #(parameter signed Bar = 1); endmodule"},
      {"module foo #(parameter int signed Bar = 1); endmodule"},
      {"module foo #(parameter Other_t Bar); endmodule"},
      {"module foo #(parameter pkg::Other_t Bar); endmodule"},
      {"module foo #(parameter pkg::Other_t Bar = enum_e::value); endmodule"},
      {"class foo #(parameter Other_t Bar); endclass"},
      {"class foo #(parameter pkg::Other_t Bar); endclass"},
      {"class foo #(parameter pkg::Other_t Bar = enum_e::value); endclass"},
  };
  for (const auto &test : kTestCases) {
    VerilogAnalyzer analyzer(test, "");
    ASSERT_OK(analyzer.Analyze());
    const auto &root = analyzer.Data().SyntaxTree();
    const auto param_declarations = FindAllParamDeclarations(*root);
    const auto *type_info_symbol =
        GetParamTypeInfoSymbol(*param_declarations.front().match);
    const auto t = type_info_symbol->Tag();
    EXPECT_EQ(t.kind, verible::SymbolKind::kNode);
    EXPECT_EQ(NodeEnum(t.tag), NodeEnum::kTypeInfo);
    EXPECT_FALSE(IsTypeInfoEmpty(*type_info_symbol));
  }
}

TEST(FindAllParamByNameTest, FindNamesOfParams) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {""},
      {"module m;\nendmodule\n"},
      {"module m;\n module_type #(2, 2) y1();\nendmodule"},
      {"module m;\n module_type #(.",
       {kTag, "P"},
       "(2), .",
       {kTag, "P2"},
       "(2)) y1();\nendmodule"},
      {"module m;\n module_type #(.",
       {kTag, "P"},
       "(2), .",
       {kTag, "P1"},
       "(3)) y1();\nendmodule"},
      {"module m;\n module_type #(x, y) y1();\nendmodule"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          const auto &instances = FindAllNamedParams(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> params;
          for (const auto &instance : instances) {
            const auto *decl = GetNamedParamFromActualParam(*instance.match);

            params.emplace_back(TreeSearchMatch{decl, {/* ignored context */}});
          }
          return params;
        });
  }
}

TEST(FindAllParamByNameTest, FindParenGroupOfNamedParam) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {""},
      {"module m;\nendmodule\n"},
      {"module m;\n module_type #(2, 2) y1();\nendmodule"},
      {"module m;\n module_type #(.P",
       {kTag, "(2)"},
       ", .P2",
       {kTag, "(2)"},
       ") y1();\nendmodule"},
      {"module m;\n module_type #(.P",
       {kTag, "(2)"},
       ", .P1",
       {kTag, "(3)"},
       ") y1();\nendmodule"},
      {"module m;\n module_type #(x, y) y1();\nendmodule"},
      {"module m;\n module_type #(.x, .y) y1();\nendmodule"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          const auto &instances = FindAllNamedParams(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> paren_groups;
          for (const auto &instance : instances) {
            const auto *paren_group =
                GetParenGroupFromActualParam(*instance.match);
            if (paren_group == nullptr) {
              continue;
            }
            paren_groups.emplace_back(
                TreeSearchMatch{paren_group, {/* ignored context */}});
          }
          return paren_groups;
        });
  }
}

TEST(FindAllParamTest, FindExpressionFromParameterType) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {""},
      {"module m;\nendmodule\n"},
      {"module foo; parameter type Bar = ", {kTag, "1"}, "; endmodule"},
      {"module foo #(parameter type Bar = ", {kTag, "H.y"}, "); endmodule"},
      {"module foo; localparam type Bar = ", {kTag, "var1"}, "; endmodule"},
      {"class foo; parameter type Bar = ", {kTag, "1"}, "; endclass"},
      {"class foo; localparam type Bar = ", {kTag, "1"}, "; endclass"},
      {"package foo; parameter type Bar = ", {kTag, "1"}, "; endpackage"},
      {"parameter type Bar = ", {kTag, "1"}, ";"},
      {"module foo #(parameter type Bar = int); endmodule"},
      {"module foo #(parameter type Bar = ", {kTag, "Foo#(1)"}, "); endmodule"},
      {"module foo #(parameter type Bar = ",
       {kTag, "Foo#(int)"},
       "); endmodule"},
      {"module foo #(parameter type Bar = ",
       {kTag, "Foo#(Baz#(int))"},
       "); endmodule"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          const auto &types = FindAllParamDeclarations(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> expressions;
          for (const auto &type : types) {
            const auto *type_assignment =
                GetTypeAssignmentFromParamDeclaration(*type.match);
            if (type_assignment == nullptr) {
              continue;
            }
            const auto *expression =
                GetExpressionFromTypeAssignment(*type_assignment);
            if (expression == nullptr) {
              continue;
            }
            expressions.emplace_back(
                TreeSearchMatch{expression, {/* ignored context */}});
          }
          return expressions;
        });
  }
}

}  // namespace
}  // namespace verilog
