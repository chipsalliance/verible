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

#include "verible/verilog/CST/statement.h"

#include <memory>
#include <string_view>
#include <vector>

#include "absl/log/die_if_null.h"
#include "gtest/gtest.h"
#include "verible/common/analysis/matcher/matcher-builders.h"
#include "verible/common/analysis/syntax-tree-search-test-utils.h"
#include "verible/common/analysis/syntax-tree-search.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/text-structure.h"
#include "verible/common/text/tree-utils.h"
#include "verible/common/util/logging.h"
#include "verible/verilog/CST/match-test-utils.h"
#include "verible/verilog/CST/verilog-matchers.h"
#include "verible/verilog/CST/verilog-nonterminals.h"
#include "verible/verilog/analysis/verilog-analyzer.h"

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
      {.expected_construct = NodeEnum::kGenerateIfClause,
       .token_data = {"module m;\n"
                      "  if (expr)\n",
                      {kTag, ";"},  // null generate item
                      "\n"
                      "  else \n"
                      "   bar foo;\n"
                      "endmodule\n"}},
      {.expected_construct = NodeEnum::kGenerateIfClause,
       .token_data = {"module m;\n"
                      "  if (expr)\n"
                      "   ",
                      {kTag, "foo bar;"},
                      "\n"
                      "  else \n"
                      "   bar foo;\n"
                      "endmodule\n"}},
      {.expected_construct = NodeEnum::kGenerateIfClause,
       .token_data = {"module m;\n"
                      "  if (expr)\n"
                      "   ",
                      {kTag, "begin\nfoo bar;end"},
                      "\n"
                      "  else \n"
                      "   bar foo;\n"
                      "endmodule\n"}},

      {.expected_construct = NodeEnum::kGenerateElseClause,
       .token_data = {"module m;\n"
                      "  if (expr)\n"
                      "   foo bar;\n"
                      "  else \n",
                      {kTag, ";"},  // null generate item
                      "\n"
                      "endmodule\n"}},
      {.expected_construct = NodeEnum::kGenerateElseClause,
       .token_data = {"module m;\n"
                      "  if (expr)\n"
                      "   foo bar;\n"
                      "  else \n",
                      {kTag, "bar#(1)   foo;"},
                      "\n"
                      "endmodule\n"}},
      {.expected_construct = NodeEnum::kGenerateElseClause,
       .token_data = {"module m;\n"
                      "  if (expr)\n"
                      "   foo bar;\n"
                      "  else \n",
                      {kTag, "begin \nbar#(1)   foo; baz bam();\nend"},
                      "\n"
                      "endmodule\n"}},

      {.expected_construct = NodeEnum::kLoopGenerateConstruct,
       .token_data = {"module m;\n"
                      "  for (genvar i=0; i<N; ++i)\n"
                      "   ",
                      {kTag, ";"},  // null generate item
                      "\n"
                      "endmodule\n"}},
      {.expected_construct = NodeEnum::kLoopGenerateConstruct,
       .token_data = {"module m;\n"
                      "  for (genvar i=0; i<N; ++i)\n"
                      "   ",
                      {kTag, "foo#(.N(i)) bar;"},
                      "\n"
                      "endmodule\n"}},
      {.expected_construct = NodeEnum::kLoopGenerateConstruct,
       .token_data = {"module m;\n"
                      "  for (genvar i=0; i<N; ++i)\n"
                      "   ",
                      {kTag, "begin:l1\n      foo#(.N(i)) bar;\n  end : l1"},
                      "\n"
                      "endmodule\n"}},

      {.expected_construct = NodeEnum::kIfClause,
       .token_data = {"function f;\n"
                      "  if (expr)\n",
                      {kTag, ";"},  // null statement
                      "\n"
                      "  else \n"
                      "   bar=foo;\n"
                      "endfunction\n"}},
      {.expected_construct = NodeEnum::kIfClause,
       .token_data = {"function f;\n"
                      "  if (expr)\n"
                      "   ",
                      {kTag, "foo=bar;"},
                      "\n"
                      "  else \n"
                      "   bar=foo;\n"
                      "endfunction\n"}},
      {.expected_construct = NodeEnum::kIfClause,
       .token_data = {"task t;\n"
                      "  if (expr)\n"
                      "   ",
                      {kTag, "begin\nfoo=bar; bar=1;\nend"},
                      "\n"
                      "  else \n"
                      "   bar=foo;\n"
                      "endtask\n"}},

      {.expected_construct = NodeEnum::kElseClause,
       .token_data = {"task t;\n"
                      "  if (expr)\n"
                      "   foo =bar;\n"
                      "\n"
                      "  else\n",
                      {kTag, "bar=foo;"},
                      "endtask\n"}},
      {.expected_construct = NodeEnum::kElseClause,
       .token_data = {"task t;\n"
                      "  if (expr)\n"
                      "   foo =bar;\n"
                      "\n"
                      "  else\n",
                      {kTag, ";"},  // null statement
                      "endtask\n"}},
      {.expected_construct = NodeEnum::kElseClause,
       .token_data = {"function f;\n"
                      "  if (expr)\n"
                      "   foo =bar;\n"
                      "  else\n",
                      {kTag, "begin:bb bar=foo(baz);\n\nend :\nbb"},
                      "\nendfunction\n"}},

      {.expected_construct = NodeEnum::kForLoopStatement,
       .token_data = {"function f;\n"
                      "  for (int j=N; expr; --j)\n"
                      "   ",
                      {kTag, ";"},  // null statement
                      "\n"
                      "endfunction\n"}},
      {.expected_construct = NodeEnum::kForLoopStatement,
       .token_data = {"function f;\n"
                      "  for (int j=N; expr; --j)\n"
                      "   ",
                      {kTag, "foo=bar;"},
                      "\n"
                      "endfunction\n"}},
      {.expected_construct = NodeEnum::kForLoopStatement,
       .token_data = {"task t;\n"
                      "  for (int j=N; expr; --j)\n"
                      "   ",
                      {kTag, "begin\nfoo=bar; bar=1;\nend"},
                      "\n"
                      "endtask\n"}},

      {.expected_construct = NodeEnum::kDoWhileLoopStatement,
       .token_data = {"function f;\n"
                      "   do\n",
                      {kTag, ";"},  // null statement
                      "  while (expr);\n"
                      "endfunction\n"}},
      {.expected_construct = NodeEnum::kDoWhileLoopStatement,
       .token_data = {"function f;\n"
                      "   do\n",
                      {kTag, "foo=bar;"},
                      "  while (expr);\n"
                      "endfunction\n"}},
      {.expected_construct = NodeEnum::kDoWhileLoopStatement,
       .token_data = {"task t;\n"
                      "  do ",
                      {kTag, "begin\nfoo=bar; bar=1;\nend"},
                      "  while (expr);\n"
                      "endtask\n"}},

      {.expected_construct = NodeEnum::kForeverLoopStatement,
       .token_data = {"function f;\n"
                      "  forever\n",
                      {kTag, ";"},  // null statement
                      "\n"
                      "endfunction\n"}},
      {.expected_construct = NodeEnum::kForeverLoopStatement,
       .token_data = {"function f;\n"
                      "  forever\n"
                      "   ",
                      {kTag, "foo=bar;"},
                      "\n"
                      "endfunction\n"}},
      {.expected_construct = NodeEnum::kForeverLoopStatement,
       .token_data = {"task t;\n"
                      "  forever\n"
                      "   ",
                      {kTag, "begin\nfoo=bar; bar=1;\nend"},
                      "\n"
                      "endtask\n"}},

      {.expected_construct = NodeEnum::kForeachLoopStatement,
       .token_data = {"function f;\n"
                      "  foreach (x[i])\n",
                      {kTag, ";"},  // null statement
                      "\n"
                      "endfunction\n"}},
      {.expected_construct = NodeEnum::kForeachLoopStatement,
       .token_data = {"function f;\n"
                      "  foreach (x[i])\n"
                      "   ",
                      {kTag, "foo=bar;"},
                      "\n"
                      "endfunction\n"}},
      {.expected_construct = NodeEnum::kForeachLoopStatement,
       .token_data = {"task t;\n"
                      "  foreach (x[i])\n"
                      "   ",
                      {kTag, "begin\nfoo=bar; bar=1;\nend"},
                      "\n"
                      "endtask\n"}},

      {.expected_construct = NodeEnum::kRepeatLoopStatement,
       .token_data = {"function f;\n"
                      "  repeat (8)\n",
                      {kTag, ";"},  // null statement
                      "\n"
                      "endfunction\n"}},
      {.expected_construct = NodeEnum::kRepeatLoopStatement,
       .token_data = {"function f;\n"
                      "  repeat (8)\n"
                      "   ",
                      {kTag, "foo=bar;"},
                      "\n"
                      "endfunction\n"}},
      {.expected_construct = NodeEnum::kRepeatLoopStatement,
       .token_data = {"task t;\n"
                      "  repeat (9)\n"
                      "   ",
                      {kTag, "begin\nfoo=bar; bar=1;\nend"},
                      "\n"
                      "endtask\n"}},

      {.expected_construct = NodeEnum::kWhileLoopStatement,
       .token_data = {"function f;\n"
                      "  while (expr)\n",
                      {kTag, ";"},  // null statement
                      "\n"
                      "endfunction\n"}},
      {.expected_construct = NodeEnum::kWhileLoopStatement,
       .token_data = {"function f;\n"
                      "  while (expr)\n"
                      "   ",
                      {kTag, "foo=bar;"},
                      "\n"
                      "endfunction\n"}},
      {.expected_construct = NodeEnum::kWhileLoopStatement,
       .token_data = {"task t;\n"
                      "  while (expr)\n"
                      "   ",
                      {kTag, "begin\nfoo=bar; bar=1;\nend"},
                      "\n"
                      "endtask\n"}},

      {.expected_construct = NodeEnum::kProceduralTimingControlStatement,
       .token_data = {"module  m;\n"
                      "  always @(negedge c)\n",
                      {kTag, ";"},  // null statement
                      "\n"
                      "endmodule\n"}},
      {.expected_construct = NodeEnum::kProceduralTimingControlStatement,
       .token_data = {"module  m;\n"
                      "  always @(negedge c)\n",
                      {kTag, "foo=bar;"},
                      "\n"
                      "endmodule\n"}},
      {.expected_construct = NodeEnum::kProceduralTimingControlStatement,
       .token_data = {"module  m;\n"
                      "  always @(negedge c)\n",
                      {kTag, "begin\nfoo=bar; bar=1;\nend"},
                      "\n"
                      "endmodule\n"}},

      {.expected_construct = NodeEnum::kAssertionClause,
       .token_data = {"task  t;\n"
                      "  assert (expr)\n",
                      {kTag, ";"},  // null statement
                      "\n"
                      "endtask\n"}},
      {.expected_construct = NodeEnum::kAssertionClause,
       .token_data = {"task  t;\n"
                      "  assert (expr)\n",
                      {kTag, "action();"},
                      "\n"
                      "endtask\n"}},
      {.expected_construct = NodeEnum::kAssertionClause,
       .token_data = {"task  t;\n"
                      "  assert (expr)\n",
                      {kTag, "begin action(); end"},
                      "\n"
                      "endtask\n"}},

      {.expected_construct = NodeEnum::kAssumeClause,
       .token_data = {"task  t;\n"
                      "  assume (expr)\n",
                      {kTag, ";"},  // null statement
                      "\n"
                      "endtask\n"}},
      {.expected_construct = NodeEnum::kAssumeClause,
       .token_data = {"task  t;\n"
                      "  assume (expr)\n",
                      {kTag, "action();"},
                      "\n"
                      "endtask\n"}},
      {.expected_construct = NodeEnum::kAssumeClause,
       .token_data = {"task  t;\n"
                      "  assume (expr)\n",
                      {kTag, "begin action(); end"},
                      "\n"
                      "endtask\n"}},

      {.expected_construct = NodeEnum::kWaitStatement,
       .token_data = {"task  t;\n"
                      "  wait (expr)\n",
                      {kTag, ";"},  // null statement
                      "\n"
                      "endtask\n"}},
      {.expected_construct = NodeEnum::kWaitStatement,
       .token_data = {"task  t;\n"
                      "  wait (expr)\n",
                      {kTag, "snooze();"},
                      "\n"
                      "endtask\n"}},
      {.expected_construct = NodeEnum::kWaitStatement,
       .token_data = {"task  t;\n"
                      "  wait (expr)\n",
                      {kTag, "begin snooze(); end"},
                      "\n"
                      "endtask\n"}},

      {.expected_construct = NodeEnum::kCoverStatement,
       .token_data = {"task  t;\n"
                      "  cover (expr)\n",
                      {kTag, ";"},  // null statement
                      "\n"
                      "endtask\n"}},
      {.expected_construct = NodeEnum::kCoverStatement,
       .token_data = {"task  t;\n"
                      "  cover (expr)\n",
                      {kTag, "snooze();"},
                      "\n"
                      "endtask\n"}},
      {.expected_construct = NodeEnum::kCoverStatement,
       .token_data = {"task  t;\n"
                      "  cover (expr)\n",
                      {kTag, "begin snooze(); end"},
                      "\n"
                      "endtask\n"}},

      {.expected_construct = NodeEnum::kAssertPropertyClause,
       .token_data = {"task  t;\n"
                      "  assert property (p_expr)\n",
                      {kTag, ";"},  // null statement
                      "\n"
                      "endtask\n"}},
      {.expected_construct = NodeEnum::kAssertPropertyClause,
       .token_data = {"task  t;\n"
                      "  assert property (p_expr)\n",
                      {kTag, "action();"},
                      "\n"
                      "endtask\n"}},
      {.expected_construct = NodeEnum::kAssertPropertyClause,
       .token_data = {"task  t;\n"
                      "  assert property (p_expr)\n",
                      {kTag, "begin action(); end"},
                      "\n"
                      "endtask\n"}},

      {.expected_construct = NodeEnum::kAssumePropertyClause,
       .token_data = {"task  t;\n"
                      "  assume property (p_expr)\n",
                      {kTag, ";"},  // null statement
                      "\n"
                      "endtask\n"}},
      {.expected_construct = NodeEnum::kAssumePropertyClause,
       .token_data = {"task  t;\n"
                      "  assume property (p_expr)\n",
                      {kTag, "action();"},
                      "\n"
                      "endtask\n"}},
      {.expected_construct = NodeEnum::kAssumePropertyClause,
       .token_data = {"task  t;\n"
                      "  assume property (p_expr)\n",
                      {kTag, "begin action(); end"},
                      "\n"
                      "endtask\n"}},

      {.expected_construct = NodeEnum::kExpectPropertyClause,
       .token_data = {"task  t;\n"
                      "  expect (p_expr)\n",
                      {kTag, ";"},  // null statement
                      "\n"
                      "endtask\n"}},
      {.expected_construct = NodeEnum::kExpectPropertyClause,
       .token_data = {"task  t;\n"
                      "  expect (p_expr)\n",
                      {kTag, "action();"},
                      "\n"
                      "endtask\n"}},
      {.expected_construct = NodeEnum::kExpectPropertyClause,
       .token_data = {"task  t;\n"
                      "  expect (p_expr)\n",
                      {kTag, "begin action(); end"},
                      "\n"
                      "endtask\n"}},

      {.expected_construct = NodeEnum::kCoverPropertyStatement,
       .token_data = {"task  t;\n"
                      "  cover property (p_expr)\n",
                      {kTag, ";"},  // null statement
                      "\n"
                      "endtask\n"}},
      {.expected_construct = NodeEnum::kCoverPropertyStatement,
       .token_data = {"task  t;\n"
                      "  cover property (p_expr)\n",
                      {kTag, "action();"},
                      "\n"
                      "endtask\n"}},
      {.expected_construct = NodeEnum::kCoverPropertyStatement,
       .token_data = {"task  t;\n"
                      "  cover property (p_expr)\n",
                      {kTag, "begin action(); end"},
                      "\n"
                      "endtask\n"}},

      {.expected_construct = NodeEnum::kCoverSequenceStatement,
       .token_data = {"task  t;\n"
                      "  cover sequence (s_expr)\n",
                      {kTag, ";"},  // null statement
                      "\n"
                      "endtask\n"}},
      {.expected_construct = NodeEnum::kCoverSequenceStatement,
       .token_data = {"task  t;\n"
                      "  cover sequence (s_expr)\n",
                      {kTag, "action();"},
                      "\n"
                      "endtask\n"}},
      {.expected_construct = NodeEnum::kCoverSequenceStatement,
       .token_data = {"task  t;\n"
                      "  cover sequence (s_expr)\n",
                      {kTag, "begin action(); end"},
                      "\n"
                      "endtask\n"}},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test.token_data,
        [&test](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();

          // Grab outer statement constructs.
          const auto statements = verible::SearchSyntaxTree(
              *ABSL_DIE_IF_NULL(root),
              verible::matcher::DynamicTagMatchBuilder(SymbolTag{
                  .kind = SymbolKind::kNode,
                  .tag = static_cast<int>(test.expected_construct)})());

          // Extract subtree of interest.
          std::vector<TreeSearchMatch> bodies;
          for (const auto &statement : statements) {
            const auto *body = GetAnyControlStatementBody(*statement.match);
            bodies.push_back(TreeSearchMatch{
                .match = body, .context = {/* ignored context */}});
          }
          return bodies;
        });
  }
}

TEST(GetAnyConditionalIfClauseTest, Various) {
  const ControlStatementTestData kTestCases[] = {
      // each of these test cases should match exactly one statement body
      {.expected_construct = NodeEnum::kConditionalGenerateConstruct,
       .token_data = {"module m;\n",
                      {static_cast<int>(NodeEnum::kGenerateIfClause),
                       "if (expr);"},
                      "\n"
                      "endmodule\n"}},
      {.expected_construct = NodeEnum::kConditionalGenerateConstruct,
       .token_data = {"module m;\n",
                      {static_cast<int>(NodeEnum::kGenerateIfClause),
                       "if (expr) foo bar;"},
                      "\n"
                      "endmodule\n"}},
      {.expected_construct = NodeEnum::kConditionalGenerateConstruct,
       .token_data = {"module m;\n",
                      {static_cast<int>(NodeEnum::kGenerateIfClause),
                       "if (expr) foo bar;"},
                      "\n"
                      "  else \n"
                      "   bar foo;\n"
                      "endmodule\n"}},
      {.expected_construct = NodeEnum::kConditionalGenerateConstruct,
       .token_data = {"module m;\n",
                      {static_cast<int>(NodeEnum::kGenerateIfClause),
                       "if (expr) begin\nfoo bar;end"},
                      "\n"
                      "  else \n"
                      "   bar foo;\n"
                      "endmodule\n"}},

      {.expected_construct = NodeEnum::kConditionalStatement,
       .token_data = {"function f;\n",
                      {static_cast<int>(NodeEnum::kIfClause), "if ( expr );"},
                      "\n"
                      "endfunction\n"}},
      {.expected_construct = NodeEnum::kConditionalStatement,
       .token_data = {"function f;\n",
                      {static_cast<int>(NodeEnum::kIfClause),
                       "if ( expr ) foo=bar;"},
                      "\n"
                      "endfunction\n"}},
      {.expected_construct = NodeEnum::kConditionalStatement,
       .token_data = {"function f;\n",
                      {static_cast<int>(NodeEnum::kIfClause),
                       "if ( expr ) foo=bar;"},
                      "\n"
                      "  else \n"
                      "   bar=foo;\n"
                      "endfunction\n"}},
      {.expected_construct = NodeEnum::kConditionalStatement,
       .token_data = {"task t;\n",
                      {static_cast<int>(NodeEnum::kIfClause),
                       "if  (expr)begin\nfoo=bar; bar=1;\nend"},
                      "\n"
                      "  else \n"
                      "   bar=foo;\n"
                      "endtask\n"}},

      {.expected_construct = NodeEnum::kAssertionStatement,
       .token_data = {"function f;\n",
                      {static_cast<int>(NodeEnum::kAssertionClause),
                       "assert ( expr );"},  // null statement
                      "\n"
                      "endfunction\n"}},
      {.expected_construct = NodeEnum::kAssertionStatement,
       .token_data = {"function f;\n",
                      {static_cast<int>(NodeEnum::kAssertionClause),
                       "assert ( expr ) foo=bar;"},
                      "\n"
                      "endfunction\n"}},
      {.expected_construct = NodeEnum::kAssertionStatement,
       .token_data = {"function f;\n",
                      {static_cast<int>(NodeEnum::kAssertionClause),
                       "assert ( expr ) foo=bar;"},
                      "\n"
                      "  else \n"
                      "   bar=foo;\n"
                      "endfunction\n"}},
      {.expected_construct = NodeEnum::kAssertionStatement,
       .token_data = {"task t;\n",
                      {static_cast<int>(NodeEnum::kAssertionClause),
                       "assert  (expr)begin\nfoo=bar; bar=1;\nend"},
                      "\n"
                      "  else \n"
                      "   bar=foo;\n"
                      "endtask\n"}},

      {.expected_construct = NodeEnum::kAssumeStatement,
       .token_data = {"function f;\n",
                      {static_cast<int>(NodeEnum::kAssumeClause),
                       "assume ( expr );"},
                      "\n"
                      "endfunction\n"}},
      {.expected_construct = NodeEnum::kAssumeStatement,
       .token_data = {"function f;\n",
                      {static_cast<int>(NodeEnum::kAssumeClause),
                       "assume ( expr ) foo=bar;"},
                      "\n"
                      "endfunction\n"}},
      {.expected_construct = NodeEnum::kAssumeStatement,
       .token_data = {"function f;\n",
                      {static_cast<int>(NodeEnum::kAssumeClause),
                       "assume ( expr ) foo=bar;"},
                      "\n"
                      "  else \n"
                      "   bar=foo;\n"
                      "endfunction\n"}},
      {.expected_construct = NodeEnum::kAssumeStatement,
       .token_data = {"task t;\n",
                      {static_cast<int>(NodeEnum::kAssumeClause),
                       "assume  (expr)begin\nfoo=bar; bar=1;\nend"},
                      "\n"
                      "  else \n"
                      "   bar=foo;\n"
                      "endtask\n"}},

      {.expected_construct = NodeEnum::kAssertPropertyStatement,
       .token_data = {"task t;\n",
                      {static_cast<int>(NodeEnum::kAssertPropertyClause),
                       "assert property ( p_expr );"},  // null statement
                      "\n"
                      "endtask\n"}},
      {.expected_construct = NodeEnum::kAssertPropertyStatement,
       .token_data = {"task t;\n",
                      {static_cast<int>(NodeEnum::kAssertPropertyClause),
                       "assert property ( p_expr ) foo=bar;"},
                      "\n"
                      "endtask\n"}},
      {.expected_construct = NodeEnum::kAssertPropertyStatement,
       .token_data = {"task t;\n",
                      {static_cast<int>(NodeEnum::kAssertPropertyClause),
                       "assert property ( p_expr ) foo=bar;"},
                      "\n"
                      "  else \n"
                      "   bar=foo;\n"
                      "endtask\n"}},
      {.expected_construct = NodeEnum::kAssertPropertyStatement,
       .token_data = {"task t;\n",
                      {static_cast<int>(NodeEnum::kAssertPropertyClause),
                       "assert property  (p_expr)begin\nfoo=bar; bar=1;\nend"},
                      "\n"
                      "  else \n"
                      "   bar=foo;\n"
                      "endtask\n"}},

      {.expected_construct = NodeEnum::kAssumePropertyStatement,
       .token_data = {"task t;\n",
                      {static_cast<int>(NodeEnum::kAssumePropertyClause),
                       "assume property ( p_expr );"},  // null statement
                      "\n"
                      "endtask\n"}},
      {.expected_construct = NodeEnum::kAssumePropertyStatement,
       .token_data = {"task t;\n",
                      {static_cast<int>(NodeEnum::kAssumePropertyClause),
                       "assume property ( p_expr ) foo=bar;"},
                      "\n"
                      "endtask\n"}},
      {.expected_construct = NodeEnum::kAssumePropertyStatement,
       .token_data = {"task t;\n",
                      {static_cast<int>(NodeEnum::kAssumePropertyClause),
                       "assume property ( p_expr ) foo=bar;"},
                      "\n"
                      "  else \n"
                      "   bar=foo;\n"
                      "endtask\n"}},
      {.expected_construct = NodeEnum::kAssumePropertyStatement,
       .token_data = {"task t;\n",
                      {static_cast<int>(NodeEnum::kAssumePropertyClause),
                       "assume property  (p_expr)begin\nfoo=bar; bar=1;\nend"},
                      "\n"
                      "  else \n"
                      "   bar=foo;\n"
                      "endtask\n"}},

      {.expected_construct = NodeEnum::kExpectPropertyStatement,
       .token_data = {"task t;\n",
                      {static_cast<int>(NodeEnum::kExpectPropertyClause),
                       "expect ( p_expr );"},  // null statement
                      "\n"
                      "endtask\n"}},
      {.expected_construct = NodeEnum::kExpectPropertyStatement,
       .token_data = {"task t;\n",
                      {static_cast<int>(NodeEnum::kExpectPropertyClause),
                       "expect ( p_expr ) foo=bar;"},
                      "\n"
                      "endtask\n"}},
      {.expected_construct = NodeEnum::kExpectPropertyStatement,
       .token_data = {"task t;\n",
                      {static_cast<int>(NodeEnum::kExpectPropertyClause),
                       "expect ( p_expr ) foo=bar;"},
                      "\n"
                      "  else \n"
                      "   bar=foo;\n"
                      "endtask\n"}},
      {.expected_construct = NodeEnum::kExpectPropertyStatement,
       .token_data = {"task t;\n",
                      {static_cast<int>(NodeEnum::kExpectPropertyClause),
                       "expect (p_expr)begin\nfoo=bar; bar=1;\nend"},
                      "\n"
                      "  else \n"
                      "   bar=foo;\n"
                      "endtask\n"}},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test.token_data,
        [&test](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();

          // Grab outer statement constructs.
          const auto statements = verible::SearchSyntaxTree(
              *ABSL_DIE_IF_NULL(root),
              verible::matcher::DynamicTagMatchBuilder(SymbolTag{
                  .kind = SymbolKind::kNode,
                  .tag = static_cast<int>(test.expected_construct)})());

          // Extract subtree of interest.
          std::vector<TreeSearchMatch> bodies;
          for (const auto &statement : statements) {
            const auto *clause = GetAnyConditionalIfClause(*statement.match);
            bodies.push_back(TreeSearchMatch{
                .match = clause, .context = {/* ignored context */}});
          }
          return bodies;
        });
  }
}

TEST(GetAnyConditionalElseClauseTest, NoElseClause) {
  const ControlStatementTestData kTestCases[] = {
      // each of these test cases should match exactly one statement body
      {.expected_construct = NodeEnum::kConditionalGenerateConstruct,
       .token_data = {"module m;\n",
                      {static_cast<int>(NodeEnum::kGenerateIfClause),
                       "if (expr);"},
                      "\n"
                      "endmodule\n"}},
      {.expected_construct = NodeEnum::kConditionalGenerateConstruct,
       .token_data = {"module m;\n",
                      {static_cast<int>(NodeEnum::kGenerateIfClause),
                       "if (expr) foo bar;"},
                      "\n"
                      "endmodule\n"}},

      {.expected_construct = NodeEnum::kConditionalStatement,
       .token_data = {"function f;\n",
                      {static_cast<int>(NodeEnum::kIfClause), "if ( expr );"},
                      "\n"
                      "endfunction\n"}},
      {.expected_construct = NodeEnum::kConditionalStatement,
       .token_data = {"function f;\n",
                      {static_cast<int>(NodeEnum::kIfClause),
                       "if ( expr ) foo=bar;"},
                      "\n"
                      "endfunction\n"}},

      {.expected_construct = NodeEnum::kAssertionStatement,
       .token_data = {"task t;\n",
                      {static_cast<int>(NodeEnum::kAssertionClause),
                       "assert ( expr );"},
                      "\n"
                      "endtask\n"}},
      {.expected_construct = NodeEnum::kAssertionStatement,
       .token_data = {"task t;\n",
                      {static_cast<int>(NodeEnum::kAssertionClause),
                       "assert ( expr ) foo=bar;"},
                      "\n"
                      "endtask\n"}},

      {.expected_construct = NodeEnum::kAssumeStatement,
       .token_data = {"task t;\n",
                      {static_cast<int>(NodeEnum::kAssumeClause),
                       "assume ( expr );"},
                      "\n"
                      "endtask\n"}},
      {.expected_construct = NodeEnum::kAssumeStatement,
       .token_data = {"task t;\n",
                      {static_cast<int>(NodeEnum::kAssumeClause),
                       "assume ( expr ) foo=bar;"},
                      "\n"
                      "endtask\n"}},

      {.expected_construct = NodeEnum::kAssertPropertyStatement,
       .token_data = {"task t;\n",
                      {static_cast<int>(NodeEnum::kAssertPropertyClause),
                       "assert property( expr );"},
                      "\n"
                      "endtask\n"}},
      {.expected_construct = NodeEnum::kAssertPropertyStatement,
       .token_data = {"task t;\n",
                      {static_cast<int>(NodeEnum::kAssertPropertyClause),
                       "assert property ( expr ) foo=bar;"},
                      "\n"
                      "endtask\n"}},

      {.expected_construct = NodeEnum::kAssumePropertyStatement,
       .token_data = {"task t;\n",
                      {static_cast<int>(NodeEnum::kAssumePropertyClause),
                       "assume property( expr );"},
                      "\n"
                      "endtask\n"}},
      {.expected_construct = NodeEnum::kAssumePropertyStatement,
       .token_data = {"task t;\n",
                      {static_cast<int>(NodeEnum::kAssumePropertyClause),
                       "assume property ( expr ) foo=bar;"},
                      "\n"
                      "endtask\n"}},

      {.expected_construct = NodeEnum::kExpectPropertyStatement,
       .token_data = {"task t;\n",
                      {static_cast<int>(NodeEnum::kExpectPropertyClause),
                       "expect( expr );"},
                      "\n"
                      "endtask\n"}},
      {.expected_construct = NodeEnum::kExpectPropertyStatement,
       .token_data = {"task t;\n",
                      {static_cast<int>(NodeEnum::kExpectPropertyClause),
                       "expect ( expr ) foo=bar;"},
                      "\n"
                      "endtask\n"}},
  };
  for (const auto &test : kTestCases) {
    const std::string_view code(test.token_data.code);
    VerilogAnalyzer analyzer(code, "test-file");
    ASSERT_OK(analyzer.Analyze()) << "failed on:\n" << code;
    const auto &root = analyzer.Data().SyntaxTree();

    const auto statements = verible::SearchSyntaxTree(
        *ABSL_DIE_IF_NULL(root),
        verible::matcher::DynamicTagMatchBuilder(
            SymbolTag{.kind = SymbolKind::kNode,
                      .tag = static_cast<int>(test.expected_construct)})());
    ASSERT_EQ(statements.size(), 1);
    const auto &statement = *statements.front().match;
    const auto *clause = GetAnyConditionalElseClause(statement);
    EXPECT_EQ(clause, nullptr);
  }
}

TEST(GetAnyConditionalElseClauseTest, HaveElseClause) {
  const ControlStatementTestData kTestCases[] = {
      // each of these test cases should match exactly one statement body
      {.expected_construct = NodeEnum::kConditionalGenerateConstruct,
       .token_data = {"module m;\n",
                      "if (expr);\n",
                      {static_cast<int>(NodeEnum::kGenerateElseClause),
                       "else \n"
                       "   ;"},  // null else body
                      "\n"
                      "endmodule\n"}},
      {.expected_construct = NodeEnum::kConditionalGenerateConstruct,
       .token_data = {"module m;\n",
                      "if (expr);\n",
                      {static_cast<int>(NodeEnum::kGenerateElseClause),
                       "else \n"
                       "   bar foo;"},
                      "\n"
                      "endmodule\n"}},
      {.expected_construct = NodeEnum::kConditionalGenerateConstruct,
       .token_data = {"module m;\n",
                      "if (expr) foo bar;\n",
                      {static_cast<int>(NodeEnum::kGenerateElseClause),
                       "else \n"
                       "   bar foo;"},
                      "\n"
                      "endmodule\n"}},
      {.expected_construct = NodeEnum::kConditionalGenerateConstruct,
       .token_data = {"module m;\n",
                      "if (expr) foo bar;\n",
                      {static_cast<int>(NodeEnum::kGenerateElseClause),
                       "else \n"
                       "   begin bar foo;\nend"},
                      "\n"
                      "endmodule\n"}},

      {.expected_construct = NodeEnum::kConditionalStatement,
       .token_data = {"function f;\n",
                      "if ( expr );\n",
                      {static_cast<int>(NodeEnum::kElseClause),
                       "else \n"
                       "   ;"},  // null else body
                      "\n"
                      "endfunction\n"}},
      {.expected_construct = NodeEnum::kConditionalStatement,
       .token_data = {"function f;\n",
                      "if ( expr );\n",
                      {static_cast<int>(NodeEnum::kElseClause),
                       "else \n"
                       "   bar=foo;"},
                      "\n"
                      "endfunction\n"}},
      {.expected_construct = NodeEnum::kConditionalStatement,
       .token_data = {"function f;\n",
                      "if ( expr ) foo=bar;\n",
                      {static_cast<int>(NodeEnum::kElseClause),
                       "else \n"
                       "   bar=foo;"},
                      "\n"
                      "endfunction\n"}},
      {.expected_construct = NodeEnum::kConditionalStatement,
       .token_data = {"function f;\n",
                      "if ( expr ) foo=bar;\n",
                      {static_cast<int>(NodeEnum::kElseClause),
                       "else \n"
                       "   begin\nbar=foo;\nend"},
                      "\n"
                      "endfunction\n"}},

      {.expected_construct = NodeEnum::kAssertionStatement,
       .token_data = {"task t;\n",
                      "assert ( expr )\n",  // no statement
                      {static_cast<int>(NodeEnum::kElseClause),
                       "else \n"
                       "   bar=foo;"},
                      "\n"
                      "endtask\n"}},
      {.expected_construct = NodeEnum::kAssertionStatement,
       .token_data = {"task t;\n",
                      "assert ( expr );\n",  // null statement
                      {static_cast<int>(NodeEnum::kElseClause),
                       "else \n"
                       "   bar=foo;"},
                      "\n"
                      "endtask\n"}},
      {.expected_construct = NodeEnum::kAssertionStatement,
       .token_data = {"task t;\n",
                      "assert ( expr ) foo=bar;\n",
                      {static_cast<int>(NodeEnum::kElseClause),
                       "else \n"
                       "   ;"},  // null else body
                      "\n"
                      "endtask\n"}},
      {.expected_construct = NodeEnum::kAssertionStatement,
       .token_data = {"task t;\n",
                      "assert ( expr ) foo=bar;\n",
                      {static_cast<int>(NodeEnum::kElseClause),
                       "else \n"
                       "   bar=foo;"},
                      "\n"
                      "endtask\n"}},
      {.expected_construct = NodeEnum::kAssertionStatement,
       .token_data = {"task t;\n",
                      "assert ( expr ) foo=bar;\n",
                      {static_cast<int>(NodeEnum::kElseClause),
                       "else \n"
                       "   begin\nbar=foo;\nend"},
                      "\n"
                      "endtask\n"}},

      {.expected_construct = NodeEnum::kAssumeStatement,
       .token_data = {"task t;\n",
                      "assume ( expr )\n",  // no statement
                      {static_cast<int>(NodeEnum::kElseClause),
                       "else \n"
                       "   bar=foo;"},
                      "\n"
                      "endtask\n"}},
      {.expected_construct = NodeEnum::kAssumeStatement,
       .token_data = {"task t;\n",
                      "assume ( expr );\n",  // null statement
                      {static_cast<int>(NodeEnum::kElseClause),
                       "else \n"
                       "   bar=foo;"},
                      "\n"
                      "endtask\n"}},
      {.expected_construct = NodeEnum::kAssumeStatement,
       .token_data = {"task t;\n",
                      "assume ( expr ) foo=bar;\n",
                      {static_cast<int>(NodeEnum::kElseClause),
                       "else \n"
                       "   bar=foo;"},
                      "\n"
                      "endtask\n"}},
      {.expected_construct = NodeEnum::kAssumeStatement,
       .token_data = {"task t;\n",
                      "assume ( expr ) foo=bar;\n",
                      {static_cast<int>(NodeEnum::kElseClause),
                       "else \n"
                       "   ;"},  // null else body
                      "\n"
                      "endtask\n"}},
      {.expected_construct = NodeEnum::kAssumeStatement,
       .token_data = {"task t;\n",
                      "assume ( expr ) foo=bar;\n",
                      {static_cast<int>(NodeEnum::kElseClause),
                       "else \n"
                       "   begin\nbar=foo;\nend"},
                      "\n"
                      "endtask\n"}},

      {.expected_construct = NodeEnum::kAssertPropertyStatement,
       .token_data = {"task t;\n",
                      "assert property ( expr )\n",  // no statement
                      {static_cast<int>(NodeEnum::kElseClause),
                       "else \n"
                       "   bar=foo;"},
                      "\n"
                      "endtask\n"}},
      {.expected_construct = NodeEnum::kAssertPropertyStatement,
       .token_data = {"task t;\n",
                      "assert property ( expr );\n",  // null statement
                      {static_cast<int>(NodeEnum::kElseClause),
                       "else \n"
                       "   bar=foo;"},
                      "\n"
                      "endtask\n"}},
      {.expected_construct = NodeEnum::kAssertPropertyStatement,
       .token_data = {"task t;\n",
                      "assert property ( expr ) foo=bar;\n",
                      {static_cast<int>(NodeEnum::kElseClause),
                       "else \n"
                       "   ;"},  // null else body
                      "\n"
                      "endtask\n"}},
      {.expected_construct = NodeEnum::kAssertPropertyStatement,
       .token_data = {"task t;\n",
                      "assert property ( expr ) foo=bar;\n",
                      {static_cast<int>(NodeEnum::kElseClause),
                       "else \n"
                       "   bar=foo;"},
                      "\n"
                      "endtask\n"}},
      {.expected_construct = NodeEnum::kAssertPropertyStatement,
       .token_data = {"task t;\n",
                      "assert property ( expr ) foo=bar;\n",
                      {static_cast<int>(NodeEnum::kElseClause),
                       "else \n"
                       "   begin\nbar=foo;\nend"},
                      "\n"
                      "endtask\n"}},

      {.expected_construct = NodeEnum::kAssumePropertyStatement,
       .token_data = {"task t;\n",
                      "assume property ( expr )\n",  // no statement
                      {static_cast<int>(NodeEnum::kElseClause),
                       "else \n"
                       "   bar=foo;"},
                      "\n"
                      "endtask\n"}},
      {.expected_construct = NodeEnum::kAssumePropertyStatement,
       .token_data = {"task t;\n",
                      "assume property ( expr );\n",  // null statement
                      {static_cast<int>(NodeEnum::kElseClause),
                       "else \n"
                       "   bar=foo;"},
                      "\n"
                      "endtask\n"}},
      {.expected_construct = NodeEnum::kAssumePropertyStatement,
       .token_data = {"task t;\n",
                      "assume property ( expr ) foo=bar;\n",
                      {static_cast<int>(NodeEnum::kElseClause),
                       "else \n"
                       "   ;"},  // null else body
                      "\n"
                      "endtask\n"}},
      {.expected_construct = NodeEnum::kAssumePropertyStatement,
       .token_data = {"task t;\n",
                      "assume property ( expr ) foo=bar;\n",
                      {static_cast<int>(NodeEnum::kElseClause),
                       "else \n"
                       "   bar=foo;"},
                      "\n"
                      "endtask\n"}},
      {.expected_construct = NodeEnum::kAssumePropertyStatement,
       .token_data = {"task t;\n",
                      "assume property ( expr ) foo=bar;\n",
                      {static_cast<int>(NodeEnum::kElseClause),
                       "else \n"
                       "   begin\nbar=foo;\nend"},
                      "\n"
                      "endtask\n"}},

      {.expected_construct = NodeEnum::kExpectPropertyStatement,
       .token_data = {"task t;\n",
                      "expect ( expr )\n",  // no statement
                      {static_cast<int>(NodeEnum::kElseClause),
                       "else \n"
                       "   bar=foo;"},
                      "\n"
                      "endtask\n"}},
      {.expected_construct = NodeEnum::kExpectPropertyStatement,
       .token_data = {"task t;\n",
                      "expect ( expr );\n",  // null statement
                      {static_cast<int>(NodeEnum::kElseClause),
                       "else \n"
                       "   bar=foo;"},
                      "\n"
                      "endtask\n"}},
      {.expected_construct = NodeEnum::kExpectPropertyStatement,
       .token_data = {"task t;\n",
                      "expect ( expr ) foo=bar;\n",
                      {static_cast<int>(NodeEnum::kElseClause),
                       "else \n"
                       "   ;"},  // null else body
                      "\n"
                      "endtask\n"}},
      {.expected_construct = NodeEnum::kExpectPropertyStatement,
       .token_data = {"task t;\n",
                      "expect ( expr ) foo=bar;\n",
                      {static_cast<int>(NodeEnum::kElseClause),
                       "else \n"
                       "   bar=foo;"},
                      "\n"
                      "endtask\n"}},
      {.expected_construct = NodeEnum::kExpectPropertyStatement,
       .token_data = {"task t;\n",
                      "expect ( expr ) foo=bar;\n",
                      {static_cast<int>(NodeEnum::kElseClause),
                       "else \n"
                       "   begin\nbar=foo;\nend"},
                      "\n"
                      "endtask\n"}},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test.token_data,
        [&test](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();

          // Grab outer statement constructs.
          const auto statements = verible::SearchSyntaxTree(
              *ABSL_DIE_IF_NULL(root),
              verible::matcher::DynamicTagMatchBuilder(SymbolTag{
                  .kind = SymbolKind::kNode,
                  .tag = static_cast<int>(test.expected_construct)})());

          // Extract subtree of interest.
          std::vector<TreeSearchMatch> bodies;
          for (const auto &statement : statements) {
            const auto *clause = GetAnyConditionalElseClause(*statement.match);
            bodies.push_back(TreeSearchMatch{
                .match = clause, .context = {/* ignored context */}});
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
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          const auto &instances =
              FindAllForLoopsInitializations(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> names;
          for (const auto &instance : instances) {
            const auto *variable_name =
                GetVariableNameFromForInitialization(*instance.match);
            names.emplace_back(TreeSearchMatch{
                .match = variable_name, .context = {/* ignored context */}});
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
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          const auto &instances =
              FindAllForLoopsInitializations(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> types;
          for (const auto &instance : instances) {
            const auto *type =
                GetDataTypeFromForInitialization(*instance.match);
            if (type == nullptr) {
              continue;
            }
            types.emplace_back(TreeSearchMatch{
                .match = type, .context = {/* ignored context */}});
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
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          const auto &instances =
              FindAllForLoopsInitializations(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> expressions;
          for (const auto &instance : instances) {
            const auto *expression =
                GetExpressionFromForInitialization(*instance.match);
            expressions.emplace_back(TreeSearchMatch{
                .match = expression, .context = {/* ignored context */}});
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
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          const auto &blocks = FindAllGenerateBlocks(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> begins;
          for (const auto &block : blocks) {
            const auto *begin = GetGenerateBlockBegin(*block.match);
            begins.emplace_back(TreeSearchMatch{
                .match = begin, .context = {/* ignored context */}});
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
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          const auto &blocks = FindAllGenerateBlocks(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> ends;
          for (const auto &block : blocks) {
            const auto *end = GetGenerateBlockEnd(*block.match);
            ends.emplace_back(TreeSearchMatch{
                .match = end, .context = {/* ignored context */}});
          }
          return ends;
        });
  }
}

TEST(FindAllNonBlockingAssignmentTest, Various) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {"module m;\n"
       "always @(*) x = 1;\n"
       "endmodule"},
      {"module m;\n"
       "always_ff @(posedge clk) ",
       {kTag, "x <= 1;"},
       "\nendmodule"},
      {"module m;\n"
       "always_ff @(posedge clk) begin\n",
       {kTag, "a <= b;"},
       "\n",
       {kTag, "c <= d();"},
       "\n",
       {kTag, "e <= `F;"},
       "\nend endmodule"},
      {"module m;\n"
       "always_latch ",
       {kTag, "x <= 1;"},
       "\nendmodule"},
      {"module m;\n"
       "always@(*) ",
       {kTag, "x[1] <= y[2];"},
       "\nendmodule"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          return FindAllNonBlockingAssignments(*ABSL_DIE_IF_NULL(root));
        });
  }
}

TEST(GetNonBlockingAssignmentRhsTest, Various) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {"module m;\n"
       "always @(*) x = 1;\n"
       "endmodule"},
      {"module m;\n"
       "always_ff @(posedge clk) "
       "x <= ",
       {kTag, "1"},
       ";\nendmodule"},
      {"module m;\n"
       "always_ff @(posedge clk) begin\n"
       "a <= ",
       {kTag, "b"},
       ";\nc <= ",
       {kTag, "d()"},
       ";\ne <= ",
       {kTag, "`F"},
       ";\nend endmodule"},
      {"module m;\n"
       "always_latch "
       "x <= ",
       {kTag, "1"},
       ";\nendmodule"},
      {"module m;\n"
       "always@(*) x[1] <= ",
       {kTag, "y[2]"},
       ";\nendmodule"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          const auto &non_blocking_assignments =
              FindAllNonBlockingAssignments(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> right_hand_sides;
          right_hand_sides.reserve(non_blocking_assignments.size());
          for (const auto &assignment : non_blocking_assignments) {
            const auto *rhs = GetNonBlockingAssignmentRhs(
                verible::SymbolCastToNode(*assignment.match));
            right_hand_sides.emplace_back(TreeSearchMatch{
                .match = rhs, .context = {/* ignored context */}});
          }
          return right_hand_sides;
        });
  }
}

TEST(GetNonBlockingAssignmentLhsTest, Various) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {"module m;\n"
       "always @(*) x = 1;\n"
       "endmodule"},
      {"module m;\n"
       "always_ff @(posedge clk) ",
       {kTag, "x"},
       " <= 1;\n",
       "endmodule"},
      {"module m;\n"
       "always_ff @(posedge clk) begin\n",
       {kTag, "a"},
       " <= b;\n",
       {kTag, "c"},
       " <= d();\n",
       {kTag, "e"},
       " <= `F;\n"
       "end endmodule"},
      {"module m;\n"
       "always_latch ",
       {kTag, "x"},
       " <= 1;\n"
       "endmodule"},
      {"module m;\n"
       "always@(*) ",
       {kTag, "x[1]"},
       " <= y[2];\n"
       "endmodule"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          const auto &non_blocking_assignments =
              FindAllNonBlockingAssignments(*ABSL_DIE_IF_NULL(root));

          std::vector<TreeSearchMatch> left_hand_sides;
          left_hand_sides.reserve(non_blocking_assignments.size());
          for (const auto &assignment : non_blocking_assignments) {
            const auto *lhs = GetNonBlockingAssignmentLhs(
                verible::SymbolCastToNode(*assignment.match));
            left_hand_sides.emplace_back(TreeSearchMatch{
                .match = lhs, .context = {/* ignored context */}});
          }
          return left_hand_sides;
        });
  }
}

TEST(GetIfClauseHeaderTest, Various) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {"module m;\n"
       "always @(*)\n",
       {kTag, "if (y)"},
       " x = 1;\n"
       "endmodule"},
      {"module m;\n"
       "always @(*) x = 1;\n"
       "endmodule"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          const auto &if_clauses =
              verible::SearchSyntaxTree(*root, NodekIfClause());

          std::vector<TreeSearchMatch> if_headers;
          if_headers.reserve(if_clauses.size());
          for (const auto &if_clause : if_clauses) {
            const auto *header =
                GetIfClauseHeader(verible::SymbolCastToNode(*if_clause.match));
            if_headers.emplace_back(TreeSearchMatch{
                .match = header, .context = {/* ignored context */}});
          }
          return if_headers;
        });
  }
}

TEST(GetIfClauseExpressionTest, Various) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {"module m;\n"
       "always @(*) if(",
       {kTag, "y"},
       ")\n "
       "x = 1;\n"
       "endmodule"},
      {"module m;\n"
       "always @(*) if(",
       {kTag, "func()"},
       ") x = 1;\n"
       "endmodule"},
      {"module m;\n"
       "always @(*) if(",
       {kTag, "y"},
       ") begin\n",
       "if(",
       {kTag, "z"},
       ") x = 1;\n"
       "end endmodule"},
      {"module m;\n"
       "always @(*) x = 1;\n"
       "endmodule"},
  };
  for (const auto &test : kTestCases) {
    TestVerilogSyntaxRangeMatches(
        __FUNCTION__, test, [](const TextStructureView &text_structure) {
          const auto &root = text_structure.SyntaxTree();
          const auto &if_headers =
              verible::SearchSyntaxTree(*root, NodekIfHeader());

          std::vector<TreeSearchMatch> if_expressions;
          if_expressions.reserve(if_headers.size());
          for (const auto &if_header : if_headers) {
            const auto *expression = GetIfHeaderExpression(
                verible::SymbolCastToNode(*if_header.match));
            if_expressions.emplace_back(TreeSearchMatch{
                .match = expression, .context = {/* ignored context */}});
          }
          return if_expressions;
        });
  }
}

}  // namespace
}  // namespace verilog
