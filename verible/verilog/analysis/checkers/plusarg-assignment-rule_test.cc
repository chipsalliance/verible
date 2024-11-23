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

#include "verible/verilog/analysis/checkers/plusarg-assignment-rule.h"

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

TEST(PlusargAssignmentTest, TestPlusargs) {
  // {} implies that no lint errors are expected.
  constexpr int kToken = SystemTFIdentifier;
  const std::initializer_list<LintTestCase> kTestCases = {
      {""},
      {"class c; function f; endfunction endclass"},
      {"class c; function f; string a = \"$psprintf\"; endfunction endclass"},
      {"class c; function f; $test$plusarg(); endfunction endclass"},
      {"class c; function f; ",
       {kToken, "$test$plusargs"},
       "(some, args); endfunction endclass"},
  };

  RunLintTestCases<VerilogAnalyzer, PlusargAssignmentRule>(kTestCases);
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
