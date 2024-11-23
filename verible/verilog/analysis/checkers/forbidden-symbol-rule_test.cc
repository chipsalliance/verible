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

#include "verible/verilog/analysis/checkers/forbidden-symbol-rule.h"

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

// TODO(jeremycs): add examples of actual usage from code search
// `uvm_info("report_server",
//           $psprintf("Quit count reached: %0d",
//                      get_max_quit_count()), UVM_NONE)
// my_name = $psprintf("driver[%0d]", my_id);

constexpr int kToken = SystemTFIdentifier;

TEST(InvalidSystemTFTest, Psprintf) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {"class c; function f; ",
       {kToken, "$psprintf"},
       "(some, args);\n"
       "endfunction\nendclass\n"},
      {"class c; function f;\n",
       {kToken, "$psprintf"},
       "(some, args);\n",
       {kToken, "$psprintf"},
       "(\"%d\", value);\n"
       "endfunction\nendclass\n"},
  };

  RunLintTestCases<VerilogAnalyzer, ForbiddenSystemTaskFunctionRule>(
      kTestCases);
}

TEST(InvalidSystemTFTest, LintClean) {
  // {} implies that no lint errors are expected.
  const std::initializer_list<LintTestCase> kTestCases = {
      {"class c; function f; string a = \"$psprintf\"; endfunction endclass"},
      {"class c; function f; string a = $sformat(); endfunction endclass"},
      {"class c; function f; $display(\"foo\"); endfunction endclass"},
      {"class c; function f; foo(bar); //$psprintf\n"
       "endfunction endclass"},
      {"class c; function f; foo(bar); endfunction endclass"},
  };

  RunLintTestCases<VerilogAnalyzer, ForbiddenSystemTaskFunctionRule>(
      kTestCases);
}

TEST(InvalidSystemTFTest, Random) {
  const std::initializer_list<LintTestCase> kTestCases = {
      // $random function tests
      {"class c; function f;\n"
       "if ($value$plusargs(\"vera_random_seed=%d\",seed ))\n"
       "rand_num  = $urandom(seed) + ",
       {kToken, "$random"},
       ";\n"
       "endfunction endclass"},
      {"class c; function f;\n",
       "this_rand = {",
       {kToken, "$random"},
       "(seed_integer)} % 2;\n"
       "endfunction endclass"},
  };

  RunLintTestCases<VerilogAnalyzer, ForbiddenSystemTaskFunctionRule>(
      kTestCases);
}

TEST(InvalidSystemTFTest, Srandom) {
  const std::initializer_list<LintTestCase> kTestCases = {
      // $srandom function tests
      {"class c; function f;\n"
       "rand_num = ",
       {kToken, "$srandom"},
       "(seed, $urandom);\n"
       "endfunction endclass"},
      {"class c; function f;\n"
       "foo({",
       {kToken, "$srandom"},
       "(1, 1)} % 2);\n"
       "endfunction endclass\n"},
  };

  RunLintTestCases<VerilogAnalyzer, ForbiddenSystemTaskFunctionRule>(
      kTestCases);
}

TEST(InvalidSystemTFTest, Dist) {
  const std::initializer_list<LintTestCase> kTestCases = {
      // $dist_* function tests
      {"class c; function f;\n",
       {kToken, "$dist_chi_square"},
       "(a, b, c);"
       "endfunction endclass\n"},
      {"class c; function f;\n",
       {kToken, "$dist_erlang"},
       ";"
       "endfunction endclass\n"},
      {"class c; function f;\n",
       {kToken, "$dist_exponential"},
       "(a, b, c);"
       "endfunction endclass\n"},
      {"class c; function f;\n"
       "stall_count = ",
       {kToken, "$dist_normal"},
       "(seed, max, max) % max;"
       "endfunction endclass\n"},
      {"class c; function f;\n",
       {kToken, "$dist_poisson"},
       "(a, b, c);"
       "endfunction endclass\n"},
      {"class c; function f;\n",
       {kToken, "$dist_t"},
       "(a, b, c);"
       "endfunction endclass\n"},
      {"class c; function f;\n",
       {kToken, "$dist_uniform"},
       "(a, b, c);"
       "endfunction endclass\n"},
  };

  RunLintTestCases<VerilogAnalyzer, ForbiddenSystemTaskFunctionRule>(
      kTestCases);
}

TEST(InvalidSystemTFTest, MacroArg) {
  const std::initializer_list<LintTestCase> kTestCases = {
      // check for forbidden function calls inside macro call arguments
      {"class c; function f;\n"
       "string z = `FOO(",
       {kToken, "$psprintf"},
       "(some, args));\n"
       "endfunction endclass"},
      {"class c; function f;\n"
       "string z = `FOO(",
       {kToken, "$psprintf"},
       "(some, `BAR(123, ",
       {kToken, "$psprintf"},
       "())));\n"
       "endfunction endclass"},
  };

  RunLintTestCases<VerilogAnalyzer, ForbiddenSystemTaskFunctionRule>(
      kTestCases);
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
