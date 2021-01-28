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

#include "verilog/preprocessor/verilog_preprocess.h"

#include <map>
#include <vector>

#include "absl/status/status.h"
#include "common/text/macro_definition.h"
#include "common/text/token_info.h"
#include "common/util/container_util.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "verilog/analysis/verilog_analyzer.h"

namespace verilog {
namespace {

using testing::ElementsAre;
using testing::Pair;
using verible::container::FindOrNull;

class PreprocessorTester {
 public:
  explicit PreprocessorTester(const char* text)
      : analyzer_(text, "<<inline-file>>"), status_() {
    status_ = analyzer_.Analyze();
  }

  const VerilogPreprocessData& PreprocessorData() const {
    return analyzer_.PreprocessorData();
  }

  const absl::Status& Status() const { return status_; }

  const VerilogAnalyzer& Analyzer() const { return analyzer_; }

 private:
  VerilogAnalyzer analyzer_;
  absl::Status status_;
};

struct FailTest {
  const char* input;
  int offset;
};

TEST(VerilogPreprocessTest, InvalidPreprocessorInputs) {
  const FailTest test_cases[] = {
      {"`define\n", 8},                      // unterminated macro definition
      {"\n\n`define\n", 10},                 // unterminated macro definition
      {"`define 789\n", 8},                  // expect identifier for macro name
      {"`define 789 non-sense\n", 8},        // expect identifier for macro name
      {"`define 789 \\\nnon-sense\n", 8},    // expect identifier for macro name
      {"`define FOO(\n", 13},                // unterminated parameter list
      {"`define FOO(234\n", 12},             // invalid parameter name
      {"`define FOO(234)\n", 12},            // invalid parameter name
      {"`define FOO(aaa\n", 16},             // unterminated parameter list
      {"`define FOO(aaa;\n", 15},            // bad parameter separator
      {"`define FOO(aaa bbb\n", 16},         // bad parameter separator
      {"`define FOO(aaa bbb)\n", 16},        // bad parameter separator
      {"`define FOO(aaa+bbb)\n", 15},        // bad parameter separator
      {"`define FOO(aaa.zzz\n", 15},         // bad parameter separator
      {"`define FOO(aaa.zzz)\n", 15},        // bad parameter separator
      {"`define FOO(aaa,\n", 17},            // unterminated parameter list
      {"`define FOO(aaa,)\n", 16},           // missing parameter name
      {"`define FOO(,,)\n", 12},             // missing parameter name
      {"`define FOO(aaa, 345)\n", 17},       // invalid parameter name
      {"`define FOO(aaa=\n", 17},            // unterminated default parameter
      {"`define FOO(aaa =\n", 18},           // unterminated default parameter
      {"`define FOO(aaa = 9\n", 20},         // expecting ',' or ')'
      {"`define FOO(aaa = 9, bbb =\n", 27},  // unterminated parameter list
      {"`define FOO(aa = 9, bb = 2\n", 27},  // expecting ',' or ')'
  };
  for (const auto& test_case : test_cases) {
    PreprocessorTester tester(test_case.input);
    EXPECT_FALSE(tester.Status().ok())
        << "Expected preprocess to fail on invalid input: \"" << test_case.input
        << "\"";
    const auto& rejected_tokens = tester.Analyzer().GetRejectedTokens();
    ASSERT_FALSE(rejected_tokens.empty())
        << "on invalid input: \"" << test_case.input << "\"";
    const int rejected_token_offset =
        rejected_tokens[0].token_info.left(tester.Analyzer().Data().Contents());
    EXPECT_EQ(rejected_token_offset, test_case.offset)
        << "on invalid input: \"" << test_case.input << "\"";
  }
}

// Verify that VerilogPreprocess works without any directives.
TEST(VerilogPreprocessTest, WorksWithoutDefinitions) {
  const char* test_cases[] = {
      "",
      "\n",
      "module foo;\nendmodule\n",
      "module foo(input x, output y);\nendmodule\n",
  };
  for (const auto& test_case : test_cases) {
    PreprocessorTester tester(test_case);
    EXPECT_TRUE(tester.Status().ok());
    const auto& definitions = tester.PreprocessorData().macro_definitions;
    EXPECT_TRUE(definitions.empty());
    EXPECT_TRUE(tester.PreprocessorData().errors.empty());
    EXPECT_TRUE(tester.Analyzer().GetRejectedTokens().empty());
  }
}

TEST(VerilogPreprocessTest, OneMacroDefinitionNoParamsNoValue) {
  const char* test_cases[] = {
      "`define FOOOO\n",
      "`define     FOOOO\n",
      "module foo;\nendmodule\n"
      "`define FOOOO\n",
      "`define FOOOO\n"
      "module foo;\nendmodule\n",
  };
  for (const auto& test_case : test_cases) {
    PreprocessorTester tester(test_case);
    EXPECT_TRUE(tester.Status().ok());
    EXPECT_TRUE(tester.PreprocessorData().errors.empty());
    EXPECT_TRUE(tester.Analyzer().GetRejectedTokens().empty());
    const auto& definitions = tester.PreprocessorData().macro_definitions;
    EXPECT_THAT(definitions, ElementsAre(Pair("FOOOO", testing::_)));
    auto macro = FindOrNull(definitions, "FOOOO");
    ASSERT_NE(macro, nullptr);
    EXPECT_EQ(macro->DefinitionText().text(), "");
    EXPECT_FALSE(macro->IsCallable());
    EXPECT_TRUE(macro->Parameters().empty());
  }
}

TEST(VerilogPreprocessTest, OneMacroDefinitionNoParamsSimpleValue) {
  PreprocessorTester tester(
      "module foo;\nendmodule\n"
      "`define FOOOO \"bar\"\n");
  const auto& definitions = tester.PreprocessorData().macro_definitions;
  EXPECT_TRUE(tester.Status().ok()) << "Unexpected analyzer failure.";
  EXPECT_TRUE(tester.PreprocessorData().errors.empty());
  EXPECT_TRUE(tester.Analyzer().GetRejectedTokens().empty());
  EXPECT_THAT(definitions, ElementsAre(Pair("FOOOO", testing::_)));
  auto macro = FindOrNull(definitions, "FOOOO");
  ASSERT_NE(macro, nullptr);
  EXPECT_EQ(macro->DefinitionText().text(), "\"bar\"");
  EXPECT_FALSE(macro->IsCallable());
  EXPECT_TRUE(macro->Parameters().empty());
}

TEST(VerilogPreprocessTest, OneMacroDefinitionOneParamWithValue) {
  PreprocessorTester tester(
      "module foo;\nendmodule\n"
      "`define FOOOO(x) (x+1)\n");
  const auto& definitions = tester.PreprocessorData().macro_definitions;
  EXPECT_TRUE(tester.Status().ok()) << "Unexpected analyzer failure.";
  EXPECT_TRUE(tester.PreprocessorData().errors.empty());
  EXPECT_TRUE(tester.Analyzer().GetRejectedTokens().empty());
  EXPECT_THAT(definitions, ElementsAre(Pair("FOOOO", testing::_)));
  auto macro = FindOrNull(definitions, "FOOOO");
  ASSERT_NE(macro, nullptr);
  EXPECT_EQ(macro->DefinitionText().text(), "(x+1)");
  EXPECT_TRUE(macro->IsCallable());
  const auto& params = macro->Parameters();
  EXPECT_EQ(params.size(), 1);
  const auto& param(params[0]);
  EXPECT_EQ(param.name.text(), "x");
  EXPECT_FALSE(param.HasDefaultText());
}

TEST(VerilogPreprocessTest, OneMacroDefinitionOneParamDefaultWithValue) {
  PreprocessorTester tester(
      "module foo;\nendmodule\n"
      "`define FOOOO(x=22) (x+3)\n");
  const auto& definitions = tester.PreprocessorData().macro_definitions;
  EXPECT_TRUE(tester.Status().ok()) << "Unexpected analyzer failure.";
  EXPECT_TRUE(tester.PreprocessorData().errors.empty());
  EXPECT_TRUE(tester.Analyzer().GetRejectedTokens().empty());
  EXPECT_THAT(definitions, ElementsAre(Pair("FOOOO", testing::_)));
  auto macro = FindOrNull(definitions, "FOOOO");
  ASSERT_NE(macro, nullptr);
  EXPECT_EQ(macro->DefinitionText().text(), "(x+3)");
  EXPECT_TRUE(macro->IsCallable());
  const auto& params = macro->Parameters();
  EXPECT_EQ(params.size(), 1);
  const auto& param(params[0]);
  EXPECT_EQ(param.name.text(), "x");
  EXPECT_TRUE(param.HasDefaultText());
  EXPECT_EQ(param.default_value.text(), "22");
}

TEST(VerilogPreprocessTest, TwoMacroDefinitions) {
  PreprocessorTester tester(
      "`define BAAAAR(y, z) (y*z)\n"
      "`define FOOOO(x=22) (x+3)\n");
  const auto& definitions = tester.PreprocessorData().macro_definitions;
  EXPECT_TRUE(tester.Status().ok()) << "Unexpected analyzer failure.";
  EXPECT_TRUE(tester.PreprocessorData().errors.empty());
  EXPECT_TRUE(tester.Analyzer().GetRejectedTokens().empty());
  EXPECT_THAT(definitions, ElementsAre(Pair("BAAAAR", testing::_),
                                       Pair("FOOOO", testing::_)));
  {
    auto macro = FindOrNull(definitions, "BAAAAR");
    ASSERT_NE(macro, nullptr);
    EXPECT_TRUE(macro->IsCallable());
    const auto& params = macro->Parameters();
    EXPECT_EQ(params.size(), 2);
  }
  {
    auto macro = FindOrNull(definitions, "FOOOO");
    ASSERT_NE(macro, nullptr);
    EXPECT_TRUE(macro->IsCallable());
    const auto& params = macro->Parameters();
    EXPECT_EQ(params.size(), 1);
  }
}

}  // namespace
}  // namespace verilog
