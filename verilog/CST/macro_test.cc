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

#include "verilog/CST/macro.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "common/analysis/syntax_tree_search.h"
#include "common/analysis/syntax_tree_search_test_utils.h"
#include "common/text/text_structure.h"
#include "common/text/token_info_test_util.h"
#include "common/util/range.h"
#include "verilog/CST/verilog_nonterminals.h"
#include "verilog/analysis/verilog_analyzer.h"

#undef EXPECT_OK
#define EXPECT_OK(value) EXPECT_TRUE((value).ok())

#undef ASSERT_OK
#define ASSERT_OK(value) ASSERT_TRUE((value).ok())

namespace verilog {
namespace {

using ::testing::ElementsAreArray;
using verible::SyntaxTreeSearchTestCase;
using verible::TreeSearchMatch;

struct FindAllTestCase {
  absl::string_view code;
  int expected_matches;
};

TEST(FindAllMacroCallsTest, Various) {
  const FindAllTestCase kTestCases[] = {
      {"", 0},
      {"module m; endmodule\n", 0},
      {"`FOO;\n", 0},
      {"`FOO()\n", 1},
      {"// `FOO()\n", 0},
      {"/* `FOO() */\n", 0},
      {"`FOO()\n`BAR()\n", 2},
      {"`FOO();\n", 1},
      {"`FOO();\n`BAR();\n", 2},
      {"`FOO(`BAR());\n", 2},  // nested
      {"`FOO(bar);\n", 1},
      {"`FOO(bar, 77);\n", 1},
      {"function f;\nf = foo(`FOO);\nendfunction\n", 0},
      {"function f;\nf = foo(`FOO());\nendfunction\n", 1},
      {"function f;\nf = `BAR(`FOO);\nendfunction\n", 1},
      {"function f;\nf = `BAR(`FOO());\nendfunction\n", 2},
      {"function f;\nf = `BAR() * `FOO();\nendfunction\n", 2},
  };
  for (const auto& test : kTestCases) {
    VerilogAnalyzer analyzer(test.code, "");
    EXPECT_OK(analyzer.Analyze());
    const auto& root = analyzer.Data().SyntaxTree();
    const auto macro_calls = FindAllMacroCalls(*ABSL_DIE_IF_NULL(root));
    EXPECT_EQ(macro_calls.size(), test.expected_matches) << "code:\n"
                                                         << test.code;
    // TODO(b/151371397): check exact substrings
  }
}

struct MatchIdTestCase {
  absl::string_view code;
  std::vector<absl::string_view> expected_names;
};

TEST(GetMacroCallIdsTest, Various) {
  const MatchIdTestCase kTestCases[] = {
      {"`FOO1()\n", {"`FOO1"}},
      {"`FOO2()\n`BAR2()\n", {"`FOO2", "`BAR2"}},
      {"`FOO3();\n", {"`FOO3"}},
      {"`FOO4();\n`BAR4();\n", {"`FOO4", "`BAR4"}},
      {"`FOO5(`BAR5());\n", {"`FOO5", "`BAR5"}},  // nested
      {"`FOO6(bar);\n", {"`FOO6"}},
      {"function f;\nf = foo(`FOO7());\nendfunction\n", {"`FOO7"}},
      {"function f;\nf = `BAR8(`FOO);\nendfunction\n", {"`BAR8"}},
      {"function f;\nf = `BAR9(`FOO9());\nendfunction\n", {"`BAR9", "`FOO9"}},
      {"function f;\nf = `BAR10() * `FOO10();\nendfunction\n",
       {"`BAR10", "`FOO10"}},
  };
  for (const auto& test : kTestCases) {
    VerilogAnalyzer analyzer(test.code, "");
    EXPECT_OK(analyzer.Analyze());
    const auto& root = analyzer.Data().SyntaxTree();
    const auto macro_calls = FindAllMacroCalls(*ABSL_DIE_IF_NULL(root));
    std::vector<absl::string_view> found_names;
    for (const auto& match : macro_calls) {
      found_names.push_back(GetMacroCallId(*match.match).text());
    }
    EXPECT_THAT(found_names, ElementsAreArray(test.expected_names))
        << "code:\n"
        << test.code;
    // TODO(b/151371397): check exact substrings
  }
}

struct CallArgsTestCase {
  absl::string_view code;
  bool expect_empty;
};

TEST(MacroCallArgsTest, Emptiness) {
  const CallArgsTestCase kTestCases[] = {
      // checks the number of call args of the first found macro call
      {"`FOO()\n", true},
      {"`FOO();\n", true},
      {"`FOO(`BAR());\n", false},  // nested
      {"`FOO(bar);\n", false},
      {"`FOO(bar, 77);\n", false},
      {"function f;\nf = foo(`FOO());\nendfunction\n", true},
      {"function f;\nf = `BAR(`FOO);\nendfunction\n", false},
      {"function f;\nf = `BAR(`FOO());\nendfunction\n", false},
      {"function f;\nf = `BAR() * `FOO();\nendfunction\n", true},
  };
  for (const auto& test : kTestCases) {
    VerilogAnalyzer analyzer(test.code, "");
    EXPECT_OK(analyzer.Analyze());
    const auto& root = analyzer.Data().SyntaxTree();
    const auto macro_calls = FindAllMacroCalls(*ABSL_DIE_IF_NULL(root));
    ASSERT_FALSE(macro_calls.empty());
    const auto& args = GetMacroCallArgs(*macro_calls.front().match);
    EXPECT_EQ(MacroCallArgsIsEmpty(args), test.expect_empty) << "code:\n"
                                                             << test.code;
    // TODO(b/151371397): check exact substrings
  }
}

TEST(GetFunctionFormalPortsGroupTest, WithFormalPorts) {
  constexpr int kTag = 1;  // value not important
  const verible::TokenInfoTestData kTestCases[] = {
      {{kTag, "`FOO"}, "\n"},
      {"package p;\n", {kTag, "`FOO"}, "\nendpackage\n"},
      {"class c;\n", {kTag, "`FOO"}, "\nendclass\n"},
      {"function f;\n", {kTag, "`FOO"}, "\nendfunction\n"},
      {"task t;\n", {kTag, "`FOO"}, "\nendtask\n"},
      {"module m;\n", {kTag, "`FOO"}, "\nendmodule\n"},
  };
  for (const auto& test : kTestCases) {
    const absl::string_view code(test.code);
    VerilogAnalyzer analyzer(code, "test-file");
    const absl::string_view code_copy(analyzer.Data().Contents());
    ASSERT_OK(analyzer.Analyze()) << "failed on:\n" << code;
    const auto& root = analyzer.Data().SyntaxTree();

    const auto macro_items = FindAllMacroGenericItems(*root);
    ASSERT_EQ(macro_items.size(), 1);
    const auto& macro_item = *macro_items.front().match;
    const auto& id = GetMacroGenericItemId(macro_item);
    const absl::string_view id_text = id.text();

    // TODO(b/151371397): Refactor this test code along with
    // common/analysis/linter_test_util.h to be able to compare set-symmetric
    // differences in 'findings'.

    // Find the tokens, rebased into the other buffer.
    const auto expected_excerpts = test.FindImportantTokens(code_copy);
    ASSERT_EQ(expected_excerpts.size(), 1);
    // Compare the string_views and their exact spans.
    const auto expected_span = expected_excerpts.front().text();
    ASSERT_TRUE(verible::IsSubRange(id_text, code_copy));
    ASSERT_TRUE(verible::IsSubRange(expected_span, code_copy));
    EXPECT_EQ(id_text, expected_span);
    EXPECT_TRUE(verible::BoundsEqual(id_text, expected_span));
  }
}

TEST(FindAllMacroDefinitions, MacroName) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {""},
      {"`define ", {kTag, "PRINT_STRING"}, "(str1) $display(\"%s\\n\", str1)"},
      {
          "`define ",
          {kTag, "PRINT_STRING"},
          "(str1) $display(\"%s\\n\", str1)\n",
          "`define ",
          {kTag, "PRINT_3_STRING"},
          R"((str1, str2, str3) \
    `PRINT_STRING(str1); \
    `PRINT_STRING(str2); \
    `PRINT_STRING(str3);)",
          "\n`define ",
          {kTag, "TEN"},
          " 10",
      },
      {"module m();\n `define ", {kTag, "my_macro"}, " 10\n endmodule"},
      {"class m;\n `define ", {kTag, "my_macro"}, " 10\n endclass"},
      {"function m();\n `define ", {kTag, "my_macro"}, " 10\n endfunction"},
      {"function int m();\n `define ",
       {kTag, "my_macro"},
       " 10\n return 1;\n endfunction"},
      {"task m();\n `define ", {kTag, "my_macro"}, " 10\n endtask"},
      {"package m;\n `define ", {kTag, "my_macro"}, " 10\n endpackage"},
      {"class m;\n `define ",
       {kTag, "my_macro"},
       " 10\n endclass\n module m(); int x = `TEN;\n endmodule"},
  };
  for (const auto& test : kTestCases) {
    const absl::string_view code(test.code);
    VerilogAnalyzer analyzer(code, "test-file");
    const auto code_copy = analyzer.Data().Contents();
    ASSERT_OK(analyzer.Analyze()) << "failed on:\n" << code;
    const auto& root = analyzer.Data().SyntaxTree();

    const auto decls = FindAllMacroDefinitions(*ABSL_DIE_IF_NULL(root));
    std::vector<TreeSearchMatch> names;
    for (const auto& decl : decls) {
      const auto& type = GetMacroName(*decl.match);
      names.push_back(TreeSearchMatch{&type, {/* ignored context */}});
    }

    std::ostringstream diffs;
    EXPECT_TRUE(test.ExactMatchFindings(names, code_copy, &diffs))
        << "failed on:\n"
        << code << "\ndiffs:\n"
        << diffs.str();
  }
}

TEST(FindAllMacroDefinitions, MacroArgsName) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {""},
      {"`define PRINT_STRING(", {kTag, "str1"}, ") $display(\"%s\\n\", str1)"},
      {
          "`define PRINT_STRING(",
          {kTag, "str1"},
          ") $display(\"%s\\n\", str1)\n",
          "`define PRINT_3_STRING(",
          {kTag, "str1"},
          ", ",
          {kTag, "str2"},
          ", ",
          {kTag, "str3"},
          ")",
          R"( \
    `PRINT_STRING(str1); \
    `PRINT_STRING(str2); \
    `PRINT_STRING(str3);)",
          "\n`define TEN 10",
      },
      {"module m();\n `define my_macro(", {kTag, "i"}, ") i\n endmodule"},
      {"class m;\n `define my_macro(", {kTag, "i"}, ") i\n endclass"},
      {"function m();\n `define my_macro(", {kTag, "i"}, ") i\n endfunction"},
      {"function int m();\n `define my_macro(",
       {kTag, "i"},
       ") i\n return 1;\n endfunction"},
      {"task m();\n `define my_macro(", {kTag, "i"}, ") i\n endtask"},
      {"package m;\n `define my_macro(", {kTag, "i"}, ") i\n endpackage"},
      {"class m;\n `define my_macro(",
       {kTag, "i"},
       ") i\n endclass\n module m(); int x = `TEN(1);\n endmodule"},
  };
  for (const auto& test : kTestCases) {
    const absl::string_view code(test.code);
    VerilogAnalyzer analyzer(code, "test-file");
    const auto code_copy = analyzer.Data().Contents();
    ASSERT_OK(analyzer.Analyze()) << "failed on:\n" << code;
    const auto& root = analyzer.Data().SyntaxTree();

    const auto decls = FindAllMacroDefinitions(*ABSL_DIE_IF_NULL(root));
    std::vector<TreeSearchMatch> names;
    for (const auto& decl : decls) {
      const auto& args = FindAllMacroDefinitionsArgs(*decl.match);
      for (const auto& arg : args) {
        const auto& name = GetMacroArgName(*arg.match);
        names.push_back(TreeSearchMatch{&name, {/* ignored context */}});
      }
    }

    std::ostringstream diffs;
    EXPECT_TRUE(test.ExactMatchFindings(names, code_copy, &diffs))
        << "failed on:\n"
        << code << "\ndiffs:\n"
        << diffs.str();
  }
}

}  // namespace
}  // namespace verilog
