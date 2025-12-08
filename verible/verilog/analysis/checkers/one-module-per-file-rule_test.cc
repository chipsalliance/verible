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

#include "verible/verilog/analysis/checkers/one-module-per-file-rule.h"

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
using verible::RunLintTestCases;

// Expected token type of findings.
constexpr int kToken = SymbolIdentifier;

// Test that no violations are found without module declarations
TEST(OneModulePerFileRuleTest, NoModules) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {""},
      {"class c; endclass"},
      {"package q; endpackage"},
  };
  RunLintTestCases<VerilogAnalyzer, OneModulePerFileRule>(kTestCases, "");
}

// Test that sole module in file is not reported
TEST(OneModulePerFileRuleTest, OneModule) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {"module moo; endmodule"},
      {"class c; endclass\nmodule moo; endmodule"},
      {"package q; endpackage\nmodule moo; endmodule"},
  };
  RunLintTestCases<VerilogAnalyzer, OneModulePerFileRule>(kTestCases, "");
}

// Test that nested module declarations are not reported
TEST(OneModulePerFileRuleTest, NestedModules) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {"module foo;\n"
       "  module moo;\n"
       "  endmodule : moo\n"
       "endmodule : foo"},
      {"module foo;\n"
       "  module moo;\n"
       "  endmodule : moo\n"
       "  module roo;\n"
       "  endmodule : roo\n"
       "endmodule : foo"},
      {"module foo;\n"
       "  module moo;\n"
       "    module roo;\n"
       "    endmodule : roo\n"
       "  endmodule : moo\n"
       "endmodule : foo"},
  };
  RunLintTestCases<VerilogAnalyzer, OneModulePerFileRule>(kTestCases, "");
}

// Test that multiple module declarations are picked up
TEST(OneModulePerFileRuleTest, MultipleModules) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {"module foo; endmodule\n"
       "module ",
       {kToken, "moo"},
       "; endmodule"},
      {"class c; endclass\n"
       "module foo; endmodule\n"
       "module ",
       {kToken, "moo"},
       "; endmodule"},
      {"package q; endpackage\n"
       "module foo; endmodule\n"
       "module ",
       {kToken, "moo"},
       "; endmodule\n"},
  };
  RunLintTestCases<VerilogAnalyzer, OneModulePerFileRule>(kTestCases, "");
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
