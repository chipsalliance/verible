// Copyright 2017-2023 The Verible Authors.
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

#include "verible/verilog/analysis/checkers/dff-name-style-rule.h"

#include <initializer_list>
#include <optional>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/strings/string_view.h"
#include "gtest/gtest.h"
#include "verible/common/analysis/linter-test-utils.h"
#include "verible/common/analysis/syntax-tree-linter-test-utils.h"
#include "verible/verilog/analysis/verilog-analyzer.h"
#include "verible/verilog/parser/verilog-token-enum.h"

namespace verilog {
namespace analysis {
namespace {

using verible::LintTestCase;
using verible::RunConfiguredLintTestCases;
using verible::RunLintTestCases;

// Tests that DffNameStyleRule correctly accepts valid names.
TEST(DffNameStyleRuleTest, AcceptDefaults) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {"module m; always_ff @(posedge c) a_r <= a_next; endmodule"},
      {"module m; always_ff @(posedge c) a_r1 <= a_next; endmodule"},
      {"module m; always_ff @(posedge c) a_ff <= a_n; endmodule"},
      {"module m; always_ff @(posedge c) a_q <= a_d; endmodule"},
      {"module m; always_ff @(posedge c) if(!rst_ni) begin a_reg <= 0; end; "
       "endmodule"},
  };
  RunLintTestCases<VerilogAnalyzer, DffNameStyleRule>(kTestCases);
}

TEST(DffNameStyleRuleTest, RejectDefaultSuffixes) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {"module m; always_ff @(posedge c) begin ",
       {SymbolIdentifier, "a_n"},
       " <= b_n; end endmodule"},
      {"module m; always_ff @(posedge c) begin a_q <= ",
       {SymbolIdentifier, "b_q"},
       "; end endmodule"},
      // Should have same base ('a' != 'b')
      {"module m; always_ff @(posedge c) begin ",
       {SymbolIdentifier, "a_q"},
       " <= b_n; end endmodule"},
  };
  RunLintTestCases<VerilogAnalyzer, DffNameStyleRule>(kTestCases);
}

TEST(DffNameStyleRuleTest, AcceptTests) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {"module m; always_ff @(posedge c) a_ff <= a_next; endmodule"},
      {"module m; always_ff @(posedge c) someverylongname_ff <= "
       "someverylongname_next; endmodule"},
      {"module m; always_ff @(posedge c) if(!rst_ni) begin a_ff <= 0; end; "
       "endmodule"},
      {"module m; always_ff @(posedge c) if(!rst_ni) begin aAaAaAaAaAa_ff <= "
       "0; end endmodule"},
      {"module m; always_ff @(posedge c) a_a_a_a_a_a_ff <= a_a_a_a_a_a_next; "
       "endmodule"},
  };
  RunConfiguredLintTestCases<VerilogAnalyzer, DffNameStyleRule>(kTestCases,
                                                                "output:ff;"
                                                                "input:next");
}

TEST(DffNameStyleRuleTest, RejectTests) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {"module m; always_ff @(posedge c) a_ff <= ",
       {TK_DecNumber, "1"},
       "; endmodule"},
      {"module m; always_ff @(posedge c) a_ff <= ",
       {TK_LP, "'{"},
       "default: '0}; endmodule"},
      // '(' token value is its ASCII value (40 == '(')
      {"module m; always_ff @(posedge c) a_ff <= ",
       {'(', "("},
       "1 + 2); endmodule"},
  };
  RunConfiguredLintTestCases<VerilogAnalyzer, DffNameStyleRule>(kTestCases,
                                                                "output:ff");
}

TEST(DffNameStyleRuleTest, AcceptPipelineStages) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {"module m; always_ff @(posedge c) a_ff2 <= a_ff; endmodule"},
      {"module m; always_ff @(posedge c) a_ff2 <= a_ff1; endmodule"},
      {"module m; always_ff @(posedge c) a_ff3 <= a_ff2; endmodule"},
      {"module m; always_ff @(posedge c) a_ff4 <= a_ff3; endmodule"},
      {"module m; always_ff @(posedge c) a_ff40 <= a_ff39; endmodule"},
      {"module m; always_ff @(posedge c) if(!rst_ni) begin a_ff2 <= 0; end "
       "endmodule"},
      {"module m; always_ff @(posedge c) if(!rst_ni) begin a_ff3 <= 0; end "
       "endmodule"},
  };
  RunConfiguredLintTestCases<VerilogAnalyzer, DffNameStyleRule>(kTestCases,
                                                                "output:ff;"
                                                                "input:next");
}

TEST(DffNameStyleRuleTest, RejectPipelineStages) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {"module m; always_ff @(posedge c) begin a_q1 <= ",
       {SymbolIdentifier, "a_q0"},
       "; end endmodule"},
      {"module m; always_ff @(posedge c) begin a_q2",
       " <= ",
       {SymbolIdentifier, "a_q0"},
       "; end endmodule"},
      {"module m; always_ff @(posedge c) begin "
       "a_q3 <= ",
       {SymbolIdentifier, "a_q5"},
       "; end endmodule"},
      {"module m; always_ff @(posedge c) begin a_q5 <= ",
       {SymbolIdentifier, "a_q2"},
       "; end endmodule"},
      {"module m; always_ff @(posedge c) begin "
       "a_q2 <= ",
       {SymbolIdentifier, "a_n"},
       "; end endmodule"},
      {"module m; always_ff @(posedge c) begin "
       "a_q3 <= ",
       {SymbolIdentifier, "a_n2"},
       "; end endmodule"},
      {"module m; always_ff @(posedge c) begin "
       "a_q2 <= ",
       {SymbolIdentifier, "a_ff"},
       "; end endmodule"},
  };
  RunConfiguredLintTestCases<VerilogAnalyzer, DffNameStyleRule>(
      kTestCases, "output:q,ff;input:n");
}

TEST(DffNameStyleRuleTest, AcceptTestsNoSuffixes) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {"module m; always_ff @(posedge c) a <= b; endmodule"},
      {"module m; always_ff @(posedge c) a_ff <= b_n; endmodule"},
      {"module m; always_ff @(posedge c) a_myverylongsuffix <= b_freedom; "
       "endmodule"},
  };
  RunConfiguredLintTestCases<VerilogAnalyzer, DffNameStyleRule>(kTestCases,
                                                                "output:;"
                                                                "input:");
}

TEST(DffNameStyleRuleTest, Reject) {
  const std::initializer_list<verible::LintTestCase> kTestCases = {
      {"module m; always_ff @(posedge c) begin ",
       {SymbolIdentifier, "a"},
       " <= b_n; end endmodule"},
      {"module m; always_ff @(posedge c) begin ",
       {MacroIdentifier, "`A_q"},
       " <= b_n; end endmodule"},
      {"module m; always_ff @(posedge c) begin ",
       {MacroIdentifier, "`A"},
       " <= b_n; end endmodule"},
      {"module m; always_ff @(posedge c) begin ",
       {SymbolIdentifier, "mystruct"},
       ".a_q <= b_n; end endmodule"},
      {"module m; always_ff @(posedge c) begin a_ff <= ",
       {SymbolIdentifier, "b"},
       "; end endmodule"},
      {"module m; always_ff @(posedge c) begin ",
       {SymbolIdentifier, "a1_ff"},
       " <= a0_n; end endmodule"},
      {"module m; always_ff @(posedge c) begin a_ff <= ",
       {SymbolIdentifier, "mystruct"},
       ".b_n; end endmodule"},
      {"module m; always_ff @(posedge c) begin a_ff <= ",
       {SymbolIdentifier, "mystruct"},
       ".b; end endmodule"},
      {"module m; always_ff @(posedge c) begin a_ff <= ",
       {SymbolIdentifier, "b_n"},
       "[2]; end endmodule"},
      {"module m; always_ff @(posedge c) begin a_ff <= ",
       {SymbolIdentifier, "b"},
       "[2]; end endmodule"},
      {"module m; always_ff @(posedge c) begin a_ff <= ",
       {SymbolIdentifier, "b_n"},
       "(); end endmodule"},
      {"module m; always_ff @(posedge c) begin a_ff <= ",
       {SymbolIdentifier, "b"},
       "(); end endmodule"},
      {"module m; always_ff @(posedge c) begin a_ff <= ",
       {MacroIdentifier, "`DEF"},
       "; end endmodule"},
      {"module m; always_ff @(posedge c) begin a_ff <= ",
       {MacroIdentifier, "`DEF_n"},
       "; end endmodule"},
      // Equal base
      {"module m; always_ff @(posedge c) begin ",
       {SymbolIdentifier, "a_q"},
       " <= b_n; end endmodule"},
  };
  RunConfiguredLintTestCases<VerilogAnalyzer, DffNameStyleRule>(
      kTestCases, "output:ff,q;input:n");
}

TEST(DffNameStyleRuleTest, ExtractPipelineStage) {
  struct Test {
    absl::string_view str;
    std::pair<absl::string_view, std::optional<int>> expected;
  };
  const std::vector<Test> kTestCases = {
      {"data_q0", {"data_q0", {}}}, {"data_q1", {"data_q", {1}}},
      {"data_q2", {"data_q", {2}}}, {"data_q20", {"data_q", {20}}},
      {"data_q0", {"data_q0", {}}}, {"a", {"a", {}}},
      {"data", {"data", {}}}};

  for (auto &test : kTestCases) {
    auto [result_str, result_int] =
        DffNameStyleRule::ExtractPipelineStage(test.str);
    CHECK_EQ(test.expected.first, result_str);
    CHECK_EQ(test.expected.second == result_int, true);
  }
}

TEST(DffNameStyleRuleTest, ExtraChecks) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {"wire ", {SymbolIdentifier, "data_ff"}, " = 1;"},
      {"module m; assign ", {SymbolIdentifier, "data_ff"}, " = 1; endmodule"},
      {"module m;\nalways_comb begin\n",
       {SymbolIdentifier, "data_ff"},
       " = 1;\n",
       {SymbolIdentifier, "data_ff"},
       "++;\n",
       "++",
       {SymbolIdentifier, "data_ff"},
       ";\n",
       {SymbolIdentifier, "data_ff"},
       " &= 1;\n",
       "end\nendmodule"},
      {"reg data_n = 1;"},
  };
  RunLintTestCases<VerilogAnalyzer, DffNameStyleRule>(kTestCases);
}

TEST(DffNameStyleRuleTest, WaiveRegexAccept) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {"module m; always_ff @(posedge c) mem <= 1; endmodule"},
      {"module m; always_ff @(posedge c) mem[addr] <= val; endmodule"},
      {"module m; always_ff @(posedge c) mem[addr][other] <= val; endmodule"},
      {"module m; always_ff @(posedge c) if(something) begin mem[addr] <= 0; "
       "end; endmodule"},
  };
  RunLintTestCases<VerilogAnalyzer, DffNameStyleRule>(kTestCases);
}

TEST(DffNameStyleRuleTest, WaiveIfBranches) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {"module m; always_ff @(posedge c) if(!rst_ni) begin a_reg <= 0; end; "
       "endmodule "},
      {"module m; always_ff @(posedge c) if( flush_i ) begin a_reg <= 0; end; "
       "endmodule "},
      {"module m; always_ff @(posedge c) if(!rst_ni || flush_i) begin a_reg <= "
       "0; end; "
       "endmodule "},
      {"module m; always_ff @(posedge c) if(flush_i || !rst_ni) begin a_reg <= "
       "0; end; "
       "endmodule "},
      {"module m; always_ff @(posedge c) if(reset) begin a_reg <= ",
       {TK_DecNumber, "0"},
       "; end; endmodule"},
      {"module m; always_ff @(posedge c) if ( reset  ) begin a_reg <= ",
       {TK_DecNumber, "0"},
       "; end; endmodule"},
  };
  RunLintTestCases<VerilogAnalyzer, DffNameStyleRule>(kTestCases);
}

TEST(DffNameStyleRuleTest, WaiveIfBranchesConfig) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {"module m; always_ff @(posedge c) if(!rst_ni) begin a_reg <= ",
       {TK_DecNumber, "0"},
       "; end; "
       "endmodule "},
      {"module m; always_ff @(posedge c) if( flush_i ) begin a_reg <= ",
       {TK_DecNumber, "0"},
       "; end; "
       "endmodule "},
      {"module m; always_ff @(posedge c) if(!rst_ni || flush_i) begin a_reg "
       "<= ",
       {TK_DecNumber, "0"},
       "; end; "
       "endmodule "},
      {"module m; always_ff @(posedge c) if(flush_i || !rst_ni) begin a_reg <= "
       "0; end; "
       "endmodule "},
      {"module m; always_ff @(posedge c) if(reset) begin a_reg <= ",
       {TK_DecNumber, "0"},
       "; end; endmodule"},
  };
  RunConfiguredLintTestCases<VerilogAnalyzer, DffNameStyleRule>(
      kTestCases, "waive_ifs_with_conditions:  flush_i || !rst_ni ");
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
