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

#include "verilog/analysis/checkers/explicit_parameter_storage_type_rule.h"

#include <initializer_list>

#include "absl/status/status.h"
#include "absl/strings/match.h"
#include "common/analysis/linter_test_utils.h"
#include "common/analysis/syntax_tree_linter_test_utils.h"
#include "gtest/gtest.h"
#include "verilog/analysis/verilog_analyzer.h"
#include "verilog/parser/verilog_token_enum.h"

namespace verilog {
namespace analysis {
namespace {

using verible::LintTestCase;
using verible::RunConfiguredLintTestCases;
using verible::RunLintTestCases;

TEST(ExplicitParameterStorageTypeRuleTest, Configuration) {
  ExplicitParameterStorageTypeRule rule;
  absl::Status status;
  EXPECT_TRUE((status = rule.Configure("")).ok()) << status.message();
  EXPECT_TRUE((status = rule.Configure("exempt_type:string")).ok())
      << status.message();

  EXPECT_FALSE((status = rule.Configure("foo:string")).ok());
  EXPECT_TRUE(absl::StrContains(status.message(), "supported parameter"));

  EXPECT_FALSE((status = rule.Configure("exempt_type:int")).ok());
  EXPECT_EQ(status.message(),
            "exempt_type: Value can only be one of ['', 'string']; got 'int'");
}

// Tests that ExplicitParameterStorageTypeRule correctly accepts
// parameters/localparams with explicitly defined storage types.
TEST(ExplicitParameterStorageTypeRuleTest, AcceptTests) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {""},
      {"module foo; endmodule"},
      {"parameter int Bar = 1;"},
      {"parameter signed Bar = 1;"},
      {"parameter unsigned Bar = 1;"},
      {"parameter int unsigned Bar = 1;"},
      {"package foo; parameter int Bar = 1; endpackage"},
      {"package foo; parameter type Bar = 1; endpackage"},
      {"module foo; localparam bit Bar = 1; endmodule"},
      {"module foo; localparam int signed Bar = 1; endmodule"},
      {"module foo #(parameter int Bar = 1); endmodule"},
      {"module foo #(parameter signed Bar = 1); endmodule"},
      {"module foo #(parameter int signed Bar = 1); endmodule"},
      {"module foo #(parameter Other_t Bar = 1); endmodule"},
      {"module foo #(parameter mypkg::Other_t Bar = mypkg::N); endmodule"},
      {"class foo; localparam logic Bar = 1; endclass"},
      {"class foo; parameter string Bar = \"Bar\"; endclass"},
      {"class foo #(parameter int Bar = 1); endclass"},
      {"class foo #(parameter Other_t Bar = 1); endclass"},
      {"class foo #(parameter mypkg::Other_t Bar = mypkg::N); endclass"},
  };
  RunLintTestCases<VerilogAnalyzer, ExplicitParameterStorageTypeRule>(
      kTestCases);
}

// Tests that ExplicitParameterStorageTypeRule rejects parameters/localparams
// that have not explicitly defined a storage type.
TEST(ExplicitParameterStorageTypeRuleTest, RejectTests) {
  constexpr int kToken = SymbolIdentifier;
  const std::initializer_list<LintTestCase> kTestCases = {
      {"parameter ", {kToken, "Bar"}, " = 1;"},
      {"package foo; parameter ", {kToken, "Bar"}, " = 1; endpackage"},
      {"module foo; localparam ", {kToken, "Bar"}, " = 1; endmodule"},
      {"class foo; localparam ", {kToken, "Bar"}, " = 1; endclass"},
      {"class foo; parameter ", {kToken, "Bar"}, " = 1; endclass"},
      {"module foo #(parameter ", {kToken, "Bar"}, " = 1); endmodule"},
  };
  RunLintTestCases<VerilogAnalyzer, ExplicitParameterStorageTypeRule>(
      kTestCases);
}

TEST(ExplicitParameterStorageTypeRuleTest, AcceptConfiguredStringExemption) {
  constexpr int kToken = SymbolIdentifier;
  // No configuration; none of these is allowed.
  {
    const std::initializer_list<LintTestCase> kTestCases = {
        {"parameter ", {kToken, "Bar"}, " = \"Bar\";"},
        {"parameter ", {kToken, "Baz"}, " = 42;"},
    };
    RunConfiguredLintTestCases<VerilogAnalyzer,
                               ExplicitParameterStorageTypeRule>(kTestCases,
                                                                 "");
  }

  // Configured to allow string exemption
  {
    const std::initializer_list<LintTestCase> kTestCases = {
        {"parameter Bar = \"Bar\";"},               // string: ok
        {"parameter ", {kToken, "Baz"}, " = 42;"},  // int: still not ok
    };
    RunConfiguredLintTestCases<VerilogAnalyzer,
                               ExplicitParameterStorageTypeRule>(
        kTestCases, "exempt_type:string");
  }
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
