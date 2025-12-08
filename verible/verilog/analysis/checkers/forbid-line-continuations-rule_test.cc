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

#include "verible/verilog/analysis/checkers/forbid-line-continuations-rule.h"

#include <initializer_list>

#include "gtest/gtest.h"
#include "verible/common/analysis/linter-test-utils.h"
#include "verible/common/analysis/syntax-tree-linter-test-utils.h"
#include "verible/verilog/analysis/verilog-analyzer.h"
#include "verible/verilog/parser/verilog-token-enum.h"

namespace verilog {
namespace analysis {
namespace {

using verible::LintTestCase;
using verible::RunLintTestCases;

TEST(StringLiteralConcatenationTest, FunctionPass) {
  const std::initializer_list<LintTestCase> kStringLiteralTestCases = {
      {""},
      {"module m;\nendmodule\n"},
      {"module m;\n", "string tmp = {\"Humpty Dumpty sat on a wall.\",",
       "\"Humpty Dumpty had a great fall.\"};", "\nendmodule"},
      {"module m;\n", "string tmp = {\"Humpty Dumpty \\ sat on a wall.\",",
       "\"Humpty Dumpty had a great fall.\"};", "\nendmodule"},
      {"module m;\nstring x = 1 ? \"foo\" : \"bar\";\nendmodule"},
      {"module m;\n initial begin\n", "if(\"foo\" == \"foo\") begin\n",
       "$display(\"%s %s\", \"Correct\", \"string\");\n", "end\n",
       "end\nendmodule"},
  };

  RunLintTestCases<VerilogAnalyzer, ForbidLineContinuationsRule>(
      kStringLiteralTestCases);
}

TEST(StringLiteralConcatenationTest, FunctionFailures) {
  constexpr int kToken = TK_StringLiteral;
  const std::initializer_list<LintTestCase> kStringLiteralTestCases = {
      {"module m;\n",
       "string tmp=",
       {kToken,
        "\"Humpty Dumpty sat on a wall. \\\nHumpty Dumpty had a great fall.\""},
       ";",
       "\nendmodule"},
      {"module m;\n",
       "string tmp=",
       {kToken,
        "\"Humpty Dumpty discovers CRLF \\\r\nHumpty Dumpty CRinged.\""},
       ";",
       "\nendmodule"},
      {"module m;\n",
       "string tmp=",
       {kToken,
        "\"Humpty Dumpty sat on a wall. \\\n\\\nHumpty Dumpty had a great "
        "fall.\""},
       ";",
       "\nendmodule"},
      {"module m;\nstring x = 1 ?",
       {kToken, "\"foo\\\nincorrect\""},
       " : \"bar\";\nendmodule"},
      {"module m;\nstring x = 1 ? \"foo\" : ",
       {kToken, "\"bar\\\nincorrect\""},
       ";\nendmodule"},
      {"module m;\n initial begin\n",
       "if(\"foo\" == \"foo\") begin\n",
       "$display(\"%s %s\", \"Incorrect\",",
       {kToken, "\"string\\\nspotted\""},
       ");\nend\n",
       "end\nendmodule"},
  };

  RunLintTestCases<VerilogAnalyzer, ForbidLineContinuationsRule>(
      kStringLiteralTestCases);
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
