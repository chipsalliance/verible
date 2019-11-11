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

}  // namespace
}  // namespace verilog
