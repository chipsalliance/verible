// Copyright 2017-2026 The Verible Authors.
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

// Test cases in this file should be *insensitive* to wrapping penalties.
// Penalty-sensitive tests belong in formatter-tuning_test.cc.
// Thematic end-to-end cases live in sibling formatter_*_test.cc files.
// New GitHub-issue regressions belong in formatter_issue_regression_test.cc.

#include "verible/verilog/formatting/formatter.h"

#include <memory>
#include <sstream>
#include <string_view>

#include "absl/log/die_if_null.h"
#include "absl/status/status.h"
#include "gtest/gtest.h"
#include "verible/common/text/text-structure.h"
#include "verible/common/util/logging.h"
#include "verible/verilog/analysis/verilog-analyzer.h"
#include "verible/verilog/formatting/format-style.h"
#include "verible/verilog/formatting/formatter-test-utils.h"

namespace verilog {
namespace formatter {

// private, extern function in formatter.cc, directly tested here.
extern absl::Status VerifyFormatting(
    const verible::TextStructureView &text_structure,
    std::string_view formatted_output, std::string_view filename);

namespace {

static constexpr VerilogPreprocess::Config kDefaultPreprocess;

using absl::StatusCode;

TEST(VerifyFormattingTest, NoError) {
  const std::string_view code("class c;endclass\n");
  const std::unique_ptr<VerilogAnalyzer> analyzer =
      VerilogAnalyzer::AnalyzeAutomaticMode(code, "<file>", kDefaultPreprocess);
  const auto &text_structure = ABSL_DIE_IF_NULL(analyzer)->Data();
  const auto status = VerifyFormatting(text_structure, code, "<filename>");
  EXPECT_OK(status);
}

// Tests that un-lexable outputs are caught as errors.
TEST(VerifyFormattingTest, LexError) {
  const std::string_view code("class c;endclass\n");
  const std::unique_ptr<VerilogAnalyzer> analyzer =
      VerilogAnalyzer::AnalyzeAutomaticMode(code, "<file>", kDefaultPreprocess);
  const auto &text_structure = ABSL_DIE_IF_NULL(analyzer)->Data();
  const std::string_view bad_code("1class c;endclass\n");  // lexical error
  const auto status = VerifyFormatting(text_structure, bad_code, "<filename>");
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), StatusCode::kDataLoss);
}

// Tests that un-parseable outputs are caught as errors.
TEST(VerifyFormattingTest, ParseError) {
  const std::string_view code("class c;endclass\n");
  const std::unique_ptr<VerilogAnalyzer> analyzer =
      VerilogAnalyzer::AnalyzeAutomaticMode(code, "<file>", kDefaultPreprocess);
  const auto &text_structure = ABSL_DIE_IF_NULL(analyzer)->Data();
  const std::string_view bad_code("classc;endclass\n");  // syntax error
  const auto status = VerifyFormatting(text_structure, bad_code, "<filename>");
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), StatusCode::kDataLoss);
}

// Tests that lexical differences are caught as errors.
TEST(VerifyFormattingTest, LexicalDifference) {
  const std::string_view code("class c;endclass\n");
  const std::unique_ptr<VerilogAnalyzer> analyzer =
      VerilogAnalyzer::AnalyzeAutomaticMode(code, "<file>", kDefaultPreprocess);
  const auto &text_structure = ABSL_DIE_IF_NULL(analyzer)->Data();
  const std::string_view bad_code("class c;;endclass\n");  // different tokens
  const auto status = VerifyFormatting(text_structure, bad_code, "<filename>");
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), StatusCode::kDataLoss);
}

TEST(FormatterTest, FormatCustomStyleTest) {
  static constexpr FormatterTestCase kTestCases[] = {
      {"", ""},
      {"module m;wire w;endmodule\n",
       "module m;\n"
       "          wire w;\n"
       "endmodule\n"},
  };

  FormatStyle style;
  style.column_limit = 40;
  style.indentation_spaces = 10;  // unconventional indentation
  style.wrap_spaces = 4;
  for (const auto &test_case : kTestCases) {
    VLOG(1) << "code-to-format:\n" << test_case.input << "<EOF>";
    std::ostringstream stream;
    const auto status =
        FormatVerilog(test_case.input, "<filename>", style, stream);
    EXPECT_OK(status);
    EXPECT_EQ(stream.str(), test_case.expected) << "code:\n" << test_case.input;
  }
}

// Small smoke subset; thematic cases live in sibling formatter_*_test.cc files.
static constexpr FormatterTestCase kSmokeFormatterTestCases[] = {
    {"", ""},
    {"\n", "\n"},
    {"\n\n", "\n\n"},
    {"\t//comment\n", "//comment\n"},
    {"\t/*comment*/\n", "/*comment*/\n"},
    {"\t/*multi-line\ncomment*/\n", "/*multi-line\ncomment*/\n"},
};

TEST(FormatterEndToEndTest, SmokeFormatterTestCases) {
  RunFormatterTestCases40(kSmokeFormatterTestCases);
}

}  // namespace
}  // namespace formatter
}  // namespace verilog
