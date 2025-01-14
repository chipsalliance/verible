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

#include "verible/verilog/CST/type.h"

#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "gtest/gtest.h"
#include "verible/common/analysis/syntax-tree-search-test-utils.h"
#include "verible/common/analysis/syntax-tree-search.h"
#include "verible/common/text/text-structure.h"
#include "verible/common/text/tree-utils.h"
#include "verible/common/util/logging.h"
#include "verible/verilog/CST/context-functions.h"
#include "verible/verilog/CST/declaration.h"
#include "verible/verilog/CST/expression.h"
#include "verible/verilog/CST/match-test-utils.h"
#include "verible/verilog/analysis/verilog-analyzer.h"

#undef ASSERT_OK
#define ASSERT_OK(value) ASSERT_TRUE((value).ok())

namespace verilog {
namespace {

using verible::SyntaxTreeSearchTestCase;
using verible::TextStructureView;
using verible::TreeSearchMatch;

// Tests

// Tests that the correct amount of node kDataType declarations are found.
TEST(FindAllDataTypeDeclarationsTest, BasicTests) {
  constexpr std::pair<std::string_view, int> kTestCases[] = {
      {"", 0},
      {"class foo; endclass", 0},
      {"function foo; endfunction", 1},
      {"function foo(int bar); endfunction", 2},
      {"function foo(bar); endfunction", 2},
      {"function foo(int foo, inout bar); endfunction", 3},
      {"function foo(bit foo, ref bar); endfunction", 3},

      {"task foo; endtask", 0},
      {"task foo(int bar); endtask", 1},
      {"task foo(bar); endtask", 1},
      {"task foo(int foo, inout bar); endtask", 2},
      {"task foo(bit foo, ref bar); endtask", 2},
  };
  for (const auto &test : kTestCases) {
    VerilogAnalyzer analyzer(test.first, "");
    ASSERT_OK(analyzer.Analyze());
    const auto &root = analyzer.Data().SyntaxTree();
    const auto data_type_declarations =
        FindAllDataTypeDeclarations(*ABSL_DIE_IF_NULL(root));
    EXPECT_EQ(data_type_declarations.size(), test.second);
  }
}

// Tests that the correct amount of kTypeDeclaration nodes
// are found.
TEST(FindAllTypeDeclarationsTest, BasicTests) {
  constexpr int kTag = 1;  // don't care
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {""},
      {{kTag, "typedef union { int a; bit [8:0] b; } name;"}},
      {{kTag, "typedef struct { int a; bit [8:0] b; } name;"}},
      {{kTag, "typedef enum { Idle, Busy } name;"}},
      {
          {kTag, "typedef union { int a; bit [8:0] b; } name;"},
          " ",
          {kTag, "typedef struct { int a; bit [8:0] b; } a_name;"},
          " ",
          {kTag, "typedef enum { Idle, Busy } another_name;"},
      },
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          return FindAllTypeDeclarations(*ABSL_DIE_IF_NULL(root));
        });
  }
}

// Tests that the correct amount of node kEnumType declarations
// are found.
TEST(FindAllEnumTypesTest, BasicTests) {
  constexpr int kTag = 1;  // don't care
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {""},
      {"typedef struct { int a; bit [8:0] flags; } name;"},
      {"typedef union { int a; bit [8:0] flags; } name;"},
      {"typedef ", {kTag, "enum { Red, Green, Blue }"}, " name;"},
      {"typedef ",
       {kTag, "enum { Red, Green, Blue }"},
       " name; "
       "typedef ",
       {kTag, "enum { Yellow, Orange, Violet }"},
       " other_name;"},
      {"typedef ",
       {kTag, "enum { Idle, Busy }"},
       " name; "
       "typedef struct { int c; bit [8:0] d; } other_name;"},
      {"typedef union { int a; bit [8:0] b; } name; "
       "typedef ",
       {kTag, "enum { Oak, Larch }"},
       " other_name;"},
      {"typedef struct { int a; bit [8:0] b; } name; "
       "typedef union { int c; bit [8:0] d; } other_name; "
       "typedef ",
       {kTag, "enum { Idle, Busy }"},
       " another_name;"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          return FindAllEnumTypes(*ABSL_DIE_IF_NULL(root));
        });
  }
}

// Tests that the correct amount of node kStructType declarations
// are found.
TEST(FindAllStructTypesTest, BasicTests) {
  constexpr int kTag = 1;  // don't care
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {""},
      {"typedef enum { aVal, bVal } name;"},
      {"typedef union { int a; bit [8:0] flags; } name;"},
      {"typedef ", {kTag, "struct { int a; bit [8:0] b; }"}, " name;"},
      {"typedef ",
       {kTag, "struct { int a; bit [8:0] b; }"},
       " name; "
       "typedef ",
       {kTag, "struct { int c; bit [8:0] d; }"},
       " other_name;"},
      {"typedef union { int a; bit [8:0] b; } name; "
       "typedef ",
       {kTag, "struct { int c; bit [8:0] d; }"},
       " other_name;"},
      {"typedef union { int a; bit [8:0] b; } name; "
       "typedef ",
       {kTag, "struct { int c; bit [8:0] d; }"},
       " other_name; "
       "typedef enum { Idle, Busy } another_name;"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          return FindAllStructTypes(*ABSL_DIE_IF_NULL(root));
        });
  }
}

// Tests that the correct amount of node kUnionType declarations
// are found.
TEST(FindAllUnionTypesTest, BasicTests) {
  constexpr int kTag = 1;  // don't care
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {""},
      {"typedef enum { aVal, bVal } name;"},
      {"typedef struct { int a; bit [8:0] flags; } name;"},
      {"typedef ", {kTag, "union { int a; bit [8:0] b; }"}, " name;"},
      {"typedef ",
       {kTag, "union { int a; bit [8:0] b; }"},
       " name; "
       "typedef ",
       {kTag, "union { int c; bit [8:0] d; }"},
       " other_name;"},
      {"typedef ",
       {kTag, "union { int a; bit [8:0] b; }"},
       " name; "
       "typedef struct { int c; bit [8:0] d; } other_name;"},
      {"typedef struct { int a; bit [8:0] b; } name; "
       "typedef ",
       {kTag, "union { int c; bit [8:0] d; }"},
       " other_name; "
       "typedef enum { Idle, Busy } another_name;"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          return FindAllUnionTypes(*ABSL_DIE_IF_NULL(root));
        });
  }
}

// Tests that IsStorageTypeOfDataTypeSpecified correctly returns true if the
// node kDataType has declared a storage type.
TEST(IsStorageTypeOfDataTypeSpecifiedTest, AcceptTests) {
  constexpr std::pair<std::string_view, int> kTestCases[] = {
      {"function foo(int bar); endfunction", 2},
      {"function foo(int foo, bit bar); endfunction", 3},
      {"function foo (bit [10:0] bar); endfunction", 2},
      {"task foo(int bar); endtask", 1},
      {"task foo(int foo, bit bar); endtask", 2},
  };
  for (const auto &test : kTestCases) {
    VerilogAnalyzer analyzer(test.first, "");
    ASSERT_OK(analyzer.Analyze());
    const auto &root = analyzer.Data().SyntaxTree();
    const auto data_type_declarations = FindAllDataTypeDeclarations(*root);
    for (const auto &data_type : data_type_declarations) {
      // Only check the node kDataTypes within a node kPortList.
      if (analysis::ContextIsInsideTaskFunctionPortList(data_type.context)) {
        EXPECT_TRUE(IsStorageTypeOfDataTypeSpecified(*(data_type.match)))
            << "Input code:\n"
            << test.first;
      }
    }
  }
}

// Tests that IsStorageTypeOfDataTypeSpecified correctly returns false if the
// node kDataType has not declared a storage type.
TEST(IsStorageTypeOfDataTypeSpecifiedTest, RejectTests) {
  constexpr std::string_view kTestCases[] = {
      {"function foo (bar); endfunction"},
      {"function foo(foo, ref bar); endfunction"},
      {"function foo(input foo, inout bar); endfunction"},
      {"task foo(bar); endtask"},
      {"task foo(foo, input bar); endtask"},
      {"task foo(input foo, inout bar); endtask"},
  };
  for (const auto &test : kTestCases) {
    VerilogAnalyzer analyzer(test, "");
    ASSERT_OK(analyzer.Analyze());
    const auto &root = analyzer.Data().SyntaxTree();
    const auto data_type_declarations = FindAllDataTypeDeclarations(*root);
    for (const auto &data_type : data_type_declarations) {
      // Only check the node kDataTypes within a node kPortList.
      if (analysis::ContextIsInsideTaskFunctionPortList(data_type.context)) {
        EXPECT_FALSE(IsStorageTypeOfDataTypeSpecified(*(data_type.match)));
      }
    }
  }
}

TEST(GetIdentifierFromTypeDeclarationTest, TypedefNames) {
  constexpr int kTag = 1;  // don't care
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {"typedef struct { int a; bit [8:0] b; } ", {kTag, "bar"}, ";"},
      {"typedef struct { int a; bit [8:0] b; } ", {kTag, "b_a_r"}, ";"},
      {"typedef struct { int a; bit [8:0] b; } ", {kTag, "hello_world"}, ";"},
      {"typedef struct { int a; bit [8:0] b; } ", {kTag, "hello_world1"}, ";"},
      {"typedef struct { int a; bit [8:0] b; } ", {kTag, "bar2"}, ";"},
      {"typedef struct { int a; } ", {kTag, "name"}, ";"},
      {"typedef union { int a; bit [8:0] b; } ", {kTag, "bar"}, ";"},
      {"typedef union { int a; bit [8:0] b; } ", {kTag, "b_a_r"}, ";"},
      {"typedef union { int a; bit [8:0] b; } ", {kTag, "hello_world"}, ";"},
      {"typedef union { int a; bit [8:0] b; } ", {kTag, "hello_world1"}, ";"},
      {"typedef union { int a; bit [8:0] b; } ", {kTag, "bar2"}, ";"},
      {"typedef union { int a; } ", {kTag, "name"}, ";"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          const auto type_declarations = FindAllTypeDeclarations(*root);
          std::vector<TreeSearchMatch> ids;
          ids.reserve(type_declarations.size());
          for (const auto &decl : type_declarations) {
            ids.push_back(TreeSearchMatch{
                GetIdentifierFromTypeDeclaration(*decl.match),
                /* no context */});
          }
          return ids;
        });
  }
}

TEST(GetVariableDeclaration, FindPackedDimensionFromDataDeclaration) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {""},
      {"module m;\nendmodule\n"},
      {"module m;\n string ",
       {kTag, "[x:y]"},
       "s;\nint ",
       {kTag, "[k:y]"},
       " v1;\n logic ",
       {kTag, "[k:y]"},
       "v2, v3;\nendmodule"},
      {"class m;\n int ",
       {kTag, "[k:y]"},
       " v1;\n logic ",
       {kTag, "[k:y]"},
       " v2, v3;\nendclass"},
      {"package m;\n int ",
       {kTag, "[k:y]"},
       " v1 = 2;\n logic ",
       {kTag, "[k:y]"},
       " v2 = 2;\nendpackage"},
      {"function m();\n int ",
       {kTag, "[k:y]"},
       " v1;\n logic ",
       {kTag, "[k:y]"},
       " v2, v3;\nendfunction"},
      {"package m;\n int ",
       {kTag, "[k:y]"},
       " v1 [x:y] = 2;\n logic ",
       {kTag, "[k:y]"},
       " v2 [x:y] = 2;\nendpackage"},
      {"function m();\n int ",
       {kTag, "[k:y]"},
       " v1 [x:y];\n logic ",
       {kTag, "[k:y]"},
       " v2 [x:y], v3 [x:y];\nendfunction"},
      {"class c;\n class_type x;\nendclass"},
  };
  for (const auto &test : kTestCases) {
    VLOG(1) << "code:\n" << test.code;
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          const auto &decls = FindAllDataDeclarations(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> packed_dimensions;
          for (const auto &decl : decls) {
            VLOG(1) << "decl: " << verible::StringSpanOfSymbol(*decl.match);
            const auto *packed_dimension =
                GetPackedDimensionFromDataDeclaration(*decl.match);
            if (packed_dimension == nullptr) continue;
            if (packed_dimension->empty()) continue;
            packed_dimensions.emplace_back(
                TreeSearchMatch{packed_dimension, {/* ignored context */}});
          }
          return packed_dimensions;
        });
  }
}

TEST(GetType, GetStructOrUnionOrEnumType) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {""},
      {"typedef ", {kTag, "struct { int a; bit [8:0] b; }"}, " name;"},
      {"typedef ",
       {kTag, "struct { int a; bit [8:0] b; }"},
       " name;\ntypedef ",
       {kTag, "struct { int c; bit [8:0] d; }"},
       " other_name;"},
      {"typedef ",
       {kTag, "union { int a; bit [8:0] b; }"},
       " name;\ntypedef ",
       {kTag, "struct { int c; bit [8:0] d; }"},
       " other_name;"},
      {"typedef ",
       {kTag, "union { int a; bit [8:0] b; }"},
       " name;\ntypedef ",
       {kTag, "struct { int c; bit [8:0] d; }"},
       " other_name;\ntypedef ",
       {kTag, "enum { Idle, Busy }"},
       " another_name;"},
  };
  for (const auto &test : kTestCases) {
    VLOG(1) << "code:\n" << test.code;
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          const auto &types = FindAllTypeDeclarations(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> names;
          for (const auto &decl : types) {
            const auto *name = GetReferencedTypeOfTypeDeclaration(*decl.match);
            if (name == nullptr) continue;
            names.emplace_back(TreeSearchMatch{name, {/* ignored context */}});
          }
          return names;
        });
  }
}

TEST(GetTypeIdentifier, GetNameOfDataType) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {""},
      {"module m(logic x);\nendmodule"},
      {"module m(", {kTag, "bus"}, " x);\nendmodule"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          const auto &types =
              FindAllDataTypeDeclarations(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> names;
          for (const auto &decl : types) {
            const auto *name = GetTypeIdentifierFromDataType(*decl.match);
            if (name == nullptr) {
              continue;
            }
            names.emplace_back(TreeSearchMatch{name, {/* ignored context */}});
          }
          return names;
        });
  }
}

TEST(GetDataImplicitIdDimensions, GetTypeOfDataImplicitIdDimensions) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {""},
      {"module m(logic x);\nendmodule"},
      {"struct {struct {int x;} var2;} var1;"},
      {"struct {", {kTag, "my_type"}, " var2;} var1;"},
      {"union {union {int x;} var2;} var1;"},
      {"union {", {kTag, "my_type"}, " var2;} var1;"},
  };
  for (const auto &test : kTestCases) {
    VLOG(1) << "code:\n" << test.code;
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          const auto &types =
              FindAllDataTypeImplicitIdDimensions(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> inner_types;
          for (const auto &decl : types) {
            const auto *inner_type =
                GetNonprimitiveTypeOfDataTypeImplicitDimensions(*decl.match);
            if (inner_type == nullptr) {
              continue;
            }
            inner_types.emplace_back(
                TreeSearchMatch{inner_type, {/* ignored context */}});
          }
          return inner_types;
        });
  }
}

TEST(GetDataImplicitIdDimensions, GetNameOfDataImplicitIdDimensions) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {""},
      {"module m(logic x);\nendmodule"},
      {"struct {int ", {kTag, "xx"}, ";} var1;"},
      {"union {int ", {kTag, "xx"}, ";} var1;"},
      {"struct {int ",
       {kTag, "xx"},
       ";\n struct {int  ",
       {kTag, "xx"},
       ";} ",
       {kTag, "var2"},
       ";} var1;"},
      {"union {int ",
       {kTag, "xx"},
       ";\n struct {int  ",
       {kTag, "xx"},
       ";} ",
       {kTag, "var2"},
       ";} var1;"},
      {"union {int ", {kTag, "xx"}, ";} var1;"},
      {"typedef union {int ", {kTag, "xx"}, ";} var1;"},
      {"typedef struct {int ", {kTag, "xx"}, ";} var1;"},
      {"struct {some_type ", {kTag, "xx"}, ";} var1;"},
      {"union {some_type ", {kTag, "xx"}, ";} var1;"},
      {"struct {some_type ", {kTag, "xx"}, ";\nint ", {kTag, "yy"}, ";} var1;"},
      {"typedef struct{\nstruct{\nint ",
       {kTag, "yy"},
       ";\n} ",
       {kTag, "yy"},
       ";}\nfar;"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          const auto &types =
              FindAllDataTypeImplicitIdDimensions(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> names;
          for (const auto &decl : types) {
            const auto name =
                GetSymbolIdentifierFromDataTypeImplicitIdDimensions(
                    *decl.match);
            names.emplace_back(
                TreeSearchMatch{name.first, {/* ignored context */}});
          }
          return names;
        });
  }
}

TEST(GetEnumName, GetEnumNameIdentifier) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {""},
      {"module m;\nendmodule\n"},
      {"module m;\nenum {",
       {kTag, "AA"},
       ", ",
       {kTag, "BB"},
       "} enum_var;\n endmodule"},
      {"module m;\ntypedef enum {",
       {kTag, "AA"},
       ", ",
       {kTag, "BB"},
       "} enum_var;\nendmodule"},
      {"package m;\ntypedef enum {",
       {kTag, "AA"},
       ", ",
       {kTag, "BB"},
       "} enum_var;\nendpackage"},
      {"package m;\ntypedef enum {",
       {kTag, "AA"},
       ", ",
       {kTag, "BB"},
       "} enum_var;\nendpackage"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          const auto &instances = FindAllEnumNames(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> names;
          for (const auto &decl : instances) {
            const auto *name = GetSymbolIdentifierFromEnumName(*decl.match);
            names.emplace_back(TreeSearchMatch{name, {/* ignored context */}});
          }
          return names;
        });
  }
}

TEST(GetIdentifiersFromDataType, GetIdentifier) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {""},
      {"function foo();\n  ", {kTag, "test"}, " bar();\nendfunction"},
      {"function ", {kTag, "void"}, " foo();\nendfunction"},
      {"function foo();\n  ", {kTag, "test"}, " data = foo();\nendfunction"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          const auto &instances =
              FindAllDataTypeDeclarations(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> names;
          for (const auto &decl : instances) {
            const auto *name = GetIdentifiersFromDataType(*decl.match);
            names.emplace_back(TreeSearchMatch{name, {/* ignored context */}});
          }
          return names;
        });
  }
}

TEST(GetLocalRootFromReference, GetLocalRoot) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {"function foo();\n  ",
       {kTag, "x"},
       " = foo.foo();\n"
       "endfunction"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          const auto &instances =
              FindAllReferenceFullExpressions(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> names;
          for (const auto &decl : instances) {
            if (ReferenceIsSimpleIdentifier(*decl.match)) {
              const auto *name = GetLocalRootFromReference(*decl.match);
              names.emplace_back(
                  TreeSearchMatch{name, {/* ignored context */}});
            }
          }
          return names;
        });
  }
}
}  // namespace
}  // namespace verilog
