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

#include "verilog/CST/DPI.h"

#include <utility>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "common/text/token_info_test_util.h"
#include "common/text/tree_utils.h"
#include "common/util/range.h"
#include "verilog/analysis/verilog_analyzer.h"

namespace verilog {
namespace {

TEST(FindAllDPIImportsTest, CountMatches) {
  constexpr std::pair<absl::string_view, int> kTestCases[] = {
      {"", 0},
      {"module m;\nendmodule\n", 0},
      {"class c;\nendclass\n", 0},
      {"function f;\nendfunction\n", 0},
      {"package p;\nendpackage\n", 0},
      {"task t;\nendtask\n", 0},
      {"module m;\n"
       "  function int add();\n"
       "  endfunction\n"
       "endmodule\n",
       0},
      {"module m;\n"
       "  import \"DPI-C\" function int add();\n"
       "endmodule\n",
       1},
      {"module m;\n"
       "  import \"DPI-C\" function int add();\n"
       "  foo bar();\n"
       "endmodule\n",
       1},
      {"module m;\n"
       "  import \"DPI-C\" function int add();\n"
       "  foo bar();\n"
       "  import \"DPI-C\" function int sub(input int x, y);\n"
       "endmodule\n",
       2},
  };
  for (const auto& test : kTestCases) {
    VerilogAnalyzer analyzer(test.first, "test-file");
    ASSERT_OK(analyzer.Analyze());
    const auto& root = analyzer.Data().SyntaxTree();
    const auto dpi_imports = FindAllDPIImports(*ABSL_DIE_IF_NULL(root));
    EXPECT_EQ(dpi_imports.size(), test.second);
  }
}

TEST(GetDPIImportPrototypeTest, Various) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::TokenInfoTestData kTestCases[] = {
      // each of these test cases should match exactly one DPI import
      {"module m;\n"
       "  import \"DPI-C\" ",
       {kTag, "function void foo;"},
       "\n"
       "endmodule\n"},
      {"module m;\n"
       "  wire w;\n"
       "  import \"DPI-C\" ",
       {kTag, "function void foo;"},
       "\n"
       "  logic l;\n"
       "endmodule\n"},
      {"module m;\n"
       "  import \"DPI-C\" ",
       {kTag, "function int add();"},
       "\n"
       "endmodule\n"},
      {"module m;\n"
       "  import   \"DPI-C\" ",
       {kTag, "function   int   add( input int x , y);"},
       "\n"
       "endmodule\n"},
  };
  for (const auto& test : kTestCases) {
    const absl::string_view code(test.code);
    VerilogAnalyzer analyzer(code, "test-file");
    const auto code_copy = analyzer.Data().Contents();
    ASSERT_OK(analyzer.Analyze()) << "failed on:\n" << code;
    const auto& root = analyzer.Data().SyntaxTree();

    const auto dpi_imports = FindAllDPIImports(*ABSL_DIE_IF_NULL(root));
    ASSERT_EQ(dpi_imports.size(), 1);
    const auto& dpi_import = *dpi_imports.front().match;
    const auto& prototype = GetDPIImportPrototype(dpi_import);
    const auto prototype_span = verible::StringSpanOfSymbol(prototype);
    // TODO(b/151371397): Refactor this test code along with
    // common/analysis/linter_test_util.h to be able to compare set-symmetric
    // differences in 'findings'.

    // Find the tokens, rebased into the other buffer.
    const auto expected_excerpts = test.FindImportantTokens(code_copy);
    ASSERT_EQ(expected_excerpts.size(), 1);
    // Compare the string_views and their exact spans.
    const auto expected_span = expected_excerpts.front().text;
    // analyzer made its own copy of code
    ASSERT_TRUE(verible::IsSubRange(prototype_span, code_copy));
    ASSERT_TRUE(verible::IsSubRange(expected_span, code_copy));
    EXPECT_EQ(prototype_span, expected_span);
    EXPECT_TRUE(verible::BoundsEqual(prototype_span, expected_span));
    // Can't use verible::BoundsEqual() to check ranges, because they belong to
    // different buffers, so we just leave the testing to string comparison.
  }
}

}  // namespace
}  // namespace verilog
