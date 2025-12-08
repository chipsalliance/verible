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

#include "verible/verilog/analysis/checkers/posix-eof-rule.h"

#include <initializer_list>

#include "gtest/gtest.h"
#include "verible/common/analysis/linter-test-utils.h"
#include "verible/common/analysis/text-structure-linter-test-utils.h"
#include "verible/verilog/analysis/verilog-analyzer.h"
#include "verible/verilog/parser/verilog-token-enum.h"

namespace verilog {
namespace analysis {
namespace {

using verible::LintTestCase;
using verible::RunApplyFixCases;
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

TEST(PosixEOFRuleTest, ApplyAutoFix) {
  const std::initializer_list<verible::AutoFixInOut> kTestCases = {
      {"module m;\nendmodule", "module m;\nendmodule\n"},
  };
  RunApplyFixCases<VerilogAnalyzer, PosixEOFRule>(kTestCases, "");
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
