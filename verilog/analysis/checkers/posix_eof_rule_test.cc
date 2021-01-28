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

#include "verilog/analysis/checkers/posix_eof_rule.h"

#include <initializer_list>

#include "common/analysis/linter_test_utils.h"
#include "common/analysis/text_structure_linter_test_utils.h"
#include "common/text/symbol.h"
#include "gtest/gtest.h"
#include "verilog/analysis/verilog_analyzer.h"
#include "verilog/parser/verilog_token_enum.h"

namespace verilog {
namespace analysis {
namespace {

using verible::LintTestCase;
using verible::RunLintTestCases;

// Tests that compliant files are accepted.
TEST(PosixEOFRuleTest, AcceptsText) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {""},    // empty file is OK
      {"\n"},  // file with one line is OK
      {"\n\n"},
      {"module foo;\nendmodule\n"},
  };
  RunLintTestCases<VerilogAnalyzer, PosixEOFRule>(kTestCases);
}

// Tests that missing EOFs are caught by the checker.
TEST(PosixEOFRuleTest, RejectsText) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {"foo", {TK_OTHER, ""}},
      {"foo\nbar", {TK_OTHER, ""}},
  };
  RunLintTestCases<VerilogAnalyzer, PosixEOFRule>(kTestCases);
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
