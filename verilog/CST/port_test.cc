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

// Unit tests for port-related concrete-syntax-tree functions.
//
// Testing strategy:
// The point of these tests is to validate the structure that is assumed
// about port declaration nodes and the structure that is actually
// created by the parser, so test *should* use the parser-generated
// syntax trees, as opposed to hand-crafted/mocked syntax trees.

#include "verilog/CST/port.h"

#include <initializer_list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "common/analysis/syntax_tree_search.h"
#include "common/analysis/syntax_tree_search_test_utils.h"
#include "common/text/concrete_syntax_leaf.h"
#include "common/text/syntax_tree_context.h"
#include "common/text/text_structure.h"
#include "common/text/token_info.h"
#include "common/util/logging.h"
#include "verilog/CST/type.h"
#include "verilog/CST/verilog_nonterminals.h"
#include "verilog/analysis/verilog_analyzer.h"

#undef ASSERT_OK
#define ASSERT_OK(value) ASSERT_TRUE((value).ok())

namespace verilog {
namespace {

using verible::SyntaxTreeSearchTestCase;
using verible::TreeSearchMatch;

// Tests that no ports are found from an empty source.
TEST(FindAllModulePortDeclarationsTest, EmptySource) {
  VerilogAnalyzer analyzer("", "");
  ASSERT_OK(analyzer.Analyze());
  const auto& root = analyzer.Data().SyntaxTree();
  const auto port_declarations =
      FindAllModulePortDeclarations(*ABSL_DIE_IF_NULL(root));
  EXPECT_TRUE(port_declarations.empty());
}

// Tests that no ports are found in port-less module.
TEST(FindAllModulePortDeclarationsTest, NonPort) {
  VerilogAnalyzer analyzer("module foo; endmodule", "");
  ASSERT_OK(analyzer.Analyze());
  const auto& root = analyzer.Data().SyntaxTree();
  const auto port_declarations =
      FindAllModulePortDeclarations(*ABSL_DIE_IF_NULL(root));
  EXPECT_TRUE(port_declarations.empty());
}

// Tests that a package-item net declaration is not a port.
TEST(FindAllModulePortDeclarationsTest, OneWire) {
  VerilogAnalyzer analyzer("wire w;", "");
  ASSERT_OK(analyzer.Analyze());
  const auto& root = analyzer.Data().SyntaxTree();
  const auto port_declarations =
      FindAllModulePortDeclarations(*ABSL_DIE_IF_NULL(root));
  EXPECT_TRUE(port_declarations.empty());
}

// Tests that a local wire inside a module is not a port.
TEST(FindAllModulePortDeclarationsTest, OneWireInModule) {
  VerilogAnalyzer analyzer("module m; wire w; endmodule", "");
  ASSERT_OK(analyzer.Analyze());
  const auto& root = analyzer.Data().SyntaxTree();
  const auto port_declarations =
      FindAllModulePortDeclarations(*ABSL_DIE_IF_NULL(root));
  EXPECT_TRUE(port_declarations.empty());
}

// Tests that a port wire inside a module is found.
TEST(FindAllModulePortDeclarationsTest, OnePortInModule) {
  const char* kTestCases[] = {
      "logic l",
      "wire w",
      "input w",
      "input [1:0] w",
      "input w [0:1]",
      "input w [6]",
      "input [7:0] w [6]",
      "input wire w",
      "reg r",
      "output r",
      "output reg r",
      "output reg [1:0] r",
      "output reg r [0:3]",
      "output reg [1:0] r [0:3]",
  };
  for (auto test : kTestCases) {
    VerilogAnalyzer analyzer(absl::StrCat("module m(", test, "); endmodule"),
                             "");
    ASSERT_OK(analyzer.Analyze());
    const auto& root = analyzer.Data().SyntaxTree();
    const auto port_declarations =
        FindAllModulePortDeclarations(*ABSL_DIE_IF_NULL(root));
    EXPECT_EQ(port_declarations.size(), 1);
    const auto& decl = port_declarations.front();
    EXPECT_TRUE(decl.context.IsInside(NodeEnum::kModuleDeclaration));
  }
}

TEST(GetIdentifierFromModulePortDeclarationTest, OnePort) {
  const std::pair<std::string, absl::string_view> kTestCases[] = {
      {"module foo(input bar); endmodule", "bar"},
      {"module foo(input logic b_a_r); endmodule", "b_a_r"},
      {"module foo(input wire hello_world = 1); endmodule", "hello_world"},
      {"module foo(wire hello_world1 = 1); endmodule", "hello_world1"},
      {"module foo(input logic [3:0] bar2); endmodule", "bar2"},
      {"module foo(input logic b_a_r [3:0]); endmodule", "b_a_r"},
      {"module foo(input logic bar [4]); endmodule", "bar"},
  };
  for (const auto& test : kTestCases) {
    VerilogAnalyzer analyzer(test.first, "");
    ASSERT_OK(analyzer.Analyze());
    const auto& root = analyzer.Data().SyntaxTree();
    const auto port_declarations = FindAllModulePortDeclarations(*root);
    const auto* identifier_leaf = GetIdentifierFromModulePortDeclaration(
        *port_declarations.front().match);
    EXPECT_EQ(identifier_leaf->get().text(), test.second);
  }
}

TEST(GetIdentifierFromModulePortDeclarationTest, MultiplePorts) {
  const std::string kTestCases[] = {
      {"module foo(input bar, output bar2); endmodule"},
      {"module foo(input logic bar, input wire bar2); endmodule"},
      {"module foo(input logic bar, output bar2); endmodule"},
      {"module foo(wire bar, wire bar2 = 1); endmodule"},
      {"module foo(input logic [3:0] bar, input logic bar2 [4]); endmodule"},
      {"module foo(input logic bar [3:0], input logic [3:0] bar2); endmodule"},
  };
  for (const auto& test : kTestCases) {
    VerilogAnalyzer analyzer(test, "");
    ASSERT_OK(analyzer.Analyze());
    const auto& root = analyzer.Data().SyntaxTree();
    const auto port_declarations = FindAllModulePortDeclarations(*root);
    ASSERT_EQ(port_declarations.size(), 2);

    const auto* identifier_leaf_1 =
        GetIdentifierFromModulePortDeclaration(*port_declarations[0].match);
    EXPECT_EQ(identifier_leaf_1->get().text(), "bar");
    const auto* identifier_leaf_2 =
        GetIdentifierFromModulePortDeclaration(*port_declarations[1].match);
    EXPECT_EQ(identifier_leaf_2->get().text(), "bar2");
  }
}

// Negative tests that no ports are found where they are not expected.
TEST(FindAllTaskFunctionPortDeclarationsTest, ExpectNoTaskFunctionPorts) {
  constexpr const char* kTestCases[] = {
      "",
      "module foo(input wire bar); endmodule",
      "function void foo(); endfunction",
      "task automatic foo(); endtask",
      "class foo; endclass",
      "class foo; function void bar(); endfunction endclass",
  };
  for (const auto* code : kTestCases) {
    VerilogAnalyzer analyzer(code, "<<inline-file>>");
    ASSERT_OK(analyzer.Analyze());
    const auto& root = analyzer.Data().SyntaxTree();
    const auto port_declarations =
        FindAllTaskFunctionPortDeclarations(*ABSL_DIE_IF_NULL(root));
    EXPECT_TRUE(port_declarations.empty());
  }
}

struct ExpectedPort {
  absl::string_view id;  // name of port
  bool have_type;        // is type specified?
};

struct TaskFunctionTestCase {
  const std::string code;
  std::initializer_list<ExpectedPort> expected_ports;
};

TEST(GetIdentifierFromTaskFunctionPortItemTest, ExpectSomeTaskFunctionPorts) {
  const TaskFunctionTestCase kTestCases[] = {
      // Function cases
      {"function void foo(bar); endfunction", {{"bar", false}}},
      {"function void foo(bar, baz); endfunction",
       {{"bar", false}, {"baz", false}}},
      {"function void foo(input int bar, output int baz); endfunction",
       {{"bar", true}, {"baz", true}}},
      {"class cls; function void foo(bar, baz); endfunction endclass",
       {{"bar", false}, {"baz", false}}},
      {"module mod; function void foo(bar, baz); endfunction endmodule",
       {{"bar", false}, {"baz", false}}},
      {"function void foo(input pkg::t_t bar, output pkg::t_t baz); "
       "endfunction",
       {{"bar", true}, {"baz", true}}},
      // Same, but for tasks
      {"task automatic foo(bar); endtask", {{"bar", false}}},
      {"task automatic foo(bar, baz); endtask",
       {{"bar", false}, {"baz", false}}},
      {"task automatic foo(input int bar, output int baz); endtask",
       {{"bar", true}, {"baz", true}}},
      {"class cls; task automatic foo(bar, baz); endtask endclass",
       {{"bar", false}, {"baz", false}}},
      {"module mod; task automatic foo(bar, baz); endtask endmodule",
       {{"bar", false}, {"baz", false}}},
      {"task automatic foo(input pkg::t_t bar, output pkg::t_t baz); endtask",
       {{"bar", true}, {"baz", true}}},
  };
  for (const auto& test : kTestCases) {
    const std::string& code = test.code;
    const auto& expected_ports = test.expected_ports;
    VerilogAnalyzer analyzer(code, "<<inline-file>>");
    ASSERT_OK(analyzer.Analyze()) << "Failed code:\n" << code;
    const auto& root = analyzer.Data().SyntaxTree();
    const auto port_declarations =
        FindAllTaskFunctionPortDeclarations(*ABSL_DIE_IF_NULL(root));
    ASSERT_EQ(port_declarations.size(), expected_ports.size());

    // Compare expected port ids one-by-one, and check for type presence.
    int i = 0;
    for (const auto& expected_port : expected_ports) {
      const auto& port_decl = port_declarations[i];
      const auto* identifier_leaf =
          GetIdentifierFromTaskFunctionPortItem(*port_decl.match);
      EXPECT_EQ(identifier_leaf->get().text(), expected_port.id)
          << "Failed code:\n"
          << code;
      const auto* port_type = GetTypeOfTaskFunctionPortItem(*port_decl.match);
      EXPECT_EQ(IsStorageTypeOfDataTypeSpecified(*ABSL_DIE_IF_NULL(port_type)),
                expected_port.have_type)
          << "Failed code:\n"
          << code;
      ++i;
    }
  }
}

TEST(GetAllPortReferences, GetPortReferenceIdentifier) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {""},
      {"module m(); endmodule: m"},
      {"module m(",
       {kTag, "a"},
       ", ",
       {kTag, "b"},
       ");\n input a, b; endmodule: m"},
      {"module m(input a,", {kTag, "b"}, "); endmodule: m"},
      {"module m(input a,", {kTag, "b"}, "[0:1]); endmodule: m"},
      {"module m(input wire a,", {kTag, "b"}, "[0:1]); endmodule: m"},
      {"module m(wire a,", {kTag, "b"}, "[0:1]); endmodule: m"},
  };
  for (const auto& test : kTestCases) {
    const absl::string_view code(test.code);
    VerilogAnalyzer analyzer(code, "test-file");
    const auto code_copy = analyzer.Data().Contents();
    ASSERT_OK(analyzer.Analyze()) << "failed on:\n" << code;
    const auto& root = analyzer.Data().SyntaxTree();

    const auto decls = FindAllPortReferences(*ABSL_DIE_IF_NULL(root));

    std::vector<TreeSearchMatch> types;
    for (const auto& decl : decls) {
      const auto* type = GetIdentifierFromPortReference(*decl.match);
      types.push_back(TreeSearchMatch{type, {/* ignored context */}});
    }

    std::ostringstream diffs;
    EXPECT_TRUE(test.ExactMatchFindings(types, code_copy, &diffs))
        << "failed on:\n"
        << code << "\ndiffs:\n"
        << diffs.str();
  }
}

}  // namespace
}  // namespace verilog
