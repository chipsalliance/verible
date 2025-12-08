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

#include "verible/verilog/analysis/checkers/endif-comment-rule.h"

#include <initializer_list>

#include "gtest/gtest.h"
#include "verible/common/analysis/linter-test-utils.h"
#include "verible/common/analysis/token-stream-linter-test-utils.h"
#include "verible/verilog/analysis/verilog-analyzer.h"
#include "verible/verilog/parser/verilog-token-enum.h"

namespace verilog {
namespace analysis {
namespace {

using verible::LintTestCase;
using verible::RunApplyFixCases;
using verible::RunLintTestCases;

// Tests that space-only text passes.
TEST(EndifCommentRuleTest, AcceptsBlank) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {""},
      {" "},
      {"\n"},
      {" \n\n"},
  };
  RunLintTestCases<VerilogAnalyzer, EndifCommentRule>(kTestCases);
}

// Tests that properly matched `endif passes.
TEST(EndifCommentRuleTest, AcceptsEndifWithComment) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {"`ifdef FOO\n`endif  // FOO\n"},
      {"`ifdef FOO\n`endif  //FOO\n"},
      {"`ifdef FOO\n`endif//FOO\n"},
      {"`ifdef FOO\n`endif  /* FOO */\n"},      // /* comment style */
      {"`ifdef FOO\n`endif  /*   FOO   */\n"},  // /* comment style */
      {"`ifdef FOO\n`endif  /*** FOO ***/\n"},  // /* comment style */
      {"`ifdef\tFOO\n`endif\t// FOO\n"},        // tabs ok as spaces
      {"`ifdef       FOO\n`endif  // FOO\n"},   // extra spaces stripped
      {"`ifdef FOO\n`endif  //       FOO\n"},
      {"`ifndef FOO\n`endif  // FOO\n"},          // ifndef
      {"`ifdef FOO  // foo!\n`endif  // FOO\n"},  // ifdef comment ignored
      {"`ifdef FOO\n`else\n`endif  // FOO\n"},
      {"`ifdef FOO\n`elsif BAR\n`endif  // FOO\n"},
      {"`ifdef FOO\n`elsif BAR\n`else\n`endif  // FOO\n"},
      {"`ifdef FOO\nmodule foo;\nendmodule\n`endif  // FOO\n"},
      {"`ifdef FOO\n`ifdef BAR\n`endif  // BAR\n`endif  // FOO\n"},
      // lexically valid, syntactically invalid, but should not crash
      {"`endif\n"},
      {"`endif  // fgh\n"},
  };
  RunLintTestCases<VerilogAnalyzer, EndifCommentRule>(kTestCases);
}

// Tests that unmatched `endif is detected.
TEST(EndifCommentRuleTest, RejectsEndifWithoutComment) {
  constexpr int kToken = PP_endif;
  const std::initializer_list<LintTestCase> kTestCases = {
      {"`ifdef FOO\n", {kToken, "`endif"}, "\n"},
      {"`ifdef FOO\n", {kToken, "`endif"}, ""},  // missing POSIX newline
      {"`ifndef FOO\n", {kToken, "`endif"}, "\n"},
      {"`ifdef FOO\n", {kToken, "`endif"}, "  //\n"},
      {"`ifdef FOO\n", {kToken, "`endif"}, "  // BAR\n"},
      {"`ifdef FOO\n", {kToken, "`endif"}, "  /* BAR    */\n"},
      {"`ifdef FOO\n", {kToken, "`endif"}, "  /**  BAR **/\n"},
      {"`ifdef FOO\n`else\n", {kToken, "`endif"}, "\n"},
      {"`ifdef FOO\n`else\n", {kToken, "`endif"}, "  // BAR\n"},
      {"`ifdef FOO\n", {kToken, "`endif"}, "  // TOO MUCH FOO\n"},
      {"`ifndef FOO\n", {kToken, "`endif"}, "  // FOO FOR YOU\n"},
      {"`ifdef FOO\n`elsif BAR\n", {kToken, "`endif"}, "  // BAR\n"},
      {"`ifdef FOO\n`elsif BAR\n", {kToken, "`endif"}, "  // BAZ\n"},
      {"`ifdef FOO\n`elsif BAR\n`else\n", {kToken, "`endif"}, "  // BAR\n"},
      {"`ifdef FOO\n`endif  // FOO\n`ifdef GOO\n", {kToken, "`endif"}, ""},
      {"`ifdef FOO\n",
       {kToken, "`endif"},
       "\n`ifdef GOO\n",
       {kToken, "`endif"}},
      {"`ifdef FOO\n`ifdef BAR\n", {kToken, "`endif"}, "\n`endif  // FOO\n"},
      {"`ifdef FOO\n`ifdef BAR\n`endif // BAR\n", {kToken, "`endif"}, "\n"},
      {"`ifdef FOO\n`ifdef BAR\n",
       {kToken, "`endif"},
       "\n",
       {kToken, "`endif"},
       "\n"},
      {"`ifdef FOO\n`ifdef BAR\n",
       {kToken, "`endif"},
       " // FOO\n",
       {kToken, "`endif"},
       " // BAR\n"},
  };
  RunLintTestCases<VerilogAnalyzer, EndifCommentRule>(kTestCases);
}

TEST(EndifCommentRuleTest, ApplyAutoFix) {
  // Alternatives the auto fix offers
  constexpr int kEOLComment = 0;
  constexpr int kBlkComment = 1;
  const std::initializer_list<verible::AutoFixInOut> kTestCases = {
      {"`ifdef FOO\n`endif\n", "`ifdef FOO\n`endif  // FOO\n", kEOLComment},
      {"`ifdef FOO  /*xyz*/\n`endif\n", "`ifdef FOO  /*xyz*/\n`endif  // FOO\n",
       kEOLComment},
      {"`ifdef FOO\n`endif\n", "`ifdef FOO\n`endif  /* FOO */\n", kBlkComment},
  };
  RunApplyFixCases<VerilogAnalyzer, EndifCommentRule>(kTestCases, "");
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
