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

#include "verilog/CST/type.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "common/analysis/syntax_tree_search.h"
#include "common/text/text_structure.h"
#include "common/util/logging.h"
#include "verilog/CST/context_functions.h"
#include "verilog/analysis/verilog_analyzer.h"

#undef ASSERT_OK
#define ASSERT_OK(value) ASSERT_TRUE((value).ok())

namespace verilog {
namespace {

// Tests

// Tests that the correct amount of node kDataType declarations are found.
TEST(FindAllDataTypeDeclarationsTest, BasicTests) {
  const std::pair<std::string, int> kTestCases[] = {
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
  for (const auto& test : kTestCases) {
    VerilogAnalyzer analyzer(test.first, "");
    ASSERT_OK(analyzer.Analyze());
    const auto& root = analyzer.Data().SyntaxTree();
    const auto data_type_declarations =
        FindAllDataTypeDeclarations(*ABSL_DIE_IF_NULL(root));
    EXPECT_EQ(data_type_declarations.size(), test.second);
  }
}

// Tests that the correct amount of kTypeDeclaration nodes
// are found.
TEST(FindAllTypeDeclarationsTest, BasicTests) {
  const std::pair<std::string, int> kTestCases[] = {
      {"", 0},
      {"typedef union { int a; bit [8:0] b; } name;", 1},
      {"typedef struct { int a; bit [8:0] b; } name;", 1},
      {"typedef enum { Idle, Busy } name;", 1},
      {"typedef union { int a; bit [8:0] b; } name; "
       "typedef struct { int a; bit [8:0] b; } a_name; "
       "typedef enum { Idle, Busy } another_name;",
       3},
  };
  for (const auto& test : kTestCases) {
    VerilogAnalyzer analyzer(test.first, "");
    ASSERT_OK(analyzer.Analyze());
    const auto& root = analyzer.Data().SyntaxTree();
    const auto type_declarations =
        FindAllTypeDeclarations(*ABSL_DIE_IF_NULL(root));
    EXPECT_EQ(type_declarations.size(), test.second);
  }
}

// Tests that no enums are found in an empty source.
TEST(FindAllEnumTypesTest, EmptySource) {
  VerilogAnalyzer analyzer("", "");
  ASSERT_OK(analyzer.Analyze());
  const auto& root = analyzer.Data().SyntaxTree();
  const auto enum_declarations = FindAllEnumTypes(*ABSL_DIE_IF_NULL(root));
  EXPECT_TRUE(enum_declarations.empty());
}

// Tests that no enums are found in a struct declaration
TEST(FindAllEnumTypesTest, EnumSource) {
  VerilogAnalyzer analyzer("typedef struct { int a; bit [8:0] flags; } name;",
                           "");
  ASSERT_OK(analyzer.Analyze());
  const auto& root = analyzer.Data().SyntaxTree();
  const auto enum_declarations = FindAllEnumTypes(*ABSL_DIE_IF_NULL(root));
  EXPECT_TRUE(enum_declarations.empty());
}

// Tests that no enums are found in a union declaration
TEST(FindAllEnumTypesTest, StructSource) {
  VerilogAnalyzer analyzer("typedef union { int a; bit [8:0] flags; } name;",
                           "");
  ASSERT_OK(analyzer.Analyze());
  const auto& root = analyzer.Data().SyntaxTree();
  const auto enum_declarations = FindAllEnumTypes(*ABSL_DIE_IF_NULL(root));
  EXPECT_TRUE(enum_declarations.empty());
}

// Tests that the correct amount of node kEnumType declarations
// are found.
TEST(FindAllEnumTypesTest, BasicTests) {
  const std::pair<std::string, int> kTestCases[] = {
      {"", 0},
      {"typedef enum { Red, Green, Blue } name;", 1},
      {"typedef enum { Red, Green, Blue } name; "
       "typedef enum { Yellow, Orange, Violet } other_name;",
       2},
      {"typedef enum { Idle, Busy } name; "
       "typedef struct { int c; bit [8:0] d; } other_name;",
       1},
      {"typedef union { int a; bit [8:0] b; } name; "
       "typedef enum { Oak, Larch } other_name;",
       1},
      {"typedef struct { int a; bit [8:0] b; } name; "
       "typedef union { int c; bit [8:0] d; } other_name; "
       "typedef enum { Idle, Busy } another_name;",
       1},
  };
  for (const auto& test : kTestCases) {
    VerilogAnalyzer analyzer(test.first, "");
    ASSERT_OK(analyzer.Analyze());
    const auto& root = analyzer.Data().SyntaxTree();
    const auto enum_data_type_declarations =
        FindAllEnumTypes(*ABSL_DIE_IF_NULL(root));
    EXPECT_EQ(enum_data_type_declarations.size(), test.second);
  }
}

// Tests that no structs are found from an empty source.
TEST(FindAllStructTypesTest, EmptySource) {
  VerilogAnalyzer analyzer("", "");
  ASSERT_OK(analyzer.Analyze());
  const auto& root = analyzer.Data().SyntaxTree();
  const auto struct_declarations = FindAllStructTypes(*ABSL_DIE_IF_NULL(root));
  EXPECT_TRUE(struct_declarations.empty());
}

// Tests that no structs are found in an enum declaration
TEST(FindAllStructTypesTest, EnumSource) {
  VerilogAnalyzer analyzer("typedef enum { aVal, bVal } name;", "");
  ASSERT_OK(analyzer.Analyze());
  const auto& root = analyzer.Data().SyntaxTree();
  const auto struct_declarations = FindAllStructTypes(*ABSL_DIE_IF_NULL(root));
  EXPECT_TRUE(struct_declarations.empty());
}

// Tests that no structs are found in a union declaration
TEST(FindAllStructTypesTest, UnionSource) {
  VerilogAnalyzer analyzer("typedef union { int a; bit [8:0] flags; } name;",
                           "");
  ASSERT_OK(analyzer.Analyze());
  const auto& root = analyzer.Data().SyntaxTree();
  const auto struct_declarations = FindAllStructTypes(*ABSL_DIE_IF_NULL(root));
  EXPECT_TRUE(struct_declarations.empty());
}

// Tests that the correct amount of node kStructType declarations
// are found.
TEST(FindAllStructTypesTest, BasicTests) {
  const std::pair<std::string, int> kTestCases[] = {
      {"", 0},
      {"typedef struct { int a; bit [8:0] b; } name;", 1},
      {"typedef struct { int a; bit [8:0] b; } name; "
       "typedef struct { int c; bit [8:0] d; } other_name;",
       2},
      {"typedef union { int a; bit [8:0] b; } name; "
       "typedef struct { int c; bit [8:0] d; } other_name;",
       1},
      {"typedef union { int a; bit [8:0] b; } name; "
       "typedef struct { int c; bit [8:0] d; } other_name; "
       "typedef enum { Idle, Busy } another_name;",
       1},
  };
  for (const auto& test : kTestCases) {
    VerilogAnalyzer analyzer(test.first, "");
    ASSERT_OK(analyzer.Analyze());
    const auto& root = analyzer.Data().SyntaxTree();
    const auto struct_data_type_declarations =
        FindAllStructTypes(*ABSL_DIE_IF_NULL(root));
    EXPECT_EQ(struct_data_type_declarations.size(), test.second);
  }
}

// Tests that no unions are found from an empty source.
TEST(FindAllUnionTypesTest, EmptySource) {
  VerilogAnalyzer analyzer("", "");
  ASSERT_OK(analyzer.Analyze());
  const auto& root = analyzer.Data().SyntaxTree();
  const auto union_declarations = FindAllUnionTypes(*ABSL_DIE_IF_NULL(root));
  EXPECT_TRUE(union_declarations.empty());
}

// Tests that no unions are found in an enum declaration
TEST(FindAllUnionTypesTest, EnumSource) {
  VerilogAnalyzer analyzer("typedef enum { aVal, bVal } name;", "");
  ASSERT_OK(analyzer.Analyze());
  const auto& root = analyzer.Data().SyntaxTree();
  const auto union_declarations = FindAllUnionTypes(*ABSL_DIE_IF_NULL(root));
  EXPECT_TRUE(union_declarations.empty());
}

// Tests that no unions are found in a struct declaration
TEST(FindAllUnionTypesTest, StructSource) {
  VerilogAnalyzer analyzer("typedef struct { int a; bit [8:0] flags; } name;",
                           "");
  ASSERT_OK(analyzer.Analyze());
  const auto& root = analyzer.Data().SyntaxTree();
  const auto union_declarations = FindAllUnionTypes(*ABSL_DIE_IF_NULL(root));
  EXPECT_TRUE(union_declarations.empty());
}

// Tests that the correct amount of node kUnionType declarations
// are found.
TEST(FindAllUnionTypesTest, BasicTests) {
  const std::pair<std::string, int> kTestCases[] = {
      {"", 0},
      {"typedef union { int a; bit [8:0] b; } name;", 1},
      {"typedef union { int a; bit [8:0] b; } name; "
       "typedef union { int c; bit [8:0] d; } other_name;",
       2},
      {"typedef union { int a; bit [8:0] b; } name; "
       "typedef struct { int c; bit [8:0] d; } other_name;",
       1},
      {"typedef struct { int a; bit [8:0] b; } name; "
       "typedef union { int c; bit [8:0] d; } other_name; "
       "typedef enum { Idle, Busy } another_name;",
       1},
  };
  for (const auto& test : kTestCases) {
    VerilogAnalyzer analyzer(test.first, "");
    ASSERT_OK(analyzer.Analyze());
    const auto& root = analyzer.Data().SyntaxTree();
    const auto union_data_type_declarations =
        FindAllUnionTypes(*ABSL_DIE_IF_NULL(root));
    EXPECT_EQ(union_data_type_declarations.size(), test.second);
  }
}

// Tests that IsStorageTypeOfDataTypeSpecified correctly returns true if the
// node kDataType has declared a storage type.
TEST(IsStorageTypeOfDataTypeSpecifiedTest, AcceptTests) {
  const std::pair<std::string, int> kTestCases[] = {
      {"function foo(int bar); endfunction", 2},
      {"function foo(int foo, bit bar); endfunction", 3},
      {"function foo (bit [10:0] bar); endfunction", 2},
      {"task foo(int bar); endtask", 1},
      {"task foo(int foo, bit bar); endtask", 2},
  };
  for (const auto& test : kTestCases) {
    VerilogAnalyzer analyzer(test.first, "");
    ASSERT_OK(analyzer.Analyze());
    const auto& root = analyzer.Data().SyntaxTree();
    const auto data_type_declarations = FindAllDataTypeDeclarations(*root);
    for (const auto& data_type : data_type_declarations) {
      // Only check the node kDataTypes within a node kPortList.
      if (analysis::ContextIsInsideTaskFunctionPortList(data_type.context)) {
        EXPECT_TRUE(IsStorageTypeOfDataTypeSpecified(*(data_type.match)));
      }
    }
  }
}

// Tests that IsStorageTypeOfDataTypeSpecified correctly returns false if the
// node kDataType has not declared a storage type.
TEST(IsStorageTypeOfDataTypeSpecifiedTest, RejectTests) {
  const std::pair<std::string, int> kTestCases[] = {
      {"function foo (bar); endfunction", 2},
      {"function foo(foo, ref bar); endfunction", 3},
      {"function foo(input foo, inout bar); endfunction", 3},
      {"task foo(bar); endtask", 1},
      {"task foo(foo, input bar); endtask", 2},
      {"task foo(input foo, inout bar); endtask", 2},
  };
  for (const auto& test : kTestCases) {
    VerilogAnalyzer analyzer(test.first, "");
    ASSERT_OK(analyzer.Analyze());
    const auto& root = analyzer.Data().SyntaxTree();
    const auto data_type_declarations = FindAllDataTypeDeclarations(*root);
    for (const auto& data_type : data_type_declarations) {
      // Only check the node kDataTypes within a node kPortList.
      if (analysis::ContextIsInsideTaskFunctionPortList(data_type.context)) {
        EXPECT_FALSE(IsStorageTypeOfDataTypeSpecified(*(data_type.match)));
      }
    }
  }
}

TEST(GetIdentifierFromTypeDeclarationTest, StructIdentifiers) {
  const std::pair<std::string, absl::string_view> kTestCases[] = {
      {"typedef struct { int a; bit [8:0] b; } bar;", "bar"},
      {"typedef struct { int a; bit [8:0] b; } b_a_r;", "b_a_r"},
      {"typedef struct { int a; bit [8:0] b; } hello_world;", "hello_world"},
      {"typedef struct { int a; bit [8:0] b; } hello_world1;", "hello_world1"},
      {"typedef struct { int a; bit [8:0] b; } bar2;", "bar2"},
      {"typedef struct { int a; } name;", "name"},
  };
  for (const auto& test : kTestCases) {
    VerilogAnalyzer analyzer(test.first, "");
    ASSERT_OK(analyzer.Analyze());
    const auto& root = analyzer.Data().SyntaxTree();
    const auto type_declarations = FindAllTypeDeclarations(*root);
    const auto* identifier_leaf =
        GetIdentifierFromTypeDeclaration(*type_declarations.front().match);
    EXPECT_EQ(identifier_leaf->get().text(), test.second);
  }
}

TEST(GetIdentifierFromTypeDeclarationTest, UnionIdentifiers) {
  const std::pair<std::string, absl::string_view> kTestCases[] = {
      {"typedef union { int a; bit [8:0] b; } bar;", "bar"},
      {"typedef union { int a; bit [8:0] b; } b_a_r;", "b_a_r"},
      {"typedef union { int a; bit [8:0] b; } hello_world;", "hello_world"},
      {"typedef union { int a; bit [8:0] b; } hello_world1;", "hello_world1"},
      {"typedef union { int a; bit [8:0] b; } bar2;", "bar2"},
      {"typedef union { int a; } name;", "name"},
  };
  for (const auto& test : kTestCases) {
    VerilogAnalyzer analyzer(test.first, "");
    ASSERT_OK(analyzer.Analyze());
    const auto& root = analyzer.Data().SyntaxTree();
    const auto type_declarations = FindAllTypeDeclarations(*root);
    const auto* identifier_leaf =
        GetIdentifierFromTypeDeclaration(*type_declarations.front().match);
    EXPECT_EQ(identifier_leaf->get().text(), test.second);
  }
}

}  // namespace
}  // namespace verilog
