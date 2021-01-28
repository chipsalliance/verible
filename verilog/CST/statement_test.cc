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

#include "verilog/CST/statement.h"

#include <memory>
#include <vector>

#include "common/analysis/matcher/matcher_builders.h"
#include "common/analysis/syntax_tree_search.h"
#include "common/analysis/syntax_tree_search_test_utils.h"
#include "common/text/concrete_syntax_tree.h"
#include "common/text/symbol.h"
#include "common/text/text_structure.h"
#include "common/text/token_info.h"
#include "common/text/token_info_test_util.h"
#include "common/util/casts.h"
#include "common/util/logging.h"
#include "common/util/range.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "verilog/CST/match_test_utils.h"
#include "verilog/CST/verilog_matchers.h"
#include "verilog/CST/verilog_nonterminals.h"
#include "verilog/analysis/verilog_analyzer.h"

#undef EXPECT_OK
#define EXPECT_OK(value) EXPECT_TRUE((value).ok())

#undef ASSERT_OK
#define ASSERT_OK(value) ASSERT_TRUE((value).ok())

namespace verilog {
namespace {

using verible::SymbolKind;
using verible::SymbolTag;
using verible::SyntaxTreeSearchTestCase;
using verible::TextStructureView;
using verible::TreeSearchMatch;

struct ControlStatementTestData {
  NodeEnum expected_construct;
  SyntaxTreeSearchTestCase token_data;
};

TEST(GetAnyControlStatementBodyTest, Various) {
  constexpr int kTag = 1;  // value doesn't matter
  const ControlStatementTestData kTestCases[] = {
      // each of these test cases should match exactly one statement body
      {NodeEnum::kGenerateIfClause,
       {"module m;\n"
        "  if (expr)\n",
        {kTag, ";"},  // null generate item
        "\n"
        "  else \n"
        "   bar foo;\n"
        "endmodule\n"}},
      {NodeEnum::kGenerateIfClause,
       {"module m;\n"
        "  if (expr)\n"
        "   ",
        {kTag, "foo bar;"},
        "\n"
        "  else \n"
        "   bar foo;\n"
        "endmodule\n"}},
      {NodeEnum::kGenerateIfClause,
       {"module m;\n"
        "  if (expr)\n"
        "   ",
        {kTag, "begin\nfoo bar;end"},
        "\n"
        "  else \n"
        "   bar foo;\n"
        "endmodule\n"}},

      {NodeEnum::kGenerateElseClause,
       {"module m;\n"
        "  if (expr)\n"
        "   foo bar;\n"
        "  else \n",
        {kTag, ";"},  // null generate item
        "\n"
        "endmodule\n"}},
      {NodeEnum::kGenerateElseClause,
       {"module m;\n"
        "  if (expr)\n"
        "   foo bar;\n"
        "  else \n",
        {kTag, "bar#(1)   foo;"},
        "\n"
        "endmodule\n"}},
      {NodeEnum::kGenerateElseClause,
       {"module m;\n"
        "  if (expr)\n"
        "   foo bar;\n"
        "  else \n",
        {kTag, "begin \nbar#(1)   foo; baz bam();\nend"},
        "\n"
        "endmodule\n"}},

      {NodeEnum::kLoopGenerateConstruct,
       {"module m;\n"
        "  for (genvar i=0; i<N; ++i)\n"
        "   ",
        {kTag, ";"},  // null generate item
        "\n"
        "endmodule\n"}},
      {NodeEnum::kLoopGenerateConstruct,
       {"module m;\n"
        "  for (genvar i=0; i<N; ++i)\n"
        "   ",
        {kTag, "foo#(.N(i)) bar;"},
        "\n"
        "endmodule\n"}},
      {NodeEnum::kLoopGenerateConstruct,
       {"module m;\n"
        "  for (genvar i=0; i<N; ++i)\n"
        "   ",
        {kTag, "begin:l1\n      foo#(.N(i)) bar;\n  end : l1"},
        "\n"
        "endmodule\n"}},

      {NodeEnum::kIfClause,
       {"function f;\n"
        "  if (expr)\n",
        {kTag, ";"},  // null statement
        "\n"
        "  else \n"
        "   bar=foo;\n"
        "endfunction\n"}},
      {NodeEnum::kIfClause,
       {"function f;\n"
        "  if (expr)\n"
        "   ",
        {kTag, "foo=bar;"},
        "\n"
        "  else \n"
        "   bar=foo;\n"
        "endfunction\n"}},
      {NodeEnum::kIfClause,
       {"task t;\n"
        "  if (expr)\n"
        "   ",
        {kTag, "begin\nfoo=bar; bar=1;\nend"},
        "\n"
        "  else \n"
        "   bar=foo;\n"
        "endtask\n"}},

      {NodeEnum::kElseClause,
       {"task t;\n"
        "  if (expr)\n"
        "   foo =bar;\n"
        "\n"
        "  else\n",
        {kTag, "bar=foo;"},
        "endtask\n"}},
      {NodeEnum::kElseClause,
       {"task t;\n"
        "  if (expr)\n"
        "   foo =bar;\n"
        "\n"
        "  else\n",
        {kTag, ";"},  // null statement
        "endtask\n"}},
      {NodeEnum::kElseClause,
       {"function f;\n"
        "  if (expr)\n"
        "   foo =bar;\n"
        "  else\n",
        {kTag, "begin:bb bar=foo(baz);\n\nend :\nbb"},
        "\nendfunction\n"}},

      {NodeEnum::kForLoopStatement,
       {"function f;\n"
        "  for (int j=N; expr; --j)\n"
        "   ",
        {kTag, ";"},  // null statement
        "\n"
        "endfunction\n"}},
      {NodeEnum::kForLoopStatement,
       {"function f;\n"
        "  for (int j=N; expr; --j)\n"
        "   ",
        {kTag, "foo=bar;"},
        "\n"
        "endfunction\n"}},
      {NodeEnum::kForLoopStatement,
       {"task t;\n"
        "  for (int j=N; expr; --j)\n"
        "   ",
        {kTag, "begin\nfoo=bar; bar=1;\nend"},
        "\n"
        "endtask\n"}},

      {NodeEnum::kDoWhileLoopStatement,
       {"function f;\n"
        "   do\n",
        {kTag, ";"},  // null statement
        "  while (expr);\n"
        "endfunction\n"}},
      {NodeEnum::kDoWhileLoopStatement,
       {"function f;\n"
        "   do\n",
        {kTag, "foo=bar;"},
        "  while (expr);\n"
        "endfunction\n"}},
      {NodeEnum::kDoWhileLoopStatement,
       {"task t;\n"
        "  do ",
        {kTag, "begin\nfoo=bar; bar=1;\nend"},
        "  while (expr);\n"
        "endtask\n"}},

      {NodeEnum::kForeverLoopStatement,
       {"function f;\n"
        "  forever\n",
        {kTag, ";"},  // null statement
        "\n"
        "endfunction\n"}},
      {NodeEnum::kForeverLoopStatement,
       {"function f;\n"
        "  forever\n"
        "   ",
        {kTag, "foo=bar;"},
        "\n"
        "endfunction\n"}},
      {NodeEnum::kForeverLoopStatement,
       {"task t;\n"
        "  forever\n"
        "   ",
        {kTag, "begin\nfoo=bar; bar=1;\nend"},
        "\n"
        "endtask\n"}},

      {NodeEnum::kForeachLoopStatement,
       {"function f;\n"
        "  foreach (x[i])\n",
        {kTag, ";"},  // null statement
        "\n"
        "endfunction\n"}},
      {NodeEnum::kForeachLoopStatement,
       {"function f;\n"
        "  foreach (x[i])\n"
        "   ",
        {kTag, "foo=bar;"},
        "\n"
        "endfunction\n"}},
      {NodeEnum::kForeachLoopStatement,
       {"task t;\n"
        "  foreach (x[i])\n"
        "   ",
        {kTag, "begin\nfoo=bar; bar=1;\nend"},
        "\n"
        "endtask\n"}},

      {NodeEnum::kRepeatLoopStatement,
       {"function f;\n"
        "  repeat (8)\n",
        {kTag, ";"},  // null statement
        "\n"
        "endfunction\n"}},
      {NodeEnum::kRepeatLoopStatement,
       {"function f;\n"
        "  repeat (8)\n"
        "   ",
        {kTag, "foo=bar;"},
        "\n"
        "endfunction\n"}},
      {NodeEnum::kRepeatLoopStatement,
       {"task t;\n"
        "  repeat (9)\n"
        "   ",
        {kTag, "begin\nfoo=bar; bar=1;\nend"},
        "\n"
        "endtask\n"}},

      {NodeEnum::kWhileLoopStatement,
       {"function f;\n"
        "  while (expr)\n",
        {kTag, ";"},  // null statement
        "\n"
        "endfunction\n"}},
      {NodeEnum::kWhileLoopStatement,
       {"function f;\n"
        "  while (expr)\n"
        "   ",
        {kTag, "foo=bar;"},
        "\n"
        "endfunction\n"}},
      {NodeEnum::kWhileLoopStatement,
       {"task t;\n"
        "  while (expr)\n"
        "   ",
        {kTag, "begin\nfoo=bar; bar=1;\nend"},
        "\n"
        "endtask\n"}},

      {NodeEnum::kProceduralTimingControlStatement,
       {"module  m;\n"
        "  always @(negedge c)\n",
        {kTag, ";"},  // null statement
        "\n"
        "endmodule\n"}},
      {NodeEnum::kProceduralTimingControlStatement,
       {"module  m;\n"
        "  always @(negedge c)\n",
        {kTag, "foo=bar;"},
        "\n"
        "endmodule\n"}},
      {NodeEnum::kProceduralTimingControlStatement,
       {"module  m;\n"
        "  always @(negedge c)\n",
        {kTag, "begin\nfoo=bar; bar=1;\nend"},
        "\n"
        "endmodule\n"}},

      {NodeEnum::kAssertionClause,
       {"task  t;\n"
        "  assert (expr)\n",
        {kTag, ";"},  // null statement
        "\n"
        "endtask\n"}},
      {NodeEnum::kAssertionClause,
       {"task  t;\n"
        "  assert (expr)\n",
        {kTag, "action();"},
        "\n"
        "endtask\n"}},
      {NodeEnum::kAssertionClause,
       {"task  t;\n"
        "  assert (expr)\n",
        {kTag, "begin action(); end"},
        "\n"
        "endtask\n"}},

      {NodeEnum::kAssumeClause,
       {"task  t;\n"
        "  assume (expr)\n",
        {kTag, ";"},  // null statement
        "\n"
        "endtask\n"}},
      {NodeEnum::kAssumeClause,
       {"task  t;\n"
        "  assume (expr)\n",
        {kTag, "action();"},
        "\n"
        "endtask\n"}},
      {NodeEnum::kAssumeClause,
       {"task  t;\n"
        "  assume (expr)\n",
        {kTag, "begin action(); end"},
        "\n"
        "endtask\n"}},

      {NodeEnum::kWaitStatement,
       {"task  t;\n"
        "  wait (expr)\n",
        {kTag, ";"},  // null statement
        "\n"
        "endtask\n"}},
      {NodeEnum::kWaitStatement,
       {"task  t;\n"
        "  wait (expr)\n",
        {kTag, "snooze();"},
        "\n"
        "endtask\n"}},
      {NodeEnum::kWaitStatement,
       {"task  t;\n"
        "  wait (expr)\n",
        {kTag, "begin snooze(); end"},
        "\n"
        "endtask\n"}},

      {NodeEnum::kCoverStatement,
       {"task  t;\n"
        "  cover (expr)\n",
        {kTag, ";"},  // null statement
        "\n"
        "endtask\n"}},
      {NodeEnum::kCoverStatement,
       {"task  t;\n"
        "  cover (expr)\n",
        {kTag, "snooze();"},
        "\n"
        "endtask\n"}},
      {NodeEnum::kCoverStatement,
       {"task  t;\n"
        "  cover (expr)\n",
        {kTag, "begin snooze(); end"},
        "\n"
        "endtask\n"}},

      {NodeEnum::kAssertPropertyClause,
       {"task  t;\n"
        "  assert property (p_expr)\n",
        {kTag, ";"},  // null statement
        "\n"
        "endtask\n"}},
      {NodeEnum::kAssertPropertyClause,
       {"task  t;\n"
        "  assert property (p_expr)\n",
        {kTag, "action();"},
        "\n"
        "endtask\n"}},
      {NodeEnum::kAssertPropertyClause,
       {"task  t;\n"
        "  assert property (p_expr)\n",
        {kTag, "begin action(); end"},
        "\n"
        "endtask\n"}},

      {NodeEnum::kAssumePropertyClause,
       {"task  t;\n"
        "  assume property (p_expr)\n",
        {kTag, ";"},  // null statement
        "\n"
        "endtask\n"}},
      {NodeEnum::kAssumePropertyClause,
       {"task  t;\n"
        "  assume property (p_expr)\n",
        {kTag, "action();"},
        "\n"
        "endtask\n"}},
      {NodeEnum::kAssumePropertyClause,
       {"task  t;\n"
        "  assume property (p_expr)\n",
        {kTag, "begin action(); end"},
        "\n"
        "endtask\n"}},

      {NodeEnum::kExpectPropertyClause,
       {"task  t;\n"
        "  expect (p_expr)\n",
        {kTag, ";"},  // null statement
        "\n"
        "endtask\n"}},
      {NodeEnum::kExpectPropertyClause,
       {"task  t;\n"
        "  expect (p_expr)\n",
        {kTag, "action();"},
        "\n"
        "endtask\n"}},
      {NodeEnum::kExpectPropertyClause,
       {"task  t;\n"
        "  expect (p_expr)\n",
        {kTag, "begin action(); end"},
        "\n"
        "endtask\n"}},

      {NodeEnum::kCoverPropertyStatement,
       {"task  t;\n"
        "  cover property (p_expr)\n",
        {kTag, ";"},  // null statement
        "\n"
        "endtask\n"}},
      {NodeEnum::kCoverPropertyStatement,
       {"task  t;\n"
        "  cover property (p_expr)\n",
        {kTag, "action();"},
        "\n"
        "endtask\n"}},
      {NodeEnum::kCoverPropertyStatement,
       {"task  t;\n"
        "  cover property (p_expr)\n",
        {kTag, "begin action(); end"},
        "\n"
        "endtask\n"}},

      {NodeEnum::kCoverSequenceStatement,
       {"task  t;\n"
        "  cover sequence (s_expr)\n",
        {kTag, ";"},  // null statement
        "\n"
        "endtask\n"}},
      {NodeEnum::kCoverSequenceStatement,
       {"task  t;\n"
        "  cover sequence (s_expr)\n",
        {kTag, "action();"},
        "\n"
        "endtask\n"}},
      {NodeEnum::kCoverSequenceStatement,
       {"task  t;\n"
        "  cover sequence (s_expr)\n",
        {kTag, "begin action(); end"},
        "\n"
        "endtask\n"}},
  };
  for (const auto& test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test.token_data,
        [&test](const TextStructureView& text_structure) {
          const auto& root = text_structure.SyntaxTree();

          // Grab outer statement constructs.
          const auto statements = verible::SearchSyntaxTree(
              *ABSL_DIE_IF_NULL(root),
              verible::matcher::DynamicTagMatchBuilder(
                  SymbolTag{SymbolKind::kNode,
                            static_cast<int>(test.expected_construct)})());

          // Extract subtree of interest.
          std::vector<TreeSearchMatch> bodies;
          for (const auto& statement : statements) {
            const auto* body = GetAnyControlStatementBody(*statement.match);
            bodies.push_back(TreeSearchMatch{body, {/* ignored context */}});
          }
          return bodies;
        });
  }
}

TEST(GetAnyConditionalIfClauseTest, Various) {
  const ControlStatementTestData kTestCases[] = {
      // each of these test cases should match exactly one statement body
      {NodeEnum::kConditionalGenerateConstruct,
       {"module m;\n",
        {static_cast<int>(NodeEnum::kGenerateIfClause), "if (expr);"},
        "\n"
        "endmodule\n"}},
      {NodeEnum::kConditionalGenerateConstruct,
       {"module m;\n",
        {static_cast<int>(NodeEnum::kGenerateIfClause), "if (expr) foo bar;"},
        "\n"
        "endmodule\n"}},
      {NodeEnum::kConditionalGenerateConstruct,
       {"module m;\n",
        {static_cast<int>(NodeEnum::kGenerateIfClause), "if (expr) foo bar;"},
        "\n"
        "  else \n"
        "   bar foo;\n"
        "endmodule\n"}},
      {NodeEnum::kConditionalGenerateConstruct,
       {"module m;\n",
        {static_cast<int>(NodeEnum::kGenerateIfClause),
         "if (expr) begin\nfoo bar;end"},
        "\n"
        "  else \n"
        "   bar foo;\n"
        "endmodule\n"}},

      {NodeEnum::kConditionalStatement,
       {"function f;\n",
        {static_cast<int>(NodeEnum::kIfClause), "if ( expr );"},
        "\n"
        "endfunction\n"}},
      {NodeEnum::kConditionalStatement,
       {"function f;\n",
        {static_cast<int>(NodeEnum::kIfClause), "if ( expr ) foo=bar;"},
        "\n"
        "endfunction\n"}},
      {NodeEnum::kConditionalStatement,
       {"function f;\n",
        {static_cast<int>(NodeEnum::kIfClause), "if ( expr ) foo=bar;"},
        "\n"
        "  else \n"
        "   bar=foo;\n"
        "endfunction\n"}},
      {NodeEnum::kConditionalStatement,
       {"task t;\n",
        {static_cast<int>(NodeEnum::kIfClause),
         "if  (expr)begin\nfoo=bar; bar=1;\nend"},
        "\n"
        "  else \n"
        "   bar=foo;\n"
        "endtask\n"}},

      {NodeEnum::kAssertionStatement,
       {"function f;\n",
        {static_cast<int>(NodeEnum::kAssertionClause),
         "assert ( expr );"},  // null statement
        "\n"
        "endfunction\n"}},
      {NodeEnum::kAssertionStatement,
       {"function f;\n",
        {static_cast<int>(NodeEnum::kAssertionClause),
         "assert ( expr ) foo=bar;"},
        "\n"
        "endfunction\n"}},
      {NodeEnum::kAssertionStatement,
       {"function f;\n",
        {static_cast<int>(NodeEnum::kAssertionClause),
         "assert ( expr ) foo=bar;"},
        "\n"
        "  else \n"
        "   bar=foo;\n"
        "endfunction\n"}},
      {NodeEnum::kAssertionStatement,
       {"task t;\n",
        {static_cast<int>(NodeEnum::kAssertionClause),
         "assert  (expr)begin\nfoo=bar; bar=1;\nend"},
        "\n"
        "  else \n"
        "   bar=foo;\n"
        "endtask\n"}},

      {NodeEnum::kAssumeStatement,
       {"function f;\n",
        {static_cast<int>(NodeEnum::kAssumeClause), "assume ( expr );"},
        "\n"
        "endfunction\n"}},
      {NodeEnum::kAssumeStatement,
       {"function f;\n",
        {static_cast<int>(NodeEnum::kAssumeClause), "assume ( expr ) foo=bar;"},
        "\n"
        "endfunction\n"}},
      {NodeEnum::kAssumeStatement,
       {"function f;\n",
        {static_cast<int>(NodeEnum::kAssumeClause), "assume ( expr ) foo=bar;"},
        "\n"
        "  else \n"
        "   bar=foo;\n"
        "endfunction\n"}},
      {NodeEnum::kAssumeStatement,
       {"task t;\n",
        {static_cast<int>(NodeEnum::kAssumeClause),
         "assume  (expr)begin\nfoo=bar; bar=1;\nend"},
        "\n"
        "  else \n"
        "   bar=foo;\n"
        "endtask\n"}},

      {NodeEnum::kAssertPropertyStatement,
       {"task t;\n",
        {static_cast<int>(NodeEnum::kAssertPropertyClause),
         "assert property ( p_expr );"},  // null statement
        "\n"
        "endtask\n"}},
      {NodeEnum::kAssertPropertyStatement,
       {"task t;\n",
        {static_cast<int>(NodeEnum::kAssertPropertyClause),
         "assert property ( p_expr ) foo=bar;"},
        "\n"
        "endtask\n"}},
      {NodeEnum::kAssertPropertyStatement,
       {"task t;\n",
        {static_cast<int>(NodeEnum::kAssertPropertyClause),
         "assert property ( p_expr ) foo=bar;"},
        "\n"
        "  else \n"
        "   bar=foo;\n"
        "endtask\n"}},
      {NodeEnum::kAssertPropertyStatement,
       {"task t;\n",
        {static_cast<int>(NodeEnum::kAssertPropertyClause),
         "assert property  (p_expr)begin\nfoo=bar; bar=1;\nend"},
        "\n"
        "  else \n"
        "   bar=foo;\n"
        "endtask\n"}},

      {NodeEnum::kAssumePropertyStatement,
       {"task t;\n",
        {static_cast<int>(NodeEnum::kAssumePropertyClause),
         "assume property ( p_expr );"},  // null statement
        "\n"
        "endtask\n"}},
      {NodeEnum::kAssumePropertyStatement,
       {"task t;\n",
        {static_cast<int>(NodeEnum::kAssumePropertyClause),
         "assume property ( p_expr ) foo=bar;"},
        "\n"
        "endtask\n"}},
      {NodeEnum::kAssumePropertyStatement,
       {"task t;\n",
        {static_cast<int>(NodeEnum::kAssumePropertyClause),
         "assume property ( p_expr ) foo=bar;"},
        "\n"
        "  else \n"
        "   bar=foo;\n"
        "endtask\n"}},
      {NodeEnum::kAssumePropertyStatement,
       {"task t;\n",
        {static_cast<int>(NodeEnum::kAssumePropertyClause),
         "assume property  (p_expr)begin\nfoo=bar; bar=1;\nend"},
        "\n"
        "  else \n"
        "   bar=foo;\n"
        "endtask\n"}},

      {NodeEnum::kExpectPropertyStatement,
       {"task t;\n",
        {static_cast<int>(NodeEnum::kExpectPropertyClause),
         "expect ( p_expr );"},  // null statement
        "\n"
        "endtask\n"}},
      {NodeEnum::kExpectPropertyStatement,
       {"task t;\n",
        {static_cast<int>(NodeEnum::kExpectPropertyClause),
         "expect ( p_expr ) foo=bar;"},
        "\n"
        "endtask\n"}},
      {NodeEnum::kExpectPropertyStatement,
       {"task t;\n",
        {static_cast<int>(NodeEnum::kExpectPropertyClause),
         "expect ( p_expr ) foo=bar;"},
        "\n"
        "  else \n"
        "   bar=foo;\n"
        "endtask\n"}},
      {NodeEnum::kExpectPropertyStatement,
       {"task t;\n",
        {static_cast<int>(NodeEnum::kExpectPropertyClause),
         "expect (p_expr)begin\nfoo=bar; bar=1;\nend"},
        "\n"
        "  else \n"
        "   bar=foo;\n"
        "endtask\n"}},
  };
  for (const auto& test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test.token_data,
        [&test](const TextStructureView& text_structure) {
          const auto& root = text_structure.SyntaxTree();

          // Grab outer statement constructs.
          const auto statements = verible::SearchSyntaxTree(
              *ABSL_DIE_IF_NULL(root),
              verible::matcher::DynamicTagMatchBuilder(
                  SymbolTag{SymbolKind::kNode,
                            static_cast<int>(test.expected_construct)})());

          // Extract subtree of interest.
          std::vector<TreeSearchMatch> bodies;
          for (const auto& statement : statements) {
            const auto* clause = GetAnyConditionalIfClause(*statement.match);
            bodies.push_back(TreeSearchMatch{clause, {/* ignored context */}});
          }
          return bodies;
        });
  }
}

TEST(GetAnyConditionalElseClauseTest, NoElseClause) {
  const ControlStatementTestData kTestCases[] = {
      // each of these test cases should match exactly one statement body
      {NodeEnum::kConditionalGenerateConstruct,
       {"module m;\n",
        {static_cast<int>(NodeEnum::kGenerateIfClause), "if (expr);"},
        "\n"
        "endmodule\n"}},
      {NodeEnum::kConditionalGenerateConstruct,
       {"module m;\n",
        {static_cast<int>(NodeEnum::kGenerateIfClause), "if (expr) foo bar;"},
        "\n"
        "endmodule\n"}},

      {NodeEnum::kConditionalStatement,
       {"function f;\n",
        {static_cast<int>(NodeEnum::kIfClause), "if ( expr );"},
        "\n"
        "endfunction\n"}},
      {NodeEnum::kConditionalStatement,
       {"function f;\n",
        {static_cast<int>(NodeEnum::kIfClause), "if ( expr ) foo=bar;"},
        "\n"
        "endfunction\n"}},

      {NodeEnum::kAssertionStatement,
       {"task t;\n",
        {static_cast<int>(NodeEnum::kAssertionClause), "assert ( expr );"},
        "\n"
        "endtask\n"}},
      {NodeEnum::kAssertionStatement,
       {"task t;\n",
        {static_cast<int>(NodeEnum::kAssertionClause),
         "assert ( expr ) foo=bar;"},
        "\n"
        "endtask\n"}},

      {NodeEnum::kAssumeStatement,
       {"task t;\n",
        {static_cast<int>(NodeEnum::kAssumeClause), "assume ( expr );"},
        "\n"
        "endtask\n"}},
      {NodeEnum::kAssumeStatement,
       {"task t;\n",
        {static_cast<int>(NodeEnum::kAssumeClause), "assume ( expr ) foo=bar;"},
        "\n"
        "endtask\n"}},

      {NodeEnum::kAssertPropertyStatement,
       {"task t;\n",
        {static_cast<int>(NodeEnum::kAssertPropertyClause),
         "assert property( expr );"},
        "\n"
        "endtask\n"}},
      {NodeEnum::kAssertPropertyStatement,
       {"task t;\n",
        {static_cast<int>(NodeEnum::kAssertPropertyClause),
         "assert property ( expr ) foo=bar;"},
        "\n"
        "endtask\n"}},

      {NodeEnum::kAssumePropertyStatement,
       {"task t;\n",
        {static_cast<int>(NodeEnum::kAssumePropertyClause),
         "assume property( expr );"},
        "\n"
        "endtask\n"}},
      {NodeEnum::kAssumePropertyStatement,
       {"task t;\n",
        {static_cast<int>(NodeEnum::kAssumePropertyClause),
         "assume property ( expr ) foo=bar;"},
        "\n"
        "endtask\n"}},

      {NodeEnum::kExpectPropertyStatement,
       {"task t;\n",
        {static_cast<int>(NodeEnum::kExpectPropertyClause), "expect( expr );"},
        "\n"
        "endtask\n"}},
      {NodeEnum::kExpectPropertyStatement,
       {"task t;\n",
        {static_cast<int>(NodeEnum::kExpectPropertyClause),
         "expect ( expr ) foo=bar;"},
        "\n"
        "endtask\n"}},
  };
  for (const auto& test : kTestCases) {
    const absl::string_view code(test.token_data.code);
    VerilogAnalyzer analyzer(code, "test-file");
    ASSERT_OK(analyzer.Analyze()) << "failed on:\n" << code;
    const auto& root = analyzer.Data().SyntaxTree();

    const auto statements = verible::SearchSyntaxTree(
        *ABSL_DIE_IF_NULL(root),
        verible::matcher::DynamicTagMatchBuilder(SymbolTag{
            SymbolKind::kNode, static_cast<int>(test.expected_construct)})());
    ASSERT_EQ(statements.size(), 1);
    const auto& statement = *statements.front().match;
    const auto* clause = GetAnyConditionalElseClause(statement);
    EXPECT_EQ(clause, nullptr);
  }
}

TEST(GetAnyConditionalElseClauseTest, HaveElseClause) {
  const ControlStatementTestData kTestCases[] = {
      // each of these test cases should match exactly one statement body
      {NodeEnum::kConditionalGenerateConstruct,
       {"module m;\n",
        "if (expr);\n",
        {static_cast<int>(NodeEnum::kGenerateElseClause),
         "else \n"
         "   ;"},  // null else body
        "\n"
        "endmodule\n"}},
      {NodeEnum::kConditionalGenerateConstruct,
       {"module m;\n",
        "if (expr);\n",
        {static_cast<int>(NodeEnum::kGenerateElseClause),
         "else \n"
         "   bar foo;"},
        "\n"
        "endmodule\n"}},
      {NodeEnum::kConditionalGenerateConstruct,
       {"module m;\n",
        "if (expr) foo bar;\n",
        {static_cast<int>(NodeEnum::kGenerateElseClause),
         "else \n"
         "   bar foo;"},
        "\n"
        "endmodule\n"}},
      {NodeEnum::kConditionalGenerateConstruct,
       {"module m;\n",
        "if (expr) foo bar;\n",
        {static_cast<int>(NodeEnum::kGenerateElseClause),
         "else \n"
         "   begin bar foo;\nend"},
        "\n"
        "endmodule\n"}},

      {NodeEnum::kConditionalStatement,
       {"function f;\n",
        "if ( expr );\n",
        {static_cast<int>(NodeEnum::kElseClause),
         "else \n"
         "   ;"},  // null else body
        "\n"
        "endfunction\n"}},
      {NodeEnum::kConditionalStatement,
       {"function f;\n",
        "if ( expr );\n",
        {static_cast<int>(NodeEnum::kElseClause),
         "else \n"
         "   bar=foo;"},
        "\n"
        "endfunction\n"}},
      {NodeEnum::kConditionalStatement,
       {"function f;\n",
        "if ( expr ) foo=bar;\n",
        {static_cast<int>(NodeEnum::kElseClause),
         "else \n"
         "   bar=foo;"},
        "\n"
        "endfunction\n"}},
      {NodeEnum::kConditionalStatement,
       {"function f;\n",
        "if ( expr ) foo=bar;\n",
        {static_cast<int>(NodeEnum::kElseClause),
         "else \n"
         "   begin\nbar=foo;\nend"},
        "\n"
        "endfunction\n"}},

      {NodeEnum::kAssertionStatement,
       {"task t;\n",
        "assert ( expr )\n",  // no statement
        {static_cast<int>(NodeEnum::kElseClause),
         "else \n"
         "   bar=foo;"},
        "\n"
        "endtask\n"}},
      {NodeEnum::kAssertionStatement,
       {"task t;\n",
        "assert ( expr );\n",  // null statement
        {static_cast<int>(NodeEnum::kElseClause),
         "else \n"
         "   bar=foo;"},
        "\n"
        "endtask\n"}},
      {NodeEnum::kAssertionStatement,
       {"task t;\n",
        "assert ( expr ) foo=bar;\n",
        {static_cast<int>(NodeEnum::kElseClause),
         "else \n"
         "   ;"},  // null else body
        "\n"
        "endtask\n"}},
      {NodeEnum::kAssertionStatement,
       {"task t;\n",
        "assert ( expr ) foo=bar;\n",
        {static_cast<int>(NodeEnum::kElseClause),
         "else \n"
         "   bar=foo;"},
        "\n"
        "endtask\n"}},
      {NodeEnum::kAssertionStatement,
       {"task t;\n",
        "assert ( expr ) foo=bar;\n",
        {static_cast<int>(NodeEnum::kElseClause),
         "else \n"
         "   begin\nbar=foo;\nend"},
        "\n"
        "endtask\n"}},

      {NodeEnum::kAssumeStatement,
       {"task t;\n",
        "assume ( expr )\n",  // no statement
        {static_cast<int>(NodeEnum::kElseClause),
         "else \n"
         "   bar=foo;"},
        "\n"
        "endtask\n"}},
      {NodeEnum::kAssumeStatement,
       {"task t;\n",
        "assume ( expr );\n",  // null statement
        {static_cast<int>(NodeEnum::kElseClause),
         "else \n"
         "   bar=foo;"},
        "\n"
        "endtask\n"}},
      {NodeEnum::kAssumeStatement,
       {"task t;\n",
        "assume ( expr ) foo=bar;\n",
        {static_cast<int>(NodeEnum::kElseClause),
         "else \n"
         "   bar=foo;"},
        "\n"
        "endtask\n"}},
      {NodeEnum::kAssumeStatement,
       {"task t;\n",
        "assume ( expr ) foo=bar;\n",
        {static_cast<int>(NodeEnum::kElseClause),
         "else \n"
         "   ;"},  // null else body
        "\n"
        "endtask\n"}},
      {NodeEnum::kAssumeStatement,
       {"task t;\n",
        "assume ( expr ) foo=bar;\n",
        {static_cast<int>(NodeEnum::kElseClause),
         "else \n"
         "   begin\nbar=foo;\nend"},
        "\n"
        "endtask\n"}},

      {NodeEnum::kAssertPropertyStatement,
       {"task t;\n",
        "assert property ( expr )\n",  // no statement
        {static_cast<int>(NodeEnum::kElseClause),
         "else \n"
         "   bar=foo;"},
        "\n"
        "endtask\n"}},
      {NodeEnum::kAssertPropertyStatement,
       {"task t;\n",
        "assert property ( expr );\n",  // null statement
        {static_cast<int>(NodeEnum::kElseClause),
         "else \n"
         "   bar=foo;"},
        "\n"
        "endtask\n"}},
      {NodeEnum::kAssertPropertyStatement,
       {"task t;\n",
        "assert property ( expr ) foo=bar;\n",
        {static_cast<int>(NodeEnum::kElseClause),
         "else \n"
         "   ;"},  // null else body
        "\n"
        "endtask\n"}},
      {NodeEnum::kAssertPropertyStatement,
       {"task t;\n",
        "assert property ( expr ) foo=bar;\n",
        {static_cast<int>(NodeEnum::kElseClause),
         "else \n"
         "   bar=foo;"},
        "\n"
        "endtask\n"}},
      {NodeEnum::kAssertPropertyStatement,
       {"task t;\n",
        "assert property ( expr ) foo=bar;\n",
        {static_cast<int>(NodeEnum::kElseClause),
         "else \n"
         "   begin\nbar=foo;\nend"},
        "\n"
        "endtask\n"}},

      {NodeEnum::kAssumePropertyStatement,
       {"task t;\n",
        "assume property ( expr )\n",  // no statement
        {static_cast<int>(NodeEnum::kElseClause),
         "else \n"
         "   bar=foo;"},
        "\n"
        "endtask\n"}},
      {NodeEnum::kAssumePropertyStatement,
       {"task t;\n",
        "assume property ( expr );\n",  // null statement
        {static_cast<int>(NodeEnum::kElseClause),
         "else \n"
         "   bar=foo;"},
        "\n"
        "endtask\n"}},
      {NodeEnum::kAssumePropertyStatement,
       {"task t;\n",
        "assume property ( expr ) foo=bar;\n",
        {static_cast<int>(NodeEnum::kElseClause),
         "else \n"
         "   ;"},  // null else body
        "\n"
        "endtask\n"}},
      {NodeEnum::kAssumePropertyStatement,
       {"task t;\n",
        "assume property ( expr ) foo=bar;\n",
        {static_cast<int>(NodeEnum::kElseClause),
         "else \n"
         "   bar=foo;"},
        "\n"
        "endtask\n"}},
      {NodeEnum::kAssumePropertyStatement,
       {"task t;\n",
        "assume property ( expr ) foo=bar;\n",
        {static_cast<int>(NodeEnum::kElseClause),
         "else \n"
         "   begin\nbar=foo;\nend"},
        "\n"
        "endtask\n"}},

      {NodeEnum::kExpectPropertyStatement,
       {"task t;\n",
        "expect ( expr )\n",  // no statement
        {static_cast<int>(NodeEnum::kElseClause),
         "else \n"
         "   bar=foo;"},
        "\n"
        "endtask\n"}},
      {NodeEnum::kExpectPropertyStatement,
       {"task t;\n",
        "expect ( expr );\n",  // null statement
        {static_cast<int>(NodeEnum::kElseClause),
         "else \n"
         "   bar=foo;"},
        "\n"
        "endtask\n"}},
      {NodeEnum::kExpectPropertyStatement,
       {"task t;\n",
        "expect ( expr ) foo=bar;\n",
        {static_cast<int>(NodeEnum::kElseClause),
         "else \n"
         "   ;"},  // null else body
        "\n"
        "endtask\n"}},
      {NodeEnum::kExpectPropertyStatement,
       {"task t;\n",
        "expect ( expr ) foo=bar;\n",
        {static_cast<int>(NodeEnum::kElseClause),
         "else \n"
         "   bar=foo;"},
        "\n"
        "endtask\n"}},
      {NodeEnum::kExpectPropertyStatement,
       {"task t;\n",
        "expect ( expr ) foo=bar;\n",
        {static_cast<int>(NodeEnum::kElseClause),
         "else \n"
         "   begin\nbar=foo;\nend"},
        "\n"
        "endtask\n"}},
  };
  for (const auto& test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test.token_data,
        [&test](const TextStructureView& text_structure) {
          const auto& root = text_structure.SyntaxTree();

          // Grab outer statement constructs.
          const auto statements = verible::SearchSyntaxTree(
              *ABSL_DIE_IF_NULL(root),
              verible::matcher::DynamicTagMatchBuilder(
                  SymbolTag{SymbolKind::kNode,
                            static_cast<int>(test.expected_construct)})());

          // Extract subtree of interest.
          std::vector<TreeSearchMatch> bodies;
          for (const auto& statement : statements) {
            const auto* clause = GetAnyConditionalElseClause(*statement.match);
            bodies.push_back(TreeSearchMatch{clause, {/* ignored context */}});
          }
          return bodies;
        });
  }
}

TEST(FindAllForLoopsInitializations, FindForInitializationNames) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {""},
      {"module m;\nendmodule\n"},
      {"function int my_fun();\nint x = 0;\nfor (int ",
       {kTag, "i"},
       " = 0, ",
       {kTag, "j"},
       " = 0; i < 50; i++) begin\nx+=i;\nend\nreturn x;\nendfunction"},
      {"module m();\nint x = 0;\ninitial begin\nfor (int ",
       {kTag, "i"},
       " = 0, ",
       {kTag, "j"},
       " = 0, bit ",
       {kTag, "k"},
       " = 0; i < 50; i++) begin\nx+=i;\nend\nend\nendmodule"},
  };
  for (const auto& test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView& text_structure) {
          const auto& root = text_structure.SyntaxTree();
          const auto& instances =
              FindAllForLoopsInitializations(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> names;
          for (const auto& instance : instances) {
            const auto& variable_name =
                GetVariableNameFromForInitialization(*instance.match);
            names.emplace_back(
                TreeSearchMatch{&variable_name, {/* ignored context */}});
          }
          return names;
        });
  }
}

TEST(FindAllForLoopsInitializations, FindForInitializationDataTypes) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {""},
      {"module m;\nendmodule\n"},
      {"function int my_fun();\nint x = 0;\nfor (",
       {kTag, "int"},
       " i = 0, j = 0; i < 50; i++) begin\nx+=i;\nend\nreturn x;\nendfunction"},
      {"module m();\nint x = 0;\ninitial begin\nfor (",
       {kTag, "int"},
       " i = 0, j = 0, ",
       {kTag, "bit"},
       " k = 0; i < 50; i++) begin\nx+=i;\nend\nend\nendmodule"},
      {"module m();\nint x = 0;\ninitial begin\nfor (",
       {kTag, "int[x:y]"},
       " i = 0, j = 0, ",
       {kTag, "bit"},
       " k = 0; i < 50; i++) begin\nx+=i;\nend\nend\nendmodule"},
  };
  for (const auto& test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView& text_structure) {
          const auto& root = text_structure.SyntaxTree();
          const auto& instances =
              FindAllForLoopsInitializations(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> types;
          for (const auto& instance : instances) {
            const auto* type =
                GetDataTypeFromForInitialization(*instance.match);
            if (type == nullptr) {
              continue;
            }
            types.emplace_back(TreeSearchMatch{type, {/* ignored context */}});
          }
          return types;
        });
  }
}

TEST(FindAllForLoopsInitializations, FindForInitializationExpressions) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {""},
      {"module m;\nendmodule\n"},
      {"function int my_fun();\nint x = 0;\nfor (int i = ",
       {kTag, "0"},
       ", j = ",
       {kTag, "0"},
       "; i < 50;i++) begin\nx+=i;\nend\nreturn x;\nendfunction"},
      {"module m();\nint x = 0;\ninitial begin\nfor (int i = ",
       {kTag, "0"},
       ", j = ",
       {kTag, "y + x"},
       ", bit k = ",
       {kTag, "0"},
       "; i < 50;i++) begin\nx+=i;\nend\nend\nendmodule"},
  };
  for (const auto& test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView& text_structure) {
          const auto& root = text_structure.SyntaxTree();
          const auto& instances =
              FindAllForLoopsInitializations(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> expressions;
          for (const auto& instance : instances) {
            const auto& expression =
                GetExpressionFromForInitialization(*instance.match);
            expressions.emplace_back(
                TreeSearchMatch{&expression, {/* ignored context */}});
          }
          return expressions;
        });
  }
}

TEST(GetGenerateBlockBeginTest, Various) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {""},
      {"module m;\nendmodule\n"},
      {"module m;\n"
       "  wire k;\n"
       "endmodule\n"},
      {"module m;\n"
       "  if (1)\n"
       "    wire www;\n"
       "endmodule\n"},
      {"module m;\n"
       "  if (1) ",
       {kTag, "begin"},
       "\n"
       "  end\n"
       "endmodule\n"},
      {"module m;\n"
       "  if (1) ",
       {kTag, "begin : my_label"},
       "\n"
       "  end : my_label\n"
       "endmodule\n"},
      {"module m;\n"
       "  if (1) ",
       {kTag, "begin"},
       "\n"
       "  end else if (2) ",
       {kTag, "begin:foo"},
       "\n"
       "  end\n"
       "endmodule\n"},
      {"module m;\n"
       "  for (genvar i=0; i<N; ++i) ",
       {kTag, "begin"},
       "\n"
       "  end\n"
       "endmodule\n"},
  };
  for (const auto& test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView& text_structure) {
          const auto& root = text_structure.SyntaxTree();
          const auto& blocks = FindAllGenerateBlocks(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> begins;
          for (const auto& block : blocks) {
            const auto& begin = GetGenerateBlockBegin(*block.match);
            begins.emplace_back(
                TreeSearchMatch{&begin, {/* ignored context */}});
          }
          return begins;
        });
  }
}

TEST(GetGenerateBlockEndTest, Various) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {""},
      {"module m;\nendmodule\n"},
      {"module m;\n"
       "  wire k;\n"
       "endmodule\n"},
      {"module m;\n"
       "  if (1)\n"
       "    wire www;\n"
       "endmodule\n"},
      {"module m;\n"
       "  if (1) begin\n"
       "  ",
       {kTag, "end"},
       "\n"
       "endmodule\n"},
      {"module m;\n"
       "  if (1) begin : my_label\n"
       "  ",
       {kTag, "end : my_label"},
       "\n"
       "endmodule\n"},
      {"module m;\n"
       "  if (1) begin : my_label\n"
       "  ",
       {kTag, "end : my_label"},
       "\n"
       "  else if (2) begin : your_label\n"
       "  ",
       {kTag, "end : your_label"},
       "\n"
       "endmodule\n"},
      {"module m;\n"
       "  for (genvar i=0; i<N; ++i) begin\n"
       "  ",
       {kTag, "end"},
       "\n"
       "endmodule\n"},
  };
  for (const auto& test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView& text_structure) {
          const auto& root = text_structure.SyntaxTree();
          const auto& blocks = FindAllGenerateBlocks(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> ends;
          for (const auto& block : blocks) {
            const auto& end = GetGenerateBlockEnd(*block.match);
            ends.emplace_back(TreeSearchMatch{&end, {/* ignored context */}});
          }
          return ends;
        });
  }
}

}  // namespace
}  // namespace verilog
