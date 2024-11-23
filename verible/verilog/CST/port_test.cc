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

#include "verible/verilog/CST/port.h"

#include <initializer_list>
#include <memory>
#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "gtest/gtest.h"
#include "verible/common/analysis/syntax-tree-search-test-utils.h"
#include "verible/common/analysis/syntax-tree-search.h"
#include "verible/common/text/concrete-syntax-leaf.h"
#include "verible/common/text/syntax-tree-context.h"
#include "verible/common/text/text-structure.h"
#include "verible/common/text/token-info.h"
#include "verible/common/util/logging.h"
#include "verible/verilog/CST/match-test-utils.h"
#include "verible/verilog/CST/type.h"
#include "verible/verilog/CST/verilog-nonterminals.h"
#include "verible/verilog/analysis/verilog-analyzer.h"

#undef ASSERT_OK
#define ASSERT_OK(value) ASSERT_TRUE((value).ok())

namespace verilog {
namespace {

using verible::SyntaxTreeSearchTestCase;
using verible::TextStructureView;
using verible::TreeSearchMatch;

// Tests that no ports are found from an empty source.
TEST(FindAllPortDeclarationsTest, EmptySource) {
  VerilogAnalyzer analyzer("", "");
  ASSERT_OK(analyzer.Analyze());
  const auto &root = analyzer.Data().SyntaxTree();
  const auto port_declarations =
      FindAllPortDeclarations(*ABSL_DIE_IF_NULL(root));
  EXPECT_TRUE(port_declarations.empty());
}

// Tests that no ports are found in port-less module.
TEST(FindAllPortDeclarationsTest, NonPort) {
  VerilogAnalyzer analyzer("module foo; endmodule", "");
  ASSERT_OK(analyzer.Analyze());
  const auto &root = analyzer.Data().SyntaxTree();
  const auto port_declarations =
      FindAllPortDeclarations(*ABSL_DIE_IF_NULL(root));
  EXPECT_TRUE(port_declarations.empty());
}

// Tests that a package-item net declaration is not a port.
TEST(FindAllPortDeclarationsTest, OneWire) {
  VerilogAnalyzer analyzer("wire w;", "");
  ASSERT_OK(analyzer.Analyze());
  const auto &root = analyzer.Data().SyntaxTree();
  const auto port_declarations =
      FindAllPortDeclarations(*ABSL_DIE_IF_NULL(root));
  EXPECT_TRUE(port_declarations.empty());
}

// Tests that a local wire inside a module is not a port.
TEST(FindAllPortDeclarationsTest, OneWireInModule) {
  VerilogAnalyzer analyzer("module m; wire w; endmodule", "");
  ASSERT_OK(analyzer.Analyze());
  const auto &root = analyzer.Data().SyntaxTree();
  const auto port_declarations =
      FindAllPortDeclarations(*ABSL_DIE_IF_NULL(root));
  EXPECT_TRUE(port_declarations.empty());
}

// Tests that a port wire inside a module is found.
TEST(FindAllPortDeclarationsTest, OnePortInModule) {
  const char *kTestCases[] = {
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
    const auto &root = analyzer.Data().SyntaxTree();
    const auto port_declarations =
        FindAllPortDeclarations(*ABSL_DIE_IF_NULL(root));
    EXPECT_EQ(port_declarations.size(), 1);
    const auto &decl = port_declarations.front();
    EXPECT_TRUE(decl.context.IsInside(NodeEnum::kModuleDeclaration));
  }
}

TEST(GetIdentifierFromPortDeclarationTest, VariousPorts) {
  constexpr int kTag = 1;  // don't care
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {"module foo(input ", {kTag, "bar"}, "); endmodule"},
      {"module foo(input logic ", {kTag, "b_a_r"}, "); endmodule"},
      {"module foo(input wire ", {kTag, "hello_world"}, " = 1); endmodule"},
      {"module foo(wire ", {kTag, "hello_world1"}, " = 1); endmodule"},
      {"module foo(input logic [3:0] ", {kTag, "bar2"}, "); endmodule"},
      {"module foo(input logic ", {kTag, "b_a_r"}, " [3:0]); endmodule"},
      {"module foo(input logic ", {kTag, "bar"}, " [4]); endmodule"},
      // multiple ports
      {"module foo(input ",
       {kTag, "bar"},
       ", output ",
       {kTag, "bar2"},
       "); endmodule"},
      {"module foo(input logic ",
       {kTag, "bar"},
       ", input wire ",
       {kTag, "bar2"},
       "); endmodule"},
      {"module foo(input logic ",
       {kTag, "bar"},
       ", output ",
       {kTag, "bar2"},
       "); endmodule"},
      {"module foo(wire ",
       {kTag, "bar"},
       ", wire ",
       {kTag, "bar2"},
       " = 1); endmodule"},
      {"module foo(input logic [3:0] ",
       {kTag, "bar"},
       ", input logic ",
       {kTag, "bar2"},
       " [4]); endmodule"},
      {"module foo(input logic ",
       {kTag, "bar"},
       " [3:0], input logic [3:0] ",
       {kTag, "bar2"},
       "); endmodule"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          const auto port_declarations = FindAllPortDeclarations(*root);
          std::vector<TreeSearchMatch> ids;
          for (const auto &port : port_declarations) {
            const auto *identifier_leaf =
                GetIdentifierFromPortDeclaration(*port.match);
            ids.push_back(TreeSearchMatch{identifier_leaf, /* no context */});
          }
          return ids;
        });
  }
}

// Tests that no ports are found from an empty source.
TEST(FindAllModulePortDeclarationsTest, EmptySource) {
  VerilogAnalyzer analyzer("", "");
  ASSERT_OK(analyzer.Analyze());
  const auto &root = analyzer.Data().SyntaxTree();
  const auto port_declarations =
      FindAllModulePortDeclarations(*ABSL_DIE_IF_NULL(root));
  EXPECT_TRUE(port_declarations.empty());
}

// Tests that no ports are found in port-less module.
TEST(FindAllModulePortDeclarationsTest, NonPort) {
  VerilogAnalyzer analyzer("module foo; endmodule", "");
  ASSERT_OK(analyzer.Analyze());
  const auto &root = analyzer.Data().SyntaxTree();
  const auto port_declarations =
      FindAllModulePortDeclarations(*ABSL_DIE_IF_NULL(root));
  EXPECT_TRUE(port_declarations.empty());
}

// Tests that a package-item net declaration is not a port.
TEST(FindAllModulePortDeclarationsTest, OneWire) {
  VerilogAnalyzer analyzer("wire w;", "");
  ASSERT_OK(analyzer.Analyze());
  const auto &root = analyzer.Data().SyntaxTree();
  const auto port_declarations =
      FindAllModulePortDeclarations(*ABSL_DIE_IF_NULL(root));
  EXPECT_TRUE(port_declarations.empty());
}

// Tests that a local wire inside a module is not a port.
TEST(FindAllModulePortDeclarationsTest, OneWireInModule) {
  VerilogAnalyzer analyzer("module m; wire w; endmodule", "");
  ASSERT_OK(analyzer.Analyze());
  const auto &root = analyzer.Data().SyntaxTree();
  const auto port_declarations =
      FindAllModulePortDeclarations(*ABSL_DIE_IF_NULL(root));
  EXPECT_TRUE(port_declarations.empty());
}

// Tests that a port wire inside a module is found.
TEST(FindAllModulePortDeclarationsTest, OnePortInModule) {
  const char *kTestCases[] = {
      "input p",     "input [1:0] p",     "input p [0:1]",
      "input p [6]", "input [7:0] p [6]", "input wire p",
      "output p",    "output reg p",      "output reg [1:0] p",
  };
  for (auto test : kTestCases) {
    VerilogAnalyzer analyzer(absl::StrCat("module m(p); ", test, "; endmodule"),
                             "");
    ASSERT_OK(analyzer.Analyze());
    const auto &root = analyzer.Data().SyntaxTree();
    const auto port_declarations =
        FindAllModulePortDeclarations(*ABSL_DIE_IF_NULL(root));
    EXPECT_EQ(port_declarations.size(), 1);
    const auto &decl = port_declarations.front();
    EXPECT_TRUE(decl.context.IsInside(NodeEnum::kModuleDeclaration));
  }
}

TEST(GetIdentifierFromModulePortDeclarationTest, VariousPorts) {
  constexpr int kTag = 1;  // don't care
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {"module foo(bar); input ", {kTag, "bar"}, "; endmodule"},
      {"module foo(b_a_r); input logic ", {kTag, "b_a_r"}, "; endmodule"},
      {"module foo(bar2); input logic [3:0] ", {kTag, "bar2"}, "; endmodule"},
      {"module foo(b_a_r); input logic ", {kTag, "b_a_r"}, " [3:0]; endmodule"},
      {"module foo(bar); input logic ", {kTag, "bar"}, " [4]; endmodule"},
      // multiple ports
      {"module foo(bar, bar2); input ",
       {kTag, "bar"},
       "; output ",
       {kTag, "bar2"},
       "; endmodule"},
      {"module foo(bar, bar2); input logic ",
       {kTag, "bar"},
       "; input wire ",
       {kTag, "bar2"},
       "; endmodule"},
      {"module foo(bar, bar2); input logic ",
       {kTag, "bar"},
       "; output ",
       {kTag, "bar2"},
       "; endmodule"},
      {"module foo(bar, bar2); input logic [3:0] ",
       {kTag, "bar"},
       "; input logic ",
       {kTag, "bar2"},
       " [4]; endmodule"},
      {"module foo(bar, bar2); input logic ",
       {kTag, "bar"},
       " [3:0]; input logic [3:0] ",
       {kTag, "bar2"},
       "; endmodule"},
      {"module foo(bar, bar2); input logic ",
       {kTag, "bar"},
       " [3:0]; input reg [3:0] ",
       {kTag, "bar2"},
       "; endmodule"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          const auto port_declarations = FindAllModulePortDeclarations(*root);
          std::vector<TreeSearchMatch> ids;
          for (const auto &port : port_declarations) {
            const auto *identifier_leaf =
                GetIdentifierFromModulePortDeclaration(*port.match);
            ids.push_back(TreeSearchMatch{identifier_leaf, /* no context */});
          }
          return ids;
        });
  }
}

// Negative tests that no ports are found where they are not expected.
TEST(FindAllTaskFunctionPortDeclarationsTest, ExpectNoTaskFunctionPorts) {
  constexpr const char *kTestCases[] = {
      "",
      "module foo(input wire bar); endmodule",
      "function void foo(); endfunction",
      "task automatic foo(); endtask",
      "class foo; endclass",
      "class foo; function void bar(); endfunction endclass",
  };
  for (const auto *code : kTestCases) {
    VerilogAnalyzer analyzer(code, "<<inline-file>>");
    ASSERT_OK(analyzer.Analyze());
    const auto &root = analyzer.Data().SyntaxTree();
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
  for (const auto &test : kTestCases) {
    const std::string &code = test.code;
    const auto &expected_ports = test.expected_ports;
    VerilogAnalyzer analyzer(code, "<<inline-file>>");
    ASSERT_OK(analyzer.Analyze()) << "Failed code:\n" << code;
    const auto &root = analyzer.Data().SyntaxTree();
    const auto port_declarations =
        FindAllTaskFunctionPortDeclarations(*ABSL_DIE_IF_NULL(root));
    ASSERT_EQ(port_declarations.size(), expected_ports.size());

    // Compare expected port ids one-by-one, and check for type presence.
    int i = 0;
    for (const auto &expected_port : expected_ports) {
      const auto &port_decl = port_declarations[i];
      const auto *identifier_leaf =
          GetIdentifierFromTaskFunctionPortItem(*port_decl.match);
      EXPECT_EQ(identifier_leaf->get().text(), expected_port.id)
          << "Failed code:\n"
          << code;
      const auto *port_type = GetTypeOfTaskFunctionPortItem(*port_decl.match);
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
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();

          const auto decls = FindAllPortReferences(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> types;
          for (const auto &decl : decls) {
            const auto *type = GetIdentifierFromPortReference(
                *GetPortReferenceFromPort(*decl.match));
            types.push_back(TreeSearchMatch{type, {/* ignored context */}});
          }
          return types;
        });
  }
}

TEST(GetActualNamedPort, GetActualPortName) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {""},
      {"module m(input in1, input int2, input in3); endmodule: m\nmodule "
       "foo(input x, input y);\ninput in3;\nm m1(.",
       {kTag, "in1"},
       "(x), .",
       {kTag, "in2"},
       "(y), "
       ".",
       {kTag, "in3"},
       ");\nendmodule"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          const auto &ports = FindAllActualNamedPort(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> names;
          for (const auto &port : ports) {
            const auto *name = GetActualNamedPortName(*port.match);
            names.emplace_back(TreeSearchMatch{name, {/* ignored context */}});
          }
          return names;
        });
  }
}

TEST(GetActualNamedPort, GetActualNamedPortParenGroup) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {""},
      {"module m(input in1, input int2, input in3); endmodule: m\nmodule "
       "foo(input x, input y);\ninput in3;\nm m1(.in1",
       {kTag, "(x)"},
       ", .in2",
       {kTag, "(y)"},
       ", ",
       ".in3);\nendmodule"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          const auto &ports = FindAllActualNamedPort(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> paren_groups;
          for (const auto &port : ports) {
            const auto *paren_group = GetActualNamedPortParenGroup(*port.match);
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

TEST(FunctionPort, GetUnpackedDimensions) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {""},
      {"module m(input in1, input int2, input in3); endmodule: m"},
      {"function void f(int x", {kTag, "[s:g]"}, ");\nendfunction"},
      {"task f(int x", {kTag, "[s:g]"}, ");\nendtask"},
      {"task f(int x",
       {kTag, "[s:g]"},
       ",int y",
       {kTag, "[s:g]"},
       ");\nendtask"},
  };

  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          const auto &ports =
              FindAllTaskFunctionPortDeclarations(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> dimensions;
          for (const auto &port : ports) {
            const auto *dimension =
                GetUnpackedDimensionsFromTaskFunctionPortItem(*port.match);
            dimensions.emplace_back(
                TreeSearchMatch{dimension, {/* ignored context */}});
          }
          return dimensions;
        });
  }
}

TEST(FunctionPort, GetDirection) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {""},
      {"module m(", {kTag, "input"}, " name); endmodule;"},
      {"module m(", {kTag, "output"}, " name); endmodule;"},
      {"module m(", {kTag, "inout"}, " name); endmodule;"},
      {"module m(name); endmodule;"},
      {"module m(", {kTag, "input"}, "  logic name); endmodule;"},
      {"module m(", {kTag, "output"}, " logic name); endmodule;"},
      {"module m(", {kTag, "inout"}, " logic name); endmodule;"},
      {"module m(logic name); endmodule;"},
      {"module m(", {kTag, "input"}, " logic name, logic second); endmodule;"},
      {"module m(",
       {kTag, "output"},
       " a, ",
       {kTag, "input"},
       " b); endmodule;"},
  };

  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          const auto &ports = FindAllPortDeclarations(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> directions;
          for (const auto &port : ports) {
            const auto *direction =
                GetDirectionFromPortDeclaration(*port.match);
            directions.emplace_back(
                TreeSearchMatch{(const verible::Symbol *)direction, {}});
          }
          return directions;
        });
  }
}

}  // namespace
}  // namespace verilog
