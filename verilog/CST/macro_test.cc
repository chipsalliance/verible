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
#include "common/text/text_structure.h"
#include "verilog/analysis/verilog_analyzer.h"

namespace verilog {
namespace {

using ::testing::ElementsAreArray;

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
      found_names.push_back(GetMacroCallId(*match.match).text);
    }
    EXPECT_THAT(found_names, ElementsAreArray(test.expected_names))
        << "code:\n"
        << test.code;
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
  }
}

}  // namespace
}  // namespace verilog
