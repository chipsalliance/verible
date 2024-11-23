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

// Unit-tests for (Verilog) LexicalContext.
//
// Testing strategy:
// LexicalContext is just a means of disambiguation for overloaded tokens.
// What is most important is that the transformed tokens are correct.
// The vast majority of tokens pass through un-modified, so focus testing
// on those transfomations, and the state functions that directly support them.
// Testing exhaustively is counter-productive because many aspects of the
// class's internal details are subject to change.

#include "verible/verilog/parser/verilog-lexical-context.h"

#include <array>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <stack>
#include <string>
#include <vector>

#include "absl/strings/match.h"
#include "absl/strings/string_view.h"
#include "gtest/gtest.h"
#include "verible/common/text/text-structure.h"
#include "verible/common/text/token-info.h"
#include "verible/common/text/token-stream-view.h"
#include "verible/common/util/logging.h"
#include "verible/verilog/analysis/verilog-analyzer.h"  // only used for lexing
#include "verible/verilog/parser/verilog-parser.h"  // only used for diagnostics
#include "verible/verilog/parser/verilog-token-enum.h"

#undef EXPECT_OK
#define EXPECT_OK(value) EXPECT_TRUE((value).ok())

namespace verilog {
namespace {

using verible::TokenInfo;
using verible::TokenSequence;
using verible::TokenStreamReferenceView;

// TODO(fangism): move this to a test-only library
// Compare value and reason.
// We define it as a macro so that when it fails, you get a meaningful line
// number.
//
// If this were a function, its signature would be:
// template <typename T>
// inline void EXPECT_EQ_REASON(const verible::WithReason<T>& r, const T&
// expected, absl::string_view pattern);
#define EXPECT_EQ_REASON(expr, expected, pattern)                        \
  {                                                                      \
    const auto &r = expr; /* evaluate expr once, auto-extend lifetime */ \
    EXPECT_EQ(r.value, expected) << r.reason;                            \
    /* value could be correct, but reason could be wrong. */             \
    EXPECT_TRUE(absl::StrContains(absl::string_view(r.reason), pattern)) \
        << "value: " << r.value << "\nreason: " << r.reason;             \
  }

template <class SM>
void ExpectStateMachineTokenSequence(
    SM &sm, verible::TokenStreamView::const_iterator *token_iter,
    std::initializer_list<int> expect_token_enums) {
  int i = 0;
  for (int expect_token_enum : expect_token_enums) {
    const TokenInfo &token(***token_iter);
    const int token_enum = token.token_enum();
    const int interpreted_enum = sm.InterpretToken(token_enum);
    const char *raw_symbol = verilog_symbol_name(token_enum);
    const char *interpreted_symbol = verilog_symbol_name(interpreted_enum);
    const char *expected_symbol = verilog_symbol_name(expect_token_enum);
    VLOG(1) << "token[" << i << "] enum: " << raw_symbol << " -> "
            << interpreted_symbol;
    EXPECT_EQ(interpreted_enum, expect_token_enum)
        << " (" << interpreted_symbol << " vs. " << expected_symbol << ')';
    sm.UpdateState(token_enum);
    ++*token_iter;
    ++i;
  }
}

// Tests for null state of state machine.
TEST(KeywordLabelStateMachineTest, NoKeywords) {
  VerilogAnalyzer analyzer("1, 2; 3;", "");
  EXPECT_OK(analyzer.Tokenize());
  analyzer.FilterTokensForSyntaxTree();
  const auto &tokens_view = analyzer.Data().GetTokenStreamView();
  EXPECT_EQ(tokens_view.size(), 7);  // including EOF

  internal::KeywordLabelStateMachine b;
  EXPECT_TRUE(b.ItemMayStart());
  int i = 0;
  for (auto iter : tokens_view) {
    b.UpdateState(iter->token_enum());
    EXPECT_FALSE(b.ItemMayStart())
        << "Error at index " << i << ", after: " << *iter;
    ++i;
  }
}

// Test for state transitions of state machine, no labels.
TEST(KeywordLabelStateMachineTest, KeywordsWithoutLabels) {
  VerilogAnalyzer analyzer("1 2 begin end begin end 3 begin 4 5 end 6", "");
  const std::array<bool, 13> expect_item_may_start{
      {false, false, true, true, true, true, true, true, true, false, true,
       true, false}};
  EXPECT_OK(analyzer.Tokenize());
  analyzer.FilterTokensForSyntaxTree();
  const auto &tokens_view = analyzer.Data().GetTokenStreamView();
  EXPECT_EQ(tokens_view.size(), expect_item_may_start.size());

  internal::KeywordLabelStateMachine b;
  EXPECT_TRUE(b.ItemMayStart());
  auto expect_iter = expect_item_may_start.begin();
  for (auto iter : tokens_view) {
    b.UpdateState(iter->token_enum());
    EXPECT_EQ(b.ItemMayStart(), *expect_iter)
        << "Error at index "
        << std::distance(expect_item_may_start.begin(), expect_iter)
        << ", after: " << *iter;
    ++expect_iter;
  }
}

// Test for state transitions of state machine, with labels.
TEST(KeywordLabelStateMachineTest, KeywordsWithLabels) {
  VerilogAnalyzer analyzer("1 begin:a end:a begin:b end:b 2", "");
  const std::array<bool, 15> expect_item_may_start{
      {false, true, false, true, true, false, true, true, false, true, true,
       false, true, false, false}};
  EXPECT_OK(analyzer.Tokenize());
  analyzer.FilterTokensForSyntaxTree();
  const auto &tokens_view = analyzer.Data().GetTokenStreamView();
  EXPECT_EQ(tokens_view.size(), expect_item_may_start.size());  // including EOF

  internal::KeywordLabelStateMachine b;
  EXPECT_TRUE(b.ItemMayStart());
  auto expect_iter = expect_item_may_start.cbegin();
  for (auto iter : tokens_view) {
    b.UpdateState(iter->token_enum());
    EXPECT_EQ(b.ItemMayStart(), *expect_iter)
        << "Error at index "
        << std::distance(expect_item_may_start.begin(), expect_iter)
        << ", after: " << *iter;
    ++expect_iter;
  }
}

// Test for state transitions of state machine, with some labels, some items.
TEST(KeywordLabelStateMachineTest, ItemsInsideBlocks) {
  VerilogAnalyzer analyzer("begin:a 1 end:a 2 begin 3 end", "");
  const std::array<bool, 12> expect_item_may_start{{true, false, true, false,
                                                    true, false, true, false,
                                                    true, true, true, true}};
  EXPECT_OK(analyzer.Tokenize());
  analyzer.FilterTokensForSyntaxTree();
  const auto &tokens_view = analyzer.Data().GetTokenStreamView();
  EXPECT_EQ(tokens_view.size(), expect_item_may_start.size());  // including EOF

  internal::KeywordLabelStateMachine b;
  EXPECT_TRUE(b.ItemMayStart());
  auto expect_iter = expect_item_may_start.cbegin();
  for (auto iter : tokens_view) {
    b.UpdateState(iter->token_enum());
    EXPECT_EQ(b.ItemMayStart(), *expect_iter)
        << "Error at index "
        << std::distance(expect_item_may_start.begin(), expect_iter)
        << ", after: " << *iter;
    ++expect_iter;
  }
}

class LastSemicolonStateMachineTest
    : public ::testing::Test,
      public internal::LastSemicolonStateMachine {
 public:
  LastSemicolonStateMachineTest()
      : internal::LastSemicolonStateMachine(
            TK_property, TK_endproperty,
            SemicolonEndOfAssertionVariableDeclarations) {}
};

// Tests that the one and only semicolon in the range of interest is updated.
TEST_F(LastSemicolonStateMachineTest, LifeCycleOneSemicolon) {
  EXPECT_EQ(state_, kNone);
  EXPECT_TRUE(semicolons_.empty());

  // Purely synthesized token sequence for testing:
  // Only enums matter, not text.
  constexpr absl::string_view text("don't care");
  TokenInfo tokens[] = {
      TokenInfo(TK_module, text),
      TokenInfo(SymbolIdentifier, text),
      TokenInfo(';', text),
      TokenInfo(TK_property, text),
      TokenInfo(SymbolIdentifier, text),
      TokenInfo(';', text),  // only this one should be modified
      TokenInfo(SymbolIdentifier, text),
      TokenInfo(TK_endproperty, text),
      TokenInfo(';', text),
  };

  UpdateState(&tokens[0]);
  EXPECT_EQ(state_, kNone);

  UpdateState(&tokens[1]);
  UpdateState(&tokens[2]);
  EXPECT_EQ(state_, kNone);

  UpdateState(&tokens[3]);  // TK_property
  EXPECT_EQ(state_, kActive);

  UpdateState(&tokens[4]);  // SymbolIdentifier
  EXPECT_EQ(state_, kActive);

  UpdateState(&tokens[5]);  // ';'
  EXPECT_EQ(state_, kActive);
  EXPECT_EQ(semicolons_.top(), &tokens[5]);

  UpdateState(&tokens[6]);  // SymbolIdentifier
  EXPECT_EQ(state_, kActive);

  UpdateState(&tokens[7]);  // TK_endproperty
  EXPECT_EQ(state_, kNone);
  EXPECT_TRUE(semicolons_.empty());
  EXPECT_EQ(tokens[2].token_enum(), ';');  // unmodified
  EXPECT_EQ(tokens[5].token_enum(),
            SemicolonEndOfAssertionVariableDeclarations);

  UpdateState(&tokens[8]);  // ';'
  EXPECT_EQ(state_, kNone);
  EXPECT_TRUE(semicolons_.empty());
  EXPECT_EQ(tokens[8].token_enum(), ';');  // unmodified
}

// Tests that only the last semicolon in the range of interest is updated.
TEST_F(LastSemicolonStateMachineTest, LifeCycleFinalSemicolon) {
  EXPECT_EQ(state_, kNone);
  EXPECT_TRUE(semicolons_.empty());

  // Purely synthesized token sequence for testing:
  // Only enums matter, not text.
  constexpr absl::string_view text("don't care");
  TokenInfo tokens[] = {
      TokenInfo(TK_module, text),
      TokenInfo(SymbolIdentifier, text),
      TokenInfo(';', text),
      TokenInfo(TK_property, text),
      TokenInfo(SymbolIdentifier, text),
      TokenInfo(';', text),
      TokenInfo(SymbolIdentifier, text),
      TokenInfo(';', text),  // only this one should be modified
      TokenInfo(SymbolIdentifier, text),
      TokenInfo(TK_endproperty, text),
      TokenInfo(';', text),
  };

  UpdateState(&tokens[0]);
  EXPECT_EQ(state_, kNone);

  UpdateState(&tokens[1]);
  UpdateState(&tokens[2]);
  EXPECT_EQ(state_, kNone);

  UpdateState(&tokens[3]);  // TK_property
  EXPECT_EQ(state_, kActive);

  UpdateState(&tokens[4]);  // SymbolIdentifier
  EXPECT_EQ(state_, kActive);

  UpdateState(&tokens[5]);  // ';'
  EXPECT_EQ(state_, kActive);
  EXPECT_EQ(semicolons_.top(), &tokens[5]);

  UpdateState(&tokens[6]);  // SymbolIdentifier
  EXPECT_EQ(state_, kActive);

  UpdateState(&tokens[7]);  // ';'
  EXPECT_EQ(state_, kActive);
  EXPECT_EQ(semicolons_.top(), &tokens[7]);

  UpdateState(&tokens[8]);  // SymbolIdentifier
  EXPECT_EQ(state_, kActive);

  UpdateState(&tokens[9]);  // TK_endproperty
  EXPECT_EQ(state_, kNone);
  EXPECT_TRUE(semicolons_.empty());
  EXPECT_EQ(tokens[2].token_enum(), ';');  // unmodified
  EXPECT_EQ(tokens[5].token_enum(), ';');  // unmodified
  EXPECT_EQ(tokens[7].token_enum(),
            SemicolonEndOfAssertionVariableDeclarations);

  UpdateState(&tokens[10]);  // ';'
  EXPECT_EQ(state_, kNone);
  EXPECT_TRUE(semicolons_.empty());
  EXPECT_EQ(tokens[10].token_enum(), ';');  // unmodified
}

// TODO(fangism): move this into a test_util library
struct StateMachineTestBase : public ::testing::Test {
  // Lexes code and initializes token_iter to point to the first token.
  void Tokenize(const std::string &code) {
    analyzer = std::make_unique<VerilogAnalyzer>(code, "");
    EXPECT_OK(analyzer->Tokenize());
    analyzer->FilterTokensForSyntaxTree();
    token_iter = analyzer->Data().GetTokenStreamView().cbegin();
  }

  template <class SM>
  void ExpectTokenSequence(SM &sm, std::initializer_list<int> expect_enums) {
    ExpectStateMachineTokenSequence(sm, &token_iter, expect_enums);
  }

  // Parser, used only for lexing.
  std::unique_ptr<VerilogAnalyzer> analyzer;

  // Iterator over the filtered token stream.
  verible::TokenStreamView::const_iterator token_iter;
};

struct ConstraintBlockStateMachineTest : public StateMachineTestBase {
  void ExpectTokenSequence(std::initializer_list<int> expect_enums) {
    StateMachineTestBase::ExpectTokenSequence(sm, expect_enums);
  }

  // Instance of the state machine under test.
  internal::ConstraintBlockStateMachine sm;
};

// Test initial conditions of internal::ConstraintBlockStateMachine.
TEST_F(ConstraintBlockStateMachineTest, Initialization) {
  EXPECT_FALSE(sm.IsActive());
}

// Tests that empty constraint block is balanced.
TEST_F(ConstraintBlockStateMachineTest, EmptyBlock) {
  Tokenize("{}");

  ExpectTokenSequence({'{'});
  EXPECT_TRUE(sm.IsActive());

  ExpectTokenSequence({'}'});
  EXPECT_FALSE(sm.IsActive());
}

// Tests that state machine parses a soft expression.
TEST_F(ConstraintBlockStateMachineTest, SoftExpression) {
  Tokenize(R"(
  {
    soft a -> b;
  }
  )");

  ExpectTokenSequence({'{',  //
                       TK_soft, SymbolIdentifier /* a */, TK_LOGICAL_IMPLIES,
                       SymbolIdentifier, ';'});
  EXPECT_TRUE(sm.IsActive());

  ExpectTokenSequence({'}'});
  EXPECT_FALSE(sm.IsActive());
}

// Tests that state machine parses a soft expression, with extra parens.
TEST_F(ConstraintBlockStateMachineTest, SoftExpressionExtraParens) {
  Tokenize(R"(
  {
    soft (a -> b);
  }
  )");

  ExpectTokenSequence({'{',  //
                       TK_soft, '(', SymbolIdentifier /* a */,
                       TK_LOGICAL_IMPLIES, SymbolIdentifier, ')', ';'});
  EXPECT_TRUE(sm.IsActive());

  ExpectTokenSequence({'}'});
  EXPECT_FALSE(sm.IsActive());
}

// Tests that state machine parses an unexpected '}'.
TEST_F(ConstraintBlockStateMachineTest, InvalidSoftUnexpectedCloseBrace) {
  Tokenize(R"(
  {
    soft  // missing expression and ';'
  }
  )");

  ExpectTokenSequence({'{', TK_soft});
  EXPECT_TRUE(sm.IsActive());

  ExpectTokenSequence({'}'});
  EXPECT_FALSE(sm.IsActive());
}

// Tests that state machine parses a uniqueness constraint.
TEST_F(ConstraintBlockStateMachineTest, UniquenessConstraint) {
  Tokenize(R"(
  {
    unique {[0:1],[3:4]};
  }
  )");

  ExpectTokenSequence(                                          //
      {'{',                                                     //
       TK_unique, '{',                                          //
       '[', TK_DecNumber /* 0 */, ':', TK_DecNumber, ']', ',',  //
       '[', TK_DecNumber /* 3 */, ':', TK_DecNumber, ']',       //
       '}', ';'});
  EXPECT_TRUE(sm.IsActive());

  ExpectTokenSequence({'}'});
  EXPECT_FALSE(sm.IsActive());
}

// Tests that state machine parses a solve-before item.
TEST_F(ConstraintBlockStateMachineTest, SolveItem) {
  Tokenize(R"(
  {
    solve a before b;
  }
  )");

  ExpectTokenSequence(
      {'{', TK_solve, SymbolIdentifier, TK_before, SymbolIdentifier, ';'});
  EXPECT_TRUE(sm.IsActive());

  ExpectTokenSequence({'}'});
  EXPECT_FALSE(sm.IsActive());
}

// Tests that state machine parses a solve-before, with multiple variables.
TEST_F(ConstraintBlockStateMachineTest, SolveItemMultiple) {
  Tokenize(R"(
  {
    solve a, b before c, d;
  }
  )");

  ExpectTokenSequence({'{',                                                //
                       TK_solve, SymbolIdentifier, ',', SymbolIdentifier,  //
                       TK_before, SymbolIdentifier, ',', SymbolIdentifier,
                       ';'});
  EXPECT_TRUE(sm.IsActive());

  ExpectTokenSequence({'}'});
  EXPECT_FALSE(sm.IsActive());
}

// Tests that state machine parses a solve-before, with hierarchical variables.
TEST_F(ConstraintBlockStateMachineTest, SolveItemHierarchical) {
  Tokenize(R"(
  {
    solve a.b before c.d;
  }
  )");

  ExpectTokenSequence({'{',                                                //
                       TK_solve, SymbolIdentifier, '.', SymbolIdentifier,  //
                       TK_before, SymbolIdentifier, '.', SymbolIdentifier,
                       ';'});
  EXPECT_TRUE(sm.IsActive());

  ExpectTokenSequence({'}'});
  EXPECT_FALSE(sm.IsActive());
}

// Tests that state machine parses a disable-soft.
TEST_F(ConstraintBlockStateMachineTest, DisableSoft) {
  Tokenize(R"(
  {
    disable soft x.y;
  }
  )");

  ExpectTokenSequence(
      {'{', TK_disable, TK_soft, SymbolIdentifier, '.', SymbolIdentifier, ';'});
  EXPECT_TRUE(sm.IsActive());

  ExpectTokenSequence({'}'});
  EXPECT_FALSE(sm.IsActive());
}

// Tests that state machine correctly balances, even with a missing ';'.
TEST_F(ConstraintBlockStateMachineTest, BalanceConstraintSetMissingSemicolon) {
  Tokenize(R"(
  {
    f -> { g -> h }  // missing ';' after 'h'
  }
  )");

  ExpectTokenSequence(  //
      {'{', SymbolIdentifier /* f */, TK_CONSTRAINT_IMPLIES, '{',
       SymbolIdentifier, TK_CONSTRAINT_IMPLIES, SymbolIdentifier, '}'});
  EXPECT_TRUE(sm.IsActive());

  ExpectTokenSequence({'}'});
  EXPECT_FALSE(sm.IsActive());
}

// Tests that state machine correctly balances a nested constraint set.
TEST_F(ConstraintBlockStateMachineTest, InterpretRightArrowConstraintSetRHS) {
  Tokenize(R"(
  {
    f -> { g -> h; }
  }
  )");

  ExpectTokenSequence(                                   //
      {'{',                                              //
       SymbolIdentifier /* f */, TK_CONSTRAINT_IMPLIES,  //
       '{', SymbolIdentifier, TK_CONSTRAINT_IMPLIES, SymbolIdentifier, ';',
       '}'});
  EXPECT_TRUE(sm.IsActive());

  ExpectTokenSequence({'}'});
  EXPECT_FALSE(sm.IsActive());
}

// Tests that state machine correctly balances a nested constraint set.
TEST_F(ConstraintBlockStateMachineTest,
       InterpretRightArrowConstraintSetNested) {
  Tokenize(R"(
  {
    f -> {
      g -> {h;}
    }
  }
  )");

  ExpectTokenSequence(                                   //
      {'{',                                              //
       SymbolIdentifier /* f */, TK_CONSTRAINT_IMPLIES,  //
       '{', SymbolIdentifier, TK_CONSTRAINT_IMPLIES, '{', SymbolIdentifier, ';',
       '}', '}'});
  EXPECT_TRUE(sm.IsActive());

  ExpectTokenSequence({'}'});
  EXPECT_FALSE(sm.IsActive());
}

// Tests that state machine correctly balances parentheses.
TEST_F(ConstraintBlockStateMachineTest, InterpretRightArrowDeepParens) {
  Tokenize(R"(
  {
    (((f -> g))) -> ((j -> k)) -> ((p -> q));
  }
  )");

  // clang-format off
  ExpectTokenSequence(
      {'{',
       '(', '(', '(',
       SymbolIdentifier /* f */, TK_LOGICAL_IMPLIES, SymbolIdentifier,
       ')', ')', ')',
       TK_CONSTRAINT_IMPLIES,
       '(', '(',
       SymbolIdentifier /* j */, TK_LOGICAL_IMPLIES, SymbolIdentifier,
       ')', ')',
       TK_CONSTRAINT_IMPLIES,
       '(', '(',
       SymbolIdentifier /* p */, TK_LOGICAL_IMPLIES, SymbolIdentifier,
       ')', ')', ';'
      });
  // clang-format on

  EXPECT_TRUE(sm.IsActive());

  ExpectTokenSequence({'}'});
  EXPECT_FALSE(sm.IsActive());
}

// Tests that state machine correctly balances braces.
TEST_F(ConstraintBlockStateMachineTest, InterpretRightArrowDeepBraces) {
  Tokenize(R"(
  {
    {{a -> b, (g -> h)}} -> {{(j -> k), l -> m}};  // concatenation expressions
  }
  )");

  // clang-format off
  ExpectTokenSequence(
      {'{',
       '{', '{',
       SymbolIdentifier /* a */, TK_LOGICAL_IMPLIES, SymbolIdentifier,
       ',',
       '(', SymbolIdentifier /* g */, TK_LOGICAL_IMPLIES, SymbolIdentifier, ')',
       '}', '}',
       TK_CONSTRAINT_IMPLIES,
       '{', '{',
       '(', SymbolIdentifier /* j */, TK_LOGICAL_IMPLIES, SymbolIdentifier, ')',
       ',',
       SymbolIdentifier /* l */, TK_LOGICAL_IMPLIES, SymbolIdentifier,
       '}', '}', ';'});
  // clang-format on
  EXPECT_TRUE(sm.IsActive());

  ExpectTokenSequence({'}'});
  EXPECT_FALSE(sm.IsActive());
}

// Tests that state machine correctly interprets '->'.
TEST_F(ConstraintBlockStateMachineTest, InterpretRightArrow) {
  Tokenize(R"(
  {
    a -> b;
    (c -> d) -> e;
    f -> { (g -> h) -> i }  // missing ';' after 'i'
  }
  )");

  ExpectTokenSequence({'{', SymbolIdentifier /* a */, TK_CONSTRAINT_IMPLIES,
                       SymbolIdentifier, ';'});
  EXPECT_TRUE(sm.IsActive());
  ExpectTokenSequence({'(', SymbolIdentifier /* c */, TK_LOGICAL_IMPLIES,
                       SymbolIdentifier, ')', TK_CONSTRAINT_IMPLIES,
                       SymbolIdentifier, ';'});
  EXPECT_TRUE(sm.IsActive());
  ExpectTokenSequence({SymbolIdentifier /* f */, TK_CONSTRAINT_IMPLIES, '{',
                       '(', SymbolIdentifier, TK_LOGICAL_IMPLIES,
                       SymbolIdentifier, ')', TK_CONSTRAINT_IMPLIES,
                       SymbolIdentifier, '}', '}'});
  EXPECT_FALSE(sm.IsActive());
}

// Tests that state machine correctly interprets '->'.
TEST_F(ConstraintBlockStateMachineTest, InterpretRightArrow2) {
  Tokenize(R"(
  {
    a -> (b -> c);
    d -> {
      e -> (f -> g);
      (h -> i) -> j;
    }
  }
  )");

  ExpectTokenSequence({'{', SymbolIdentifier /* a */, TK_CONSTRAINT_IMPLIES,
                       '(', SymbolIdentifier, TK_LOGICAL_IMPLIES,
                       SymbolIdentifier, ')', ';'});
  ExpectTokenSequence({SymbolIdentifier /* d */, TK_CONSTRAINT_IMPLIES, '{',
                       SymbolIdentifier /* e */, TK_CONSTRAINT_IMPLIES, '(',
                       SymbolIdentifier, TK_LOGICAL_IMPLIES, SymbolIdentifier,
                       ')', ';'});
  ExpectTokenSequence({'(', SymbolIdentifier /* h */, TK_LOGICAL_IMPLIES,
                       SymbolIdentifier, ')', TK_CONSTRAINT_IMPLIES,
                       SymbolIdentifier, ';', '}', '}'});
  EXPECT_FALSE(sm.IsActive());
}

// Tests that state machine correctly interprets '->' with balanced {}
// expressions.
TEST_F(ConstraintBlockStateMachineTest, InterpretRightArrowBracedExpressions) {
  Tokenize(R"(
  {
    {2{4'h0}} -> {2{4'h1}};
    ({2{4'h2}} -> {2{4'h3}}) -> {2{4'h4}};
    {2{4'h5}} -> ({2{4'h6}} -> {2{4'h7}});
  }
  )");

  ExpectTokenSequence({'{'});
  EXPECT_TRUE(sm.IsActive());

  // Re-use the same expected token sequence for {N{M'hx}}
#define CONSTANT_CONCAT_SEQUENCE \
  '{', TK_DecNumber, '{', TK_DecNumber, TK_HexBase, TK_HexDigits, '}', '}'

  ExpectTokenSequence({CONSTANT_CONCAT_SEQUENCE, TK_CONSTRAINT_IMPLIES,
                       CONSTANT_CONCAT_SEQUENCE, ';'});
  EXPECT_TRUE(sm.IsActive());

  ExpectTokenSequence({'(', CONSTANT_CONCAT_SEQUENCE, TK_LOGICAL_IMPLIES,
                       CONSTANT_CONCAT_SEQUENCE, ')', TK_CONSTRAINT_IMPLIES,
                       CONSTANT_CONCAT_SEQUENCE, ';'});
  EXPECT_TRUE(sm.IsActive());

  ExpectTokenSequence({CONSTANT_CONCAT_SEQUENCE, TK_CONSTRAINT_IMPLIES, '(',
                       CONSTANT_CONCAT_SEQUENCE, TK_LOGICAL_IMPLIES,
                       CONSTANT_CONCAT_SEQUENCE, ')', ';'});
  EXPECT_TRUE(sm.IsActive());

#undef CONSTANT_CONCAT_SEQUENCE

  ExpectTokenSequence({'}'});
  EXPECT_FALSE(sm.IsActive());
}

// Tests that state machine recovers from a bad if construct.
TEST_F(ConstraintBlockStateMachineTest, InvalidIf) {
  Tokenize(R"(
  {
    if
  }
  )");

  ExpectTokenSequence({'{', TK_if});
  EXPECT_TRUE(sm.IsActive());
  ExpectTokenSequence({'}'});
  EXPECT_FALSE(sm.IsActive());
}

// Tests that state machine correctly interprets '->' in if blocks.
TEST_F(ConstraintBlockStateMachineTest, IfConstraintSingle) {
  Tokenize(R"(
  {
    if (a -> b) c -> d;
  }
  )");

  ExpectTokenSequence({'{', TK_if, '(', SymbolIdentifier, TK_LOGICAL_IMPLIES,
                       SymbolIdentifier, ')', SymbolIdentifier,
                       TK_CONSTRAINT_IMPLIES, SymbolIdentifier, ';'});
  EXPECT_TRUE(sm.IsActive());
  ExpectTokenSequence({'}'});
  EXPECT_FALSE(sm.IsActive());
}

// Tests that state machine correctly interprets '->' in if clauses.
TEST_F(ConstraintBlockStateMachineTest, IfConstraintSingleParenExpressions) {
  Tokenize(R"(
  {
    if (a -> b)
      (c -> d) -> (e -> f);
  }
  )");

  ExpectTokenSequence(  //
      {'{', TK_if, '(', SymbolIdentifier, TK_LOGICAL_IMPLIES, SymbolIdentifier,
       ')'});
  ExpectTokenSequence(  //
      {'(', SymbolIdentifier /* c */, TK_LOGICAL_IMPLIES, SymbolIdentifier, ')',
       TK_CONSTRAINT_IMPLIES,  //
       '(', SymbolIdentifier /* e */, TK_LOGICAL_IMPLIES, SymbolIdentifier, ')',
       ';'});
  EXPECT_TRUE(sm.IsActive());
  ExpectTokenSequence({'}'});
  EXPECT_FALSE(sm.IsActive());
}

// Tests that state machine correctly interprets '->' in constraint set
// if-clause.
TEST_F(ConstraintBlockStateMachineTest, IfConstraintBlock) {
  Tokenize(R"(
  {
    if (a -> b) { c -> d; }
  }
  )");

  ExpectTokenSequence(                                          //
      {'{', TK_if, '(',                                         //
       SymbolIdentifier, TK_LOGICAL_IMPLIES, SymbolIdentifier,  //
       ')',                                                     //
       '{', SymbolIdentifier, TK_CONSTRAINT_IMPLIES, SymbolIdentifier, ';',
       '}'});
  EXPECT_TRUE(sm.IsActive());
  ExpectTokenSequence({'}'});
  EXPECT_FALSE(sm.IsActive());
}

// Tests that state machine correctly interprets '->' in if-else clauses.
TEST_F(ConstraintBlockStateMachineTest, IfElseConstraintSingle) {
  Tokenize(R"(
  {
    if (a -> b) c -> d;
    else e -> f;
  }
  )");

  ExpectTokenSequence(  //
      {'{',             //
       TK_if, '(',      //
       SymbolIdentifier, TK_LOGICAL_IMPLIES, SymbolIdentifier,
       ')',  //
       SymbolIdentifier, TK_CONSTRAINT_IMPLIES, SymbolIdentifier, ';'});
  EXPECT_TRUE(sm.IsActive());
  ExpectTokenSequence(  //
      {TK_else,         //
       SymbolIdentifier, TK_CONSTRAINT_IMPLIES, SymbolIdentifier, ';'});
  EXPECT_TRUE(sm.IsActive());
  ExpectTokenSequence({'}'});
  EXPECT_FALSE(sm.IsActive());
}

// Tests that state machine correctly interprets '->' in if-else blocks.
TEST_F(ConstraintBlockStateMachineTest, IfElseConstraintBlocks) {
  Tokenize(R"(
  {
    if (a -> b) { c -> d; }
    else { e -> f; }
  }
  )");

  ExpectTokenSequence(                                             //
      {'{',                                                        //
       TK_if, '(',                                                 //
       SymbolIdentifier, TK_LOGICAL_IMPLIES, SymbolIdentifier,     //
       ')', '{',                                                   //
       SymbolIdentifier, TK_CONSTRAINT_IMPLIES, SymbolIdentifier,  //
       ';', '}'});
  EXPECT_TRUE(sm.IsActive());
  ExpectTokenSequence(                                             //
      {TK_else, '{',                                               //
       SymbolIdentifier, TK_CONSTRAINT_IMPLIES, SymbolIdentifier,  //
       ';', '}'});
  EXPECT_TRUE(sm.IsActive());
  ExpectTokenSequence({'}'});
  EXPECT_FALSE(sm.IsActive());
}

// Tests that state machine correctly interprets '->' in if-else nested blocks.
TEST_F(ConstraintBlockStateMachineTest, IfElseConstraintBlocksNested) {
  Tokenize(R"(
  {
    if (a -> b)
      if (p -> q) { c -> d; }
      else { e -> f; }
    else { r -> s; }
  }
  )");

  ExpectTokenSequence(  //
      {'{',             //
       TK_if, '(', SymbolIdentifier /* a */, TK_LOGICAL_IMPLIES,
       SymbolIdentifier, ')'});
  EXPECT_TRUE(sm.IsActive());
  ExpectTokenSequence(                                                     //
      {TK_if, '(',                                                         //
       SymbolIdentifier /* p */, TK_LOGICAL_IMPLIES, SymbolIdentifier,     //
       ')', '{',                                                           //
       SymbolIdentifier /* c */, TK_CONSTRAINT_IMPLIES, SymbolIdentifier,  //
       ';', '}'});
  EXPECT_TRUE(sm.IsActive());
  ExpectTokenSequence({TK_else, '{', SymbolIdentifier /* e */,
                       TK_CONSTRAINT_IMPLIES, SymbolIdentifier, ';', '}'});
  EXPECT_TRUE(sm.IsActive());
  ExpectTokenSequence({TK_else, '{', SymbolIdentifier /* r */,
                       TK_CONSTRAINT_IMPLIES, SymbolIdentifier, ';', '}'});
  EXPECT_TRUE(sm.IsActive());

  ExpectTokenSequence({'}'});
  EXPECT_FALSE(sm.IsActive());
}

// Tests that state machine recovers from a bad foreach construct.
TEST_F(ConstraintBlockStateMachineTest, InvalidForeach) {
  Tokenize(R"(
  {
    foreach
  }
  )");

  ExpectTokenSequence({'{', TK_foreach});
  EXPECT_TRUE(sm.IsActive());
  ExpectTokenSequence({'}'});
  EXPECT_FALSE(sm.IsActive());
}

// Tests that state machine correctly interprets '->' in foreach blocks.
TEST_F(ConstraintBlockStateMachineTest, ForeachSingleSimple) {
  Tokenize(R"(
  {
    foreach (a[i]) c -> d;
  }
  )");

  ExpectTokenSequence(                                                      //
      {'{',                                                                 //
       TK_foreach, '(', SymbolIdentifier, '[', SymbolIdentifier, ']', ')',  //
       SymbolIdentifier, TK_CONSTRAINT_IMPLIES, SymbolIdentifier, ';'});
  EXPECT_TRUE(sm.IsActive());
  ExpectTokenSequence({'}'});
  EXPECT_FALSE(sm.IsActive());
}

// Tests that state machine correctly interprets '->' in foreach blocks.
TEST_F(ConstraintBlockStateMachineTest, ForeachSingleHierarchical) {
  Tokenize(R"(
  {
    foreach (a.b[i,j]) c -> d;
  }
  )");

  ExpectTokenSequence(                                             //
      {'{',                                                        //
       TK_foreach, '(',                                            //
       SymbolIdentifier /* a */, '.', SymbolIdentifier,            //
       '[', SymbolIdentifier /* i */, ',', SymbolIdentifier, ']',  //
       ')',                                                        //
       SymbolIdentifier /* c */, TK_CONSTRAINT_IMPLIES, SymbolIdentifier, ';'});
  EXPECT_TRUE(sm.IsActive());
  ExpectTokenSequence({'}'});
  EXPECT_FALSE(sm.IsActive());
}

// Tests that state machine correctly interprets '->' in foreach blocks.
TEST_F(ConstraintBlockStateMachineTest, ForeachBlock) {
  Tokenize(R"(
  {
    foreach (a[i]) { c -> d; }
  }
  )");

  ExpectTokenSequence(                                                  //
      {'{',                                                             //
       TK_foreach, '(',                                                 //
       SymbolIdentifier, '[', SymbolIdentifier, ']',                    //
       ')', '{',                                                        //
       SymbolIdentifier, TK_CONSTRAINT_IMPLIES, SymbolIdentifier, ';',  //
       '}'});
  EXPECT_TRUE(sm.IsActive());
  ExpectTokenSequence({'}'});
  EXPECT_FALSE(sm.IsActive());
}

// Tests that state machine correctly interprets '->' in foreach blocks.
TEST_F(ConstraintBlockStateMachineTest, ForeachBlockNested) {
  Tokenize(R"(
  {
    foreach (a[i])
      foreach (b[j])
        { c -> d; }
  }
  )");

  // clang-format off
  ExpectTokenSequence(
      {'{',
       TK_foreach, '(',
       SymbolIdentifier /* a */, '[', SymbolIdentifier, ']',
       ')',
       TK_foreach, '(',
       SymbolIdentifier /* b */, '[', SymbolIdentifier, ']',
       ')',
       '{',
       SymbolIdentifier, TK_CONSTRAINT_IMPLIES, SymbolIdentifier, ';',
       '}'});
  // clang-format on
  EXPECT_TRUE(sm.IsActive());
  ExpectTokenSequence({'}'});
  EXPECT_FALSE(sm.IsActive());
}

// Tests that state machine correctly interprets '->' in foreach/if blocks.
TEST_F(ConstraintBlockStateMachineTest, ForeachIfMixed) {
  Tokenize(R"(
  {
    foreach (a[i])
      if (b -> j)
        { c -> d; }
    if (e -> f)
      foreach (g[i])
        { j -> k; }
  }
  )");

  // clang-format off
  ExpectTokenSequence(
      {'{', TK_foreach, '(',
       SymbolIdentifier /* a */, '[', SymbolIdentifier, ']', ')',
       TK_if, '(',
       SymbolIdentifier /* b */, TK_LOGICAL_IMPLIES, SymbolIdentifier, ')',
       '{', SymbolIdentifier, TK_CONSTRAINT_IMPLIES, SymbolIdentifier, ';',
       '}'});
  EXPECT_TRUE(sm.IsActive());
  ExpectTokenSequence(
      {TK_if, '(',
       SymbolIdentifier /* e */, TK_LOGICAL_IMPLIES, SymbolIdentifier, ')',
       TK_foreach, '(',
       SymbolIdentifier /* g */, '[', SymbolIdentifier, ']', ')',
       '{', SymbolIdentifier, TK_CONSTRAINT_IMPLIES, SymbolIdentifier, ';',
       '}'});
  // clang-format on
  EXPECT_TRUE(sm.IsActive());
  ExpectTokenSequence({'}'});
  EXPECT_FALSE(sm.IsActive());
}

struct RandomizeCallStateMachineTest : public StateMachineTestBase {
  void ExpectTokenSequence(std::initializer_list<int> expect_enums) {
    StateMachineTestBase::ExpectTokenSequence(sm, expect_enums);
  }

  // Instance of the state machine under test.
  internal::RandomizeCallStateMachine sm;
};

// Test that internal::RandomizeCallStateMachine initializes in inactive state.
TEST_F(RandomizeCallStateMachineTest, Initialization) {
  EXPECT_FALSE(sm.IsActive());
  sm.UpdateState(TK_randomize);
  EXPECT_TRUE(sm.IsActive());
}

// Test that internal::RandomizeCallStateMachine updates correctly with plain
// call.
TEST_F(RandomizeCallStateMachineTest, ParseStdCall) {
  Tokenize(R"(
  x = std::randomize;
  )");

  ExpectTokenSequence({
      SymbolIdentifier /* x */, '=', TK_randomize /* std::randomize */
  });
  EXPECT_TRUE(sm.IsActive());
  ExpectTokenSequence({';'});
  EXPECT_FALSE(sm.IsActive());
}

// Test that internal::RandomizeCallStateMachine updates correctly with method
// call.
TEST_F(RandomizeCallStateMachineTest, ParseMethodCall) {
  Tokenize(R"(
  x = y.randomize;
  )");
  ExpectTokenSequence(  //
      {SymbolIdentifier /* x */, '=', SymbolIdentifier, '.', TK_randomize});
  EXPECT_TRUE(sm.IsActive());
  ExpectTokenSequence({';'});
  EXPECT_FALSE(sm.IsActive());
}

// Test that internal::RandomizeCallStateMachine updates correctly with empty
// variables.
TEST_F(RandomizeCallStateMachineTest, ParseMethodCallEmptyVariables) {
  Tokenize(R"(
  x = y.randomize();
  )");
  ExpectTokenSequence(                 //
      {SymbolIdentifier /* x */, '=',  //
       SymbolIdentifier, '.', TK_randomize, '(', ')'});
  EXPECT_TRUE(sm.IsActive());
  ExpectTokenSequence({';'});
  EXPECT_FALSE(sm.IsActive());
}

// Test that internal::RandomizeCallStateMachine updates correctly with one
// variable.
TEST_F(RandomizeCallStateMachineTest, ParseMethodCallOneVariable) {
  Tokenize(R"(
  x = y.randomize(z);
  )");
  ExpectTokenSequence({SymbolIdentifier /* x */, '=', SymbolIdentifier, '.',
                       TK_randomize, '(', SymbolIdentifier, ')'});
  EXPECT_TRUE(sm.IsActive());
  ExpectTokenSequence({';'});
  EXPECT_FALSE(sm.IsActive());
}

// Test that internal::RandomizeCallStateMachine updates correctly with multiple
// variables.
TEST_F(RandomizeCallStateMachineTest, ParseMethodCallMultiVariables) {
  Tokenize(R"(
  x = y.randomize(z, w);
  )");
  ExpectTokenSequence(                            //
      {SymbolIdentifier /* x */, '=',             //
       SymbolIdentifier, '.', TK_randomize, '(',  //
       SymbolIdentifier, ',', SymbolIdentifier, ')'});
  EXPECT_TRUE(sm.IsActive());
  ExpectTokenSequence({';'});
  EXPECT_FALSE(sm.IsActive());
}

// Test that internal::RandomizeCallStateMachine updates correctly as a
// predicate.
TEST_F(RandomizeCallStateMachineTest, ParseMethodCallPredicate) {
  Tokenize(R"(
  if (y.randomize) begin
  )");
  ExpectTokenSequence(  //
      {TK_if, '(',      //
       SymbolIdentifier /* y */, '.', TK_randomize});
  EXPECT_TRUE(sm.IsActive());
  ExpectTokenSequence({')'});
  EXPECT_FALSE(sm.IsActive());
  ExpectTokenSequence({TK_begin});
  EXPECT_FALSE(sm.IsActive());
}

// Test that internal::RandomizeCallStateMachine updates correctly with
// constraint_block.
TEST_F(RandomizeCallStateMachineTest, ParseMethodCallWithConstraintBlock) {
  Tokenize(R"(
  if (y.randomize with {a -> b;}) begin
  )");
  ExpectTokenSequence(  //
      {TK_if, '(',      //
       SymbolIdentifier /* y */, '.', TK_randomize});
  EXPECT_TRUE(sm.IsActive());
  ExpectTokenSequence(  //
      {TK_with, '{',    //
       SymbolIdentifier /* a */, TK_CONSTRAINT_IMPLIES, SymbolIdentifier, ';',
       '}'});
  EXPECT_FALSE(sm.IsActive());
  ExpectTokenSequence({')', TK_begin});
  EXPECT_FALSE(sm.IsActive());
}

// Test that internal::RandomizeCallStateMachine updates correctly with
// constraint_block with empty variable list.
TEST_F(RandomizeCallStateMachineTest,
       ParseMethodCallWithConstraintBlockAndEmptyVariableList) {
  Tokenize(R"(
  if (y.randomize with () {a -> b;}) begin
  )");
  ExpectTokenSequence(  //
      {TK_if, '(',      //
       SymbolIdentifier /* y */, '.', TK_randomize});
  EXPECT_TRUE(sm.IsActive());
  ExpectTokenSequence(                                                     //
      {TK_with, '(', ')', '{',                                             //
       SymbolIdentifier /* a */, TK_CONSTRAINT_IMPLIES, SymbolIdentifier,  //
       ';', '}'});
  EXPECT_FALSE(sm.IsActive());
  ExpectTokenSequence({')', TK_begin});
  EXPECT_FALSE(sm.IsActive());
}

// Test that internal::RandomizeCallStateMachine updates correctly with
// constraint_block with non-empty variable list.
TEST_F(RandomizeCallStateMachineTest,
       ParseMethodCallWithConstraintBlockAndVariableList) {
  Tokenize(R"(
  if (y.randomize with (j, k) {a -> b;}) begin
  )");
  ExpectTokenSequence({TK_if, '(',  //
                       SymbolIdentifier /* y */, '.', TK_randomize});
  EXPECT_TRUE(sm.IsActive());
  ExpectTokenSequence({TK_with, '(', SymbolIdentifier /* j */, ',',
                       SymbolIdentifier /* k */, ')', '{',
                       SymbolIdentifier /* a */, TK_CONSTRAINT_IMPLIES,
                       SymbolIdentifier /* b */, ';', '}'});
  EXPECT_FALSE(sm.IsActive());
  ExpectTokenSequence({')', TK_begin});
  EXPECT_FALSE(sm.IsActive());
}

// Class for testing some internal methods of LexicalContext.
class LexicalContextTest : public ::testing::Test, public LexicalContext {
 protected:
  LexicalContextTest() = default;

  void CheckInitialState() const {
    EXPECT_EQ(previous_token_, nullptr);
    EXPECT_FALSE(in_extern_declaration_);
    EXPECT_FALSE(in_function_declaration_);
    EXPECT_FALSE(in_function_body_);
    EXPECT_FALSE(in_task_declaration_);
    EXPECT_FALSE(in_task_body_);
    EXPECT_FALSE(in_module_declaration_);
    EXPECT_FALSE(in_module_body_);
    EXPECT_FALSE(randomize_call_tracker_.IsActive());
    EXPECT_FALSE(constraint_declaration_tracker_.IsActive());
    EXPECT_FALSE(InAnyDeclaration());
    EXPECT_FALSE(InAnyDeclarationHeader());
    EXPECT_TRUE(flow_control_stack_.empty());
    EXPECT_TRUE(keyword_label_tracker_.ItemMayStart());
    EXPECT_TRUE(balance_stack_.empty());
    EXPECT_TRUE(block_stack_.empty());
    EXPECT_EQ_REASON(ExpectingBodyItemStart(), true, "first token");
  }

  void AdvanceToken() {
    TokenSequence::iterator iter(*token_iter_);
    TokenInfo &token(*iter);
    LexicalContext::AdvanceToken(&token);
    ++token_iter_;
  }

  static void ExpectCurrentTokenEnum(TokenStreamReferenceView::iterator iter,
                                     int expect_token_enum) {
    const int got_token_enum = (*iter)->token_enum();
    EXPECT_EQ(got_token_enum, expect_token_enum)
        << " from token " << **iter << " ("
        << verilog_symbol_name(got_token_enum) << " vs. "
        << verilog_symbol_name(expect_token_enum) << ')';
  }

  // Advances the token iterator once for every element in token_enums,
  // verifying token enumerations along the way.
  // Use this helper method to quickly advance through a sequence of tokens
  // without checking other interesting properties.  This also verifies that
  // the token was *not* transformed by the LexicalContext.
  void ExpectTokenSequence(std::initializer_list<int> token_enums) {
    for (const int token_enum : token_enums) {
      ExpectCurrentTokenEnum(token_iter_, token_enum);
      AdvanceToken();
      EXPECT_EQ(previous_token_->token_enum(), token_enum)
          << " from token " << **token_iter_ << " ("
          << verilog_symbol_name(previous_token_->token_enum()) << " vs. "
          << verilog_symbol_name(token_enum) << ')';
    }
  }

  // Advances the token iterator once, verifying the token enumeration before
  // and after advancement.
  void ExpectTransformedToken(int token_enum_before, int token_enum_after) {
    ExpectCurrentTokenEnum(token_iter_, token_enum_before);
    AdvanceToken();
    EXPECT_EQ(previous_token_->token_enum(), token_enum_after)
        << " (" << verilog_symbol_name(previous_token_->token_enum()) << " vs. "
        << verilog_symbol_name(token_enum_after) << ')';
  }

  // Lexes code and initializes token_iter to point to the first token.
  void Tokenize(const std::string &code) {
    analyzer_ = std::make_unique<VerilogAnalyzer>(code, "");
    EXPECT_OK(analyzer_->Tokenize());
    analyzer_->FilterTokensForSyntaxTree();
    token_refs_ = analyzer_->MutableData().MakeTokenStreamReferenceView();
    token_iter_ = token_refs_.begin();
  }

  // Parser, used only for lexing.
  std::unique_ptr<VerilogAnalyzer> analyzer_;

  // Modifiable handles into token stream.
  TokenStreamReferenceView token_refs_;

  // Iterator over the filtered token stream.
  TokenStreamReferenceView::iterator token_iter_;
};

// Test that construction and initialization work.
TEST_F(LexicalContextTest, Initialization) { CheckInitialState(); }

// Test that token stream initialization works.
TEST_F(LexicalContextTest, ScanEmptyTokens) {
  Tokenize("");
  EXPECT_EQ(token_refs_.size(), 1);  // only EOF token
  AdvanceToken();
  // Don't really care what the state is after EOF, just don't crash.
}

// Test that context of function declaration is correct.
TEST_F(LexicalContextTest, ScanEmptyFunctionDeclaration) {
  const char code[] = "function void foo; endfunction";
  Tokenize(code);
  CheckInitialState();
  EXPECT_EQ(token_refs_.size(), 6);  // including EOF token

  ExpectTokenSequence({TK_function});
  EXPECT_TRUE(in_function_declaration_);
  EXPECT_FALSE(in_function_body_);
  EXPECT_EQ_REASON(ExpectingBodyItemStart(), false,
                   "in other declaration header");
  EXPECT_FALSE(ExpectingStatement());

  ExpectTokenSequence({TK_void});
  EXPECT_TRUE(in_function_declaration_);
  EXPECT_FALSE(in_function_body_);
  EXPECT_EQ_REASON(ExpectingBodyItemStart(), false,
                   "in other declaration header");
  EXPECT_FALSE(ExpectingStatement());

  ExpectTokenSequence({SymbolIdentifier});
  EXPECT_TRUE(in_function_declaration_);
  EXPECT_FALSE(in_function_body_);
  EXPECT_EQ_REASON(ExpectingBodyItemStart(), false,
                   "in other declaration header");
  EXPECT_FALSE(ExpectingStatement());

  ExpectTokenSequence({';'});
  EXPECT_TRUE(in_function_declaration_);
  EXPECT_TRUE(in_function_body_);
  EXPECT_EQ_REASON(ExpectingBodyItemStart(), true, "end of header");
  EXPECT_TRUE(ExpectingStatement());

  ExpectTokenSequence({TK_endfunction});
  EXPECT_FALSE(in_function_declaration_);
  EXPECT_FALSE(in_function_body_);
  EXPECT_EQ_REASON(ExpectingBodyItemStart(), true, "item may start");
  EXPECT_FALSE(ExpectingStatement());
}

// Test for correct context in function declaration with empty ports.
TEST_F(LexicalContextTest, ScanFunctionDeclarationEmptyPorts) {
  const char code[] = "function void foo(); endfunction";
  Tokenize(code);
  CheckInitialState();
  EXPECT_EQ(token_refs_.size(), 8);

  ExpectTokenSequence({TK_function});
  EXPECT_TRUE(in_function_declaration_);
  EXPECT_FALSE(in_function_body_);
  EXPECT_FALSE(ExpectingBodyItemStart().value);
  EXPECT_FALSE(ExpectingStatement());

  ExpectTokenSequence({TK_void});
  EXPECT_TRUE(in_function_declaration_);
  EXPECT_FALSE(in_function_body_);
  EXPECT_FALSE(ExpectingBodyItemStart().value);
  EXPECT_FALSE(ExpectingStatement());

  ExpectTokenSequence({SymbolIdentifier});
  EXPECT_TRUE(in_function_declaration_);
  EXPECT_FALSE(in_function_body_);
  EXPECT_FALSE(ExpectingBodyItemStart().value);
  EXPECT_FALSE(ExpectingStatement());
  EXPECT_TRUE(balance_stack_.empty());

  ExpectTokenSequence({'('});
  EXPECT_TRUE(in_function_declaration_);
  EXPECT_FALSE(in_function_body_);
  EXPECT_FALSE(ExpectingBodyItemStart().value);
  EXPECT_FALSE(ExpectingStatement());
  EXPECT_EQ(balance_stack_.size(), 1);

  ExpectTokenSequence({')'});
  EXPECT_TRUE(in_function_declaration_);
  EXPECT_FALSE(in_function_body_);
  EXPECT_FALSE(ExpectingBodyItemStart().value);
  EXPECT_FALSE(ExpectingStatement());
  EXPECT_TRUE(balance_stack_.empty());

  ExpectTokenSequence({';'});
  EXPECT_TRUE(in_function_declaration_);
  EXPECT_TRUE(in_function_body_);
  EXPECT_TRUE(ExpectingStatement());
  EXPECT_EQ_REASON(ExpectingBodyItemStart(), true, "end of header");

  ExpectTokenSequence({TK_endfunction});
  EXPECT_FALSE(in_function_declaration_);
  EXPECT_FALSE(in_function_body_);
  EXPECT_FALSE(ExpectingStatement());
}

// Test for correct context in function declaration with ports.
TEST_F(LexicalContextTest, ScanFunctionDeclarationWithPorts) {
  const char code[] = "function void foo(int a, int b); endfunction";
  Tokenize(code);
  CheckInitialState();
  EXPECT_EQ(token_refs_.size(), 13);

  ExpectTokenSequence({TK_function});
  EXPECT_TRUE(in_function_declaration_);
  EXPECT_FALSE(in_function_body_);
  EXPECT_FALSE(ExpectingBodyItemStart().value);
  EXPECT_FALSE(ExpectingStatement());

  ExpectTokenSequence({TK_void});
  EXPECT_TRUE(in_function_declaration_);
  EXPECT_FALSE(in_function_body_);
  EXPECT_FALSE(ExpectingBodyItemStart().value);
  EXPECT_FALSE(ExpectingStatement());

  ExpectTokenSequence({SymbolIdentifier});
  EXPECT_TRUE(in_function_declaration_);
  EXPECT_FALSE(in_function_body_);
  EXPECT_FALSE(ExpectingBodyItemStart().value);
  EXPECT_FALSE(ExpectingStatement());
  EXPECT_TRUE(balance_stack_.empty());

  ExpectTokenSequence({'('});
  EXPECT_TRUE(in_function_declaration_);
  EXPECT_FALSE(in_function_body_);
  EXPECT_FALSE(ExpectingBodyItemStart().value);
  EXPECT_FALSE(ExpectingStatement());
  EXPECT_EQ(balance_stack_.size(), 1);

  ExpectTokenSequence({TK_int});
  EXPECT_TRUE(in_function_declaration_);
  EXPECT_FALSE(in_function_body_);
  EXPECT_FALSE(ExpectingBodyItemStart().value);
  EXPECT_FALSE(ExpectingStatement());
  EXPECT_EQ(balance_stack_.size(), 1);

  ExpectTokenSequence({SymbolIdentifier});
  EXPECT_TRUE(in_function_declaration_);
  EXPECT_FALSE(in_function_body_);
  EXPECT_FALSE(ExpectingBodyItemStart().value);
  EXPECT_FALSE(ExpectingStatement());
  EXPECT_EQ(balance_stack_.size(), 1);

  ExpectTokenSequence({','});
  EXPECT_TRUE(in_function_declaration_);
  EXPECT_FALSE(in_function_body_);
  EXPECT_FALSE(ExpectingBodyItemStart().value);
  EXPECT_FALSE(ExpectingStatement());
  EXPECT_EQ(balance_stack_.size(), 1);

  ExpectTokenSequence({TK_int});
  EXPECT_TRUE(in_function_declaration_);
  EXPECT_FALSE(in_function_body_);
  EXPECT_FALSE(ExpectingBodyItemStart().value);
  EXPECT_FALSE(ExpectingStatement());
  EXPECT_EQ(balance_stack_.size(), 1);

  ExpectTokenSequence({SymbolIdentifier});
  EXPECT_TRUE(in_function_declaration_);
  EXPECT_FALSE(in_function_body_);
  EXPECT_FALSE(ExpectingBodyItemStart().value);
  EXPECT_FALSE(ExpectingStatement());
  EXPECT_EQ(balance_stack_.size(), 1);

  ExpectTokenSequence({')'});
  EXPECT_TRUE(in_function_declaration_);
  EXPECT_FALSE(in_function_body_);
  EXPECT_FALSE(ExpectingBodyItemStart().value);
  EXPECT_FALSE(ExpectingStatement());
  EXPECT_TRUE(balance_stack_.empty());

  ExpectTokenSequence({';'});
  EXPECT_TRUE(in_function_declaration_);
  EXPECT_TRUE(in_function_body_);
  EXPECT_TRUE(ExpectingStatement());
  EXPECT_EQ_REASON(ExpectingBodyItemStart(), true, "end of header");

  ExpectTokenSequence({TK_endfunction});
  EXPECT_FALSE(in_function_declaration_);
  EXPECT_FALSE(in_function_body_);
  EXPECT_EQ_REASON(ExpectingBodyItemStart(), true, "item may start");
  EXPECT_FALSE(ExpectingStatement());
}

// Test that '->' is correctly disambiguated inside a function.
TEST_F(LexicalContextTest, ScanFunctionDeclarationWithRightArrows) {
  const char code[] = R"(
  function void foo;
    -> z;  // event-trigger
    if (a -> b) -> y;  // implies, event-trigger
    -> w;  // event-trigger
    for (; c -> d; ) begin  // implies
      -> y;  // event-trigger
    end
  endfunction
  )";
  Tokenize(code);
  CheckInitialState();
  EXPECT_EQ(token_refs_.size(), 34);  // including EOF token

  ExpectTokenSequence({TK_function, TK_void, SymbolIdentifier, ';'});

  ExpectTransformedToken(_TK_RARROW, TK_TRIGGER);

  ExpectTokenSequence({SymbolIdentifier, ';'});
  EXPECT_FALSE(InFlowControlHeader());

  ExpectTokenSequence({TK_if});
  EXPECT_TRUE(InFlowControlHeader());

  ExpectTokenSequence({'(', SymbolIdentifier});

  ExpectTransformedToken(_TK_RARROW, TK_LOGICAL_IMPLIES);

  ExpectTokenSequence({SymbolIdentifier});
  EXPECT_TRUE(InFlowControlHeader());

  ExpectTokenSequence({')'});
  EXPECT_FALSE(InFlowControlHeader());
  EXPECT_FALSE(InAnyDeclarationHeader());
  EXPECT_TRUE(in_function_body_);
  EXPECT_TRUE(previous_token_finished_header_);
  EXPECT_EQ_REASON(ExpectingBodyItemStart(), true, "end of header");
  EXPECT_TRUE(ExpectingStatement());

  ExpectTransformedToken(_TK_RARROW, TK_TRIGGER);

  ExpectTokenSequence({SymbolIdentifier /* y */, ';'});

  ExpectTransformedToken(_TK_RARROW, TK_TRIGGER);

  ExpectTokenSequence({SymbolIdentifier /* w */, ';', TK_for, '(', ';',
                       SymbolIdentifier /* c */});

  ExpectTransformedToken(_TK_RARROW, TK_LOGICAL_IMPLIES);

  ExpectTokenSequence({SymbolIdentifier /* d */, ';', ')'});
  EXPECT_FALSE(InFlowControlHeader());
  EXPECT_FALSE(InAnyDeclarationHeader());
  EXPECT_TRUE(in_function_body_);
  EXPECT_TRUE(previous_token_finished_header_);
  EXPECT_EQ_REASON(ExpectingBodyItemStart(), true, "end of header");
  EXPECT_TRUE(ExpectingStatement());

  ExpectTokenSequence({TK_begin});
  EXPECT_EQ_REASON(ExpectingBodyItemStart(), true, "item may start");
  EXPECT_TRUE(ExpectingStatement());

  ExpectTransformedToken(_TK_RARROW, TK_TRIGGER);

  ExpectTokenSequence({SymbolIdentifier /* y */, ';', TK_end, TK_endfunction});
  EXPECT_FALSE(in_function_declaration_);
  EXPECT_FALSE(in_function_body_);
}

// Test that '->' is correctly disambiguated, handling keyword labels.
TEST_F(LexicalContextTest,
       ScanFunctionDeclarationWithRightArrowsControlLabels) {
  const char code[] = R"(
  function void foo;
    if (a -> b) begin : bar
      -> y;  // implies, event-trigger
    end : bar
    -> z;  // event-trigger
  endfunction
  )";
  Tokenize(code);
  CheckInitialState();
  EXPECT_EQ(token_refs_.size(), 24);  // including EOF token

  ExpectTokenSequence({TK_function, TK_void, SymbolIdentifier, ';', TK_if});

  EXPECT_TRUE(InFlowControlHeader());

  ExpectTokenSequence({'(', SymbolIdentifier});

  ExpectTransformedToken(_TK_RARROW, TK_LOGICAL_IMPLIES);

  ExpectTokenSequence({SymbolIdentifier});
  EXPECT_TRUE(InFlowControlHeader());

  ExpectTokenSequence({')'});
  EXPECT_FALSE(InFlowControlHeader());
  EXPECT_FALSE(InAnyDeclarationHeader());
  EXPECT_TRUE(in_function_body_);
  EXPECT_TRUE(previous_token_finished_header_);
  EXPECT_EQ_REASON(ExpectingBodyItemStart(), true, "end of header");
  EXPECT_TRUE(ExpectingStatement());

  ExpectTokenSequence({TK_begin, ':', SymbolIdentifier /* bar */});

  ExpectTransformedToken(_TK_RARROW, TK_TRIGGER);

  ExpectTokenSequence(
      {SymbolIdentifier /* y */, ';', TK_end, ':', SymbolIdentifier /* bar */});

  ExpectTransformedToken(_TK_RARROW, TK_TRIGGER);

  ExpectTokenSequence({SymbolIdentifier /* w */, ';', TK_endfunction});
  EXPECT_FALSE(in_function_declaration_);
  EXPECT_FALSE(in_function_body_);
}

// Test that extern function declaration does not expect a declaration body.
TEST_F(LexicalContextTest, ScanExternMethodDeclaration) {
  const char code[] = R"(
class n;
  extern function foo;
endclass
)";
  Tokenize(code);
  CheckInitialState();
  EXPECT_EQ(token_refs_.size(), 9);  // including EOF token

  ExpectTokenSequence({TK_class, SymbolIdentifier, ';', TK_extern});
  EXPECT_TRUE(in_extern_declaration_);

  ExpectTokenSequence({TK_function, SymbolIdentifier, ';'});
  // Make sure not in function body context here because of extern declaration.
  EXPECT_FALSE(in_function_declaration_);
  EXPECT_FALSE(in_function_body_);
  EXPECT_FALSE(in_extern_declaration_);

  ExpectTokenSequence({TK_endclass});
}

// Test that extern function declaration does not expect a declaration body.
TEST_F(LexicalContextTest, ScanExternMethodDeclarationWithEmptyPorts) {
  const char code[] = R"(
class n;
  extern function foo();
endclass
)";
  Tokenize(code);
  CheckInitialState();
  EXPECT_EQ(token_refs_.size(), 11);  // including EOF token

  ExpectTokenSequence({TK_class, SymbolIdentifier, ';', TK_extern});
  EXPECT_TRUE(in_extern_declaration_);

  ExpectTokenSequence({TK_function, SymbolIdentifier, '(', ')', ';'});

  // Make sure not in function body context here because of extern declaration.
  EXPECT_FALSE(in_function_declaration_);
  EXPECT_FALSE(in_function_body_);
  EXPECT_FALSE(in_extern_declaration_);

  ExpectTokenSequence({TK_endclass});
}

// Test that extern function declaration does not expect a declaration body.
TEST_F(LexicalContextTest, ScanExternMethodDeclarationWithSomePorts) {
  const char code[] = R"(
class n;
  extern function foo(int bar);
endclass
)";
  Tokenize(code);
  CheckInitialState();
  EXPECT_EQ(token_refs_.size(), 13);  // including EOF token

  ExpectTokenSequence({TK_class, SymbolIdentifier, ';', TK_extern});
  EXPECT_TRUE(in_extern_declaration_);

  ExpectTokenSequence(
      {TK_function, SymbolIdentifier, '(', TK_int, SymbolIdentifier, ')', ';'});

  // Make sure not in function body context here because of extern declaration.
  EXPECT_FALSE(in_function_declaration_);
  EXPECT_FALSE(in_function_body_);
  EXPECT_FALSE(in_extern_declaration_);

  ExpectTokenSequence({TK_endclass});
}

// Test that extern task declaration does not expect a declaration body.
TEST_F(LexicalContextTest, ScanExternTaskDeclaration) {
  const char code[] = R"(
class n;
  extern task foo;
endclass
)";
  Tokenize(code);
  CheckInitialState();
  EXPECT_EQ(token_refs_.size(), 9);  // including EOF token

  ExpectTokenSequence({TK_class, SymbolIdentifier, ';', TK_extern});
  EXPECT_TRUE(in_extern_declaration_);

  ExpectTokenSequence({TK_task, SymbolIdentifier, ';'});
  // Make sure not in task body context here because of extern declaration.
  EXPECT_FALSE(in_task_declaration_);
  EXPECT_FALSE(in_task_body_);
  EXPECT_FALSE(in_extern_declaration_);

  ExpectTokenSequence({TK_endclass});
}

// Test that extern constraint prototype does not expect a constraint_block.
TEST_F(LexicalContextTest, ScanExternConstraintPrototype) {
  const char code[] = R"(
class n;
  extern constraint foo;
endclass
  )";
  Tokenize(code);
  CheckInitialState();
  EXPECT_EQ(token_refs_.size(), 9);  // including EOF token

  ExpectTokenSequence({TK_class, SymbolIdentifier, ';', TK_extern});
  EXPECT_TRUE(in_extern_declaration_);

  ExpectTokenSequence({TK_constraint});
  // constraint_prototype should not activate declaration tracker.
  EXPECT_FALSE(constraint_declaration_tracker_.IsActive());
  EXPECT_FALSE(in_extern_declaration_);  // reset

  ExpectTokenSequence({SymbolIdentifier, ';'});
  EXPECT_FALSE(constraint_declaration_tracker_.IsActive());

  ExpectTokenSequence({TK_endclass});
}

// Test that extern function declaration does not expect a declaration body,
// and that "->" is correctly interpreted as constraint-implication.
TEST_F(LexicalContextTest,
       ScanExternMethodDeclarationFollowedByConstraintImplies) {
  const char code[] = R"(
class n;
  extern function foo;
endclass

constraint v {
  m -> {
    x != y;
  }
}
  )";
  Tokenize(code);
  CheckInitialState();
  EXPECT_EQ(token_refs_.size(), 21);  // including EOF token

  ExpectTokenSequence({TK_class, SymbolIdentifier, ';', TK_extern});
  EXPECT_TRUE(in_extern_declaration_);

  ExpectTokenSequence({TK_function, SymbolIdentifier, ';'});
  // Make sure not in function body context here because of extern declaration.
  EXPECT_FALSE(in_function_declaration_);
  EXPECT_FALSE(in_function_body_);
  EXPECT_FALSE(in_extern_declaration_);

  ExpectTokenSequence({TK_endclass, TK_constraint});
  EXPECT_TRUE(constraint_declaration_tracker_.IsActive());
  ExpectTokenSequence({SymbolIdentifier, '{', SymbolIdentifier});

  ExpectTransformedToken(_TK_RARROW, TK_CONSTRAINT_IMPLIES);

  ExpectTokenSequence(
      {'{', SymbolIdentifier, TK_NE, SymbolIdentifier, ';', '}', '}'});
  EXPECT_FALSE(constraint_declaration_tracker_.IsActive());
}

// Test that '->' is correctly disambiguated, handling randomize-with.
TEST_F(LexicalContextTest, ScanRandomizeWithConstraintBlock) {
  const char code[] = R"(
function void rat(seq_item item);
  if (!item.randomize() with
      {
        (x -> y) -> {
          a inside {[1 : 2]};
        }
      }) begin
    `uvm_fatal("rat", "failed")
  end
endfunction : rat
  )";
  Tokenize(code);
  CheckInitialState();

  ExpectTokenSequence({TK_function, TK_void, SymbolIdentifier, '(',
                       SymbolIdentifier, SymbolIdentifier, ')', ';'});
  ExpectTokenSequence({TK_if, '(', '!', SymbolIdentifier, '.', TK_randomize});
  EXPECT_TRUE(randomize_call_tracker_.IsActive());

  ExpectTokenSequence({'(', ')', TK_with, '{', '(', SymbolIdentifier});
  ExpectTransformedToken(_TK_RARROW, TK_LOGICAL_IMPLIES);
  ExpectTokenSequence({SymbolIdentifier, ')'});
  ExpectTransformedToken(_TK_RARROW, TK_CONSTRAINT_IMPLIES);

  ExpectTokenSequence({'{', SymbolIdentifier, TK_inside, '{', '[', TK_DecNumber,
                       ':', TK_DecNumber, ']', '}', ';', '}', '}'});
  EXPECT_FALSE(randomize_call_tracker_.IsActive());
  ExpectTokenSequence({')', TK_begin});
}

// Test that '->' is correctly disambiguated inside a task.
TEST_F(LexicalContextTest, ScanTaskDeclarationWithRightArrows) {
  const char code[] = R"(
  task foo;
    -> z;  // event-trigger
    if (a -> b) -> y;  // implies, event-trigger
    -> w;  // event-trigger
    for (; c -> d; ) begin  // implies
      -> y;  // event-trigger
    end
  endtask
  )";
  Tokenize(code);
  CheckInitialState();
  EXPECT_EQ(token_refs_.size(), 33);  // including EOF token

  ExpectTokenSequence({TK_task});
  EXPECT_TRUE(in_task_declaration_);
  EXPECT_FALSE(in_task_body_);

  ExpectTokenSequence({SymbolIdentifier, ';'});
  EXPECT_TRUE(in_task_declaration_);
  EXPECT_TRUE(in_task_body_);

  ExpectTransformedToken(_TK_RARROW, TK_TRIGGER);

  ExpectTokenSequence({SymbolIdentifier, ';'});
  EXPECT_FALSE(InFlowControlHeader());

  ExpectTokenSequence({TK_if});
  EXPECT_TRUE(InFlowControlHeader());

  ExpectTokenSequence({'(', SymbolIdentifier});

  ExpectTransformedToken(_TK_RARROW, TK_LOGICAL_IMPLIES);

  ExpectTokenSequence({SymbolIdentifier});
  EXPECT_TRUE(InFlowControlHeader());

  ExpectTokenSequence({')'});
  EXPECT_FALSE(InFlowControlHeader());
  EXPECT_FALSE(InAnyDeclarationHeader());
  EXPECT_TRUE(in_task_body_);
  EXPECT_TRUE(previous_token_finished_header_);
  EXPECT_EQ_REASON(ExpectingBodyItemStart(), true, "end of header");
  EXPECT_TRUE(ExpectingStatement());

  ExpectTransformedToken(_TK_RARROW, TK_TRIGGER);

  ExpectTokenSequence({SymbolIdentifier /* y */, ';'});

  ExpectTransformedToken(_TK_RARROW, TK_TRIGGER);

  ExpectTokenSequence({SymbolIdentifier /* w */, ';', TK_for, '(', ';',
                       SymbolIdentifier /* c */});

  ExpectTransformedToken(_TK_RARROW, TK_LOGICAL_IMPLIES);

  ExpectTokenSequence({SymbolIdentifier /* d */, ';', ')'});
  EXPECT_FALSE(InFlowControlHeader());
  EXPECT_FALSE(InAnyDeclarationHeader());
  EXPECT_TRUE(in_task_body_);
  EXPECT_TRUE(previous_token_finished_header_);
  EXPECT_EQ_REASON(ExpectingBodyItemStart(), true, "end of header");
  EXPECT_TRUE(ExpectingStatement());

  ExpectTokenSequence({TK_begin});
  EXPECT_EQ_REASON(ExpectingBodyItemStart(), true, "item may start");
  EXPECT_TRUE(ExpectingStatement());

  ExpectTransformedToken(_TK_RARROW, TK_TRIGGER);

  ExpectTokenSequence({SymbolIdentifier /* y */, ';', TK_end, TK_endtask});
  EXPECT_FALSE(in_task_declaration_);
  EXPECT_FALSE(in_task_body_);
}

// Test that '->' is correctly disambiguated inside task,
// handling keyword labels.
TEST_F(LexicalContextTest, ScanTaskDeclarationWithRightArrowsControlLabels) {
  const char code[] = R"(
  task foo;
    if (a -> b) begin : bar
      -> y;  // implies, event-trigger
    end : bar
    -> z;  // event-trigger
  endtask
  )";
  Tokenize(code);
  CheckInitialState();
  EXPECT_EQ(token_refs_.size(), 23);  // including EOF token

  ExpectTokenSequence({TK_task, SymbolIdentifier, ';', TK_if});
  EXPECT_TRUE(InFlowControlHeader());

  ExpectTokenSequence({'(', SymbolIdentifier});

  ExpectTransformedToken(_TK_RARROW, TK_LOGICAL_IMPLIES);

  ExpectTokenSequence({SymbolIdentifier});
  EXPECT_TRUE(InFlowControlHeader());

  ExpectTokenSequence({')'});
  EXPECT_FALSE(InFlowControlHeader());
  EXPECT_FALSE(InAnyDeclarationHeader());
  EXPECT_TRUE(in_task_body_);
  EXPECT_TRUE(previous_token_finished_header_);
  EXPECT_EQ_REASON(ExpectingBodyItemStart(), true, "end of header");
  EXPECT_TRUE(ExpectingStatement());

  ExpectTokenSequence({TK_begin, ':', SymbolIdentifier /* bar */});

  ExpectTransformedToken(_TK_RARROW, TK_TRIGGER);

  ExpectTokenSequence(
      {SymbolIdentifier /* y */, ';', TK_end, ':', SymbolIdentifier /* bar */});

  ExpectTransformedToken(_TK_RARROW, TK_TRIGGER);

  ExpectTokenSequence({SymbolIdentifier /* w */, ';', TK_endtask});
}

// Test that '->' is correctly disambiguated inside initial blocks.
TEST_F(LexicalContextTest, ScanInitialStatementEventTrigger) {
  const char code[] = R"(
  module foo;
  initial -> x;  // -> should be event trigger
  endmodule
  )";
  Tokenize(code);
  CheckInitialState();
  EXPECT_EQ(token_refs_.size(), 9);  // including EOF token

  ExpectTokenSequence({TK_module});
  EXPECT_TRUE(in_module_declaration_);
  EXPECT_FALSE(in_module_body_);

  ExpectTokenSequence({SymbolIdentifier, ';'});
  EXPECT_TRUE(in_module_declaration_);
  EXPECT_TRUE(in_module_body_);

  ExpectTokenSequence({TK_initial});
  EXPECT_TRUE(in_initial_always_final_construct_);

  ExpectTransformedToken(_TK_RARROW, TK_TRIGGER);

  ExpectTokenSequence({SymbolIdentifier, ';'});
  EXPECT_FALSE(in_initial_always_final_construct_);

  ExpectTokenSequence({TK_endmodule});
  EXPECT_FALSE(in_module_declaration_);
  EXPECT_FALSE(in_module_body_);
}

// Test that '->' is correctly interpreted as a logical implication.
TEST_F(LexicalContextTest, AssignmentToLogcalImplicationExpression) {
  const char code[] = R"(
  module foo;
  assign a = b -> x;
  endmodule
  )";
  Tokenize(code);
  CheckInitialState();
  EXPECT_EQ(token_refs_.size(), 12);  // including EOF token

  ExpectTokenSequence({TK_module});
  EXPECT_TRUE(in_module_declaration_);
  EXPECT_FALSE(in_module_body_);

  ExpectTokenSequence({SymbolIdentifier, ';'});
  EXPECT_TRUE(in_module_declaration_);
  EXPECT_TRUE(in_module_body_);

  ExpectTokenSequence({TK_assign});
  EXPECT_FALSE(in_initial_always_final_construct_);

  ExpectTokenSequence(
      {SymbolIdentifier /* a */, '=', SymbolIdentifier /* b */});

  ExpectTransformedToken(_TK_RARROW, TK_LOGICAL_IMPLIES);

  ExpectTokenSequence({SymbolIdentifier /* x */, ';'});
  EXPECT_FALSE(in_initial_always_final_construct_);

  ExpectTokenSequence({TK_endmodule});
  EXPECT_FALSE(in_module_declaration_);
  EXPECT_FALSE(in_module_body_);
}

// Test that '->' is correctly interpreted as a logical implication.
TEST_F(LexicalContextTest, AssignmentToLogcalImplicationExpressionInSeqBlock) {
  const char code[] = R"(
  module foo;
    initial begin
      a = b -> x;
    end
  endmodule
  )";
  Tokenize(code);
  CheckInitialState();
  EXPECT_EQ(token_refs_.size(), 14);  // including EOF token

  ExpectTokenSequence({TK_module});
  EXPECT_TRUE(in_module_declaration_);
  EXPECT_FALSE(in_module_body_);

  ExpectTokenSequence({SymbolIdentifier, ';'});
  EXPECT_TRUE(in_module_declaration_);
  EXPECT_TRUE(in_module_body_);

  ExpectTokenSequence({TK_initial});
  EXPECT_TRUE(ExpectingStatement());
  EXPECT_EQ_REASON(ExpectingBodyItemStart(), true, "initial");
  EXPECT_TRUE(in_initial_always_final_construct_);

  ExpectTokenSequence({TK_begin});
  EXPECT_TRUE(ExpectingStatement());
  EXPECT_EQ_REASON(ExpectingBodyItemStart(), true, "item may start");

  ExpectTokenSequence({SymbolIdentifier /* a */});
  ExpectTokenSequence({'='});
  ExpectTokenSequence({SymbolIdentifier /* b */});

  EXPECT_FALSE(ExpectingStatement());
  EXPECT_EQ_REASON(ExpectingBodyItemStart(), false, "(default)");
  ExpectTransformedToken(_TK_RARROW, TK_LOGICAL_IMPLIES);

  ExpectTokenSequence({SymbolIdentifier /* x */, ';'});
  EXPECT_TRUE(in_initial_always_final_construct_);

  ExpectTokenSequence({TK_end});
  EXPECT_FALSE(in_initial_always_final_construct_);

  ExpectTokenSequence({TK_endmodule});
  EXPECT_FALSE(in_module_declaration_);
  EXPECT_FALSE(in_module_body_);
}

// Test that '->' is correctly disambiguated inside initial blocks.
TEST_F(LexicalContextTest, ScanInitialBlockWithRightArrows) {
  const char code[] = R"(
  module foo;
  initial begin
    -> x;
    if (a -> b) begin : bar
      -> y;  // implies, event-trigger
    end : bar
    -> z;  // event-trigger
  end
  endmodule
  )";
  Tokenize(code);
  CheckInitialState();
  EXPECT_EQ(token_refs_.size(), 29);  // including EOF token

  ExpectTokenSequence({TK_module, SymbolIdentifier, ';', TK_initial, TK_begin});

  ExpectTransformedToken(_TK_RARROW, TK_TRIGGER);

  ExpectTokenSequence({SymbolIdentifier /* x */, ';', TK_if});
  EXPECT_TRUE(InFlowControlHeader());

  ExpectTokenSequence({'(', SymbolIdentifier});

  ExpectTransformedToken(_TK_RARROW, TK_LOGICAL_IMPLIES);

  ExpectTokenSequence({SymbolIdentifier});
  EXPECT_TRUE(InFlowControlHeader());

  ExpectTokenSequence({')'});
  EXPECT_FALSE(InFlowControlHeader());
  EXPECT_FALSE(InAnyDeclarationHeader());
  EXPECT_TRUE(in_module_body_);
  EXPECT_TRUE(previous_token_finished_header_);
  EXPECT_EQ_REASON(ExpectingBodyItemStart(), true, "end of header");
  EXPECT_TRUE(ExpectingStatement());

  ExpectTokenSequence({TK_begin, ':', SymbolIdentifier /* bar */});

  ExpectTransformedToken(_TK_RARROW, TK_TRIGGER);

  ExpectTokenSequence(
      {SymbolIdentifier /* y */, ';', TK_end, ':', SymbolIdentifier /* bar */});

  ExpectTransformedToken(_TK_RARROW, TK_TRIGGER);

  ExpectTokenSequence({SymbolIdentifier /* z */, ';', TK_end, TK_endmodule});
}

// Test that '->' is correctly disambiguated as a constraint implication.
TEST_F(LexicalContextTest, ConstraintDeclarationImplication) {
  const char code[] = R"(
  constraint c {
    a -> b;
  }
  )";
  Tokenize(code);
  CheckInitialState();
  EXPECT_EQ(token_refs_.size(), 9);  // including EOF token

  ExpectTokenSequence({TK_constraint});
  EXPECT_TRUE(constraint_declaration_tracker_.IsActive());

  ExpectTokenSequence({SymbolIdentifier, '{'});
  EXPECT_EQ(balance_stack_.size(), 1);

  ExpectTokenSequence({SymbolIdentifier});

  ExpectTransformedToken(_TK_RARROW, TK_CONSTRAINT_IMPLIES);

  ExpectTokenSequence({SymbolIdentifier, ';', '}'});
  EXPECT_TRUE(balance_stack_.empty());
  EXPECT_FALSE(constraint_declaration_tracker_.IsActive());
}

// Test that '->' is correctly disambiguated as logical implication.
TEST_F(LexicalContextTest, ConstraintDeclarationLogicalImplication) {
  const char code[] = R"(
  constraint c {
    if (a -> b) {
      c -> d;
    }
  }
  )";
  Tokenize(code);
  CheckInitialState();
  EXPECT_EQ(token_refs_.size(), 17);  // including EOF token

  ExpectTokenSequence({TK_constraint});
  EXPECT_TRUE(constraint_declaration_tracker_.IsActive());

  ExpectTokenSequence({SymbolIdentifier, '{'});
  EXPECT_EQ(balance_stack_.size(), 1);

  ExpectTokenSequence({TK_if, '(', SymbolIdentifier});

  ExpectTransformedToken(_TK_RARROW, TK_LOGICAL_IMPLIES);

  ExpectTokenSequence({SymbolIdentifier, ')', '{', SymbolIdentifier});

  ExpectTransformedToken(_TK_RARROW, TK_CONSTRAINT_IMPLIES);

  ExpectTokenSequence({SymbolIdentifier, ';', '}'});
  EXPECT_TRUE(constraint_declaration_tracker_.IsActive());

  ExpectTokenSequence({'}'});
  EXPECT_TRUE(balance_stack_.empty());
  EXPECT_FALSE(constraint_declaration_tracker_.IsActive());
}

TEST_F(LexicalContextTest, MacroCallBalance) {
  const char code[] = R"(
`so_call_me_baby()
  )";
  Tokenize(code);
  CheckInitialState();
  EXPECT_EQ(token_refs_.size(), 4);  // including EOF token

  ExpectTokenSequence({MacroCallId});
  EXPECT_TRUE(balance_stack_.empty());
  ExpectTokenSequence({'('});
  EXPECT_EQ(balance_stack_.size(), 1);
  ExpectTokenSequence({MacroCallCloseToEndLine});  // ')'
  EXPECT_TRUE(balance_stack_.empty());
}

TEST_F(LexicalContextTest, MacroCallBalanceWithComment) {
  const char code[] = R"(
`so_call_me_baby()  // comment
  )";
  Tokenize(code);
  CheckInitialState();

  ExpectTokenSequence({MacroCallId});
  EXPECT_TRUE(balance_stack_.empty());
  ExpectTokenSequence({'('});
  EXPECT_EQ(balance_stack_.size(), 1);
  ExpectTokenSequence({MacroCallCloseToEndLine});  // ')'
  EXPECT_TRUE(balance_stack_.empty());
  // comment token is filtered out
}

TEST_F(LexicalContextTest, MacroCallBalanceSemicolon) {
  const char code[] = R"(
`macro1(foo+bar, `innermacro(11));
  )";
  Tokenize(code);
  CheckInitialState();

  ExpectTokenSequence({MacroCallId});
  EXPECT_TRUE(balance_stack_.empty());
  ExpectTokenSequence({'('});
  EXPECT_EQ(balance_stack_.size(), 1);

  // "foo+bar" and "`innermacro(11)" are un-lexed
  ExpectTokenSequence({MacroArg, ',', MacroArg});

  EXPECT_EQ(balance_stack_.size(), 1);
  ExpectTokenSequence({')'});

  EXPECT_TRUE(balance_stack_.empty());
  ExpectTokenSequence({';'});
}

TEST_F(LexicalContextTest, TaskEventTrigger) {
  const char code[] = R"(
module foo;
  task bar;
    begin
      -> ack;  // should be event-trigger
    end
  endtask
endmodule
  )";
  Tokenize(code);
  CheckInitialState();
  EXPECT_EQ(token_refs_.size(), 14);  // including EOF token

  ExpectTokenSequence({TK_module, SymbolIdentifier, ';'});
  EXPECT_TRUE(in_module_declaration_);
  EXPECT_TRUE(in_module_body_);
  ExpectTokenSequence({TK_task, SymbolIdentifier, ';'});
  EXPECT_TRUE(in_task_declaration_);
  EXPECT_TRUE(in_task_body_);
  ExpectTokenSequence({TK_begin});
  EXPECT_TRUE(in_task_body_);
  EXPECT_EQ_REASON(ExpectingBodyItemStart(), true, "item may start");
  EXPECT_TRUE(ExpectingStatement());
  ExpectTransformedToken(_TK_RARROW, TK_TRIGGER);
  ExpectTokenSequence({SymbolIdentifier, ';'});
  ExpectTokenSequence({TK_end, TK_endtask, TK_endmodule});
}

TEST_F(LexicalContextTest, TaskIfEventTrigger) {
  const char code[] = R"(
class reset_driver;
  `macro()
endclass: reset_driver
task drv_interface;
  if (m_kind) begin
    -> m_event;
  end
endtask: drv_interface
  )";
  Tokenize(code);
  CheckInitialState();

  ExpectTokenSequence({TK_class, SymbolIdentifier, ';'});
  ExpectTokenSequence({MacroCallId, '(', MacroCallCloseToEndLine});
  EXPECT_TRUE(balance_stack_.empty());
  ExpectTokenSequence({TK_endclass, ':', SymbolIdentifier});
  ExpectTokenSequence({TK_task, SymbolIdentifier, ';'});
  ExpectTokenSequence({TK_if, '(', SymbolIdentifier, ')', TK_begin});

  ExpectTransformedToken(_TK_RARROW, TK_TRIGGER);

  ExpectTokenSequence({SymbolIdentifier, ';', TK_end});
  ExpectTokenSequence({TK_endtask, ':', SymbolIdentifier});
}

TEST_F(LexicalContextTest, RandomizeCallWithConstraintInsideTaskDeclaration) {
  const char code[] = R"(
task wr();
  s = m.randomize() with {
    a -> b;  // should be a constraint-implication
  };
endtask
  )";
  Tokenize(code);
  CheckInitialState();
  ExpectTokenSequence({TK_task, SymbolIdentifier, '(', ')', ';'});
  ExpectTokenSequence({SymbolIdentifier /* s */, '=', SymbolIdentifier, '.',
                       TK_randomize, '(', ')', TK_with, '{',
                       SymbolIdentifier /* a */});
  ExpectTransformedToken(_TK_RARROW, TK_CONSTRAINT_IMPLIES);
  ExpectTokenSequence({SymbolIdentifier, ';', '}', ';', TK_endtask});
}

TEST_F(LexicalContextTest, RandomizeCallWithNestedConstraintImplication) {
  const char code[] = R"(
function void rat();
  if (!item.randomize() with {
      x -> {
          d -> {a;}
      }
      }) begin
  end
endfunction : rat
  )";
  Tokenize(code);
  CheckInitialState();
  ExpectTokenSequence({TK_function, TK_void, SymbolIdentifier, '(', ')', ';'});
  ExpectTokenSequence({TK_if, '(', '!', SymbolIdentifier, '.', TK_randomize,
                       '(', ')', TK_with, '{'});
  ExpectTokenSequence({SymbolIdentifier /* x */});
  ExpectTransformedToken(_TK_RARROW, TK_CONSTRAINT_IMPLIES);
  ExpectTokenSequence({'{', SymbolIdentifier /* d */});
  ExpectTransformedToken(_TK_RARROW, TK_CONSTRAINT_IMPLIES);
  ExpectTokenSequence({'{', SymbolIdentifier /* a */, ';', '}'});
  ExpectTokenSequence({'}', '}', ')', TK_begin, TK_end});
  ExpectTokenSequence({TK_endfunction, ':', SymbolIdentifier});
}

}  // namespace
}  // namespace verilog
