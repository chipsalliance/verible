// Copyright 2017-2019 The Verible Authors.
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

#include "verilog/formatting/token_scanner.h"

#include <ostream>

#include "gtest/gtest.h"
#include "verilog/parser/verilog_token_enum.h"

namespace verilog {
namespace formatter {
namespace {

// Fixture class is needed to directly test protected member types/enums.
class TokenScannerTest : public TokenScanner, public ::testing::Test {
 public:
  using state = TokenScanner::State;

  struct TransitionStateTestCase {
    TokenScanner::TokenScannerState input_state;
    yytokentype transition;
    TokenScanner::TokenScannerState expected_state;
  };

  // Print adaptor to get around protected-ness.
  struct Printer {
    explicit Printer(const TokenScannerState& s) : st(s) {}
    const TokenScannerState& st;
  };

  static const TransitionStateTestCase kTransitionStateTests[];
};

std::ostream& operator<<(std::ostream& stream,
                         const TokenScannerTest::Printer& p) {
  return p.st.Print(stream);
}

TEST_F(TokenScannerTest, TokenScannerStateEquals) {
  TokenScanner::TokenScannerState token_scanner_state;
  token_scanner_state.newline_count = 5;
  TokenScanner::TokenScannerState token_scanner_state2;
  EXPECT_NE(token_scanner_state, token_scanner_state2);
  token_scanner_state.state = TokenScanner::State::kRepeatNewline;
  EXPECT_NE(token_scanner_state, token_scanner_state2);

  token_scanner_state2.newline_count = 5;
  EXPECT_NE(token_scanner_state, token_scanner_state2);
  token_scanner_state2.state = TokenScanner::State::kRepeatNewline;
  EXPECT_EQ(token_scanner_state, token_scanner_state2);
}

// Test that the TokenScanner is initialized with kStart state and 0
// newline_count
TEST_F(TokenScannerTest, TokenScannerInitTest) {
  EXPECT_EQ(current_state_.state, kStart);
  EXPECT_EQ(current_state_.newline_count, 0);
  EXPECT_FALSE(EndState());
  EXPECT_FALSE(RepeatNewlineState());
}

// Test cases for TransitionState. Format is starting TokenScannerState,
// transition, expected TokenScannerState.
// These are only change-detector tests.
const TokenScannerTest::TransitionStateTestCase
    TokenScannerTest::kTransitionStateTests[] = {
        {{state::kStart, 0},
         yytokentype::TK_EOL_COMMENT,
         {state::kNewlineComment, 0}},
        {{state::kStart, 0},
         yytokentype::TK_COMMENT_BLOCK,
         {state::kNewlineComment, 0}},
        {{state::kStart, 0}, yytokentype::TK_NEWLINE, {state::kNewline, 1}},
        {{state::kStart, 0}, yytokentype::TK_module, {state::kStart, 0}},
        {{state::kNewline, 1},
         yytokentype::TK_NEWLINE,
         {state::kRepeatNewline, 2}},
        {{state::kRepeatNewline, 4},
         yytokentype::TK_endfunction,
         {state::kStart, 0}},
        {{state::kRepeatNewline, 4},
         yytokentype::TK_NEWLINE,
         {state::kRepeatNewline, 5}},
        {{state::kRepeatNewline, 10},
         yytokentype::TK_COMMENT_BLOCK,
         {state::kNewlineComment, 0}},
        {{state::kNewlineComment, 0},
         yytokentype::TK_context,
         {state::kStart, 0}},
        {{state::kNewlineComment, 0},
         yytokentype::TK_EOL_COMMENT,
         {state::kNewlineComment, 0}},
        {{state::kNewlineComment, 0},
         yytokentype::TK_NEWLINE,
         {state::kEnd, 1}},
        {{state::kEnd, 1},
         yytokentype::TK_EOL_COMMENT,
         {state::kNewlineComment, 0}},
        {{state::kEnd, 1}, yytokentype::TK_NEWLINE, {state::kRepeatNewline, 2}},
        {{state::kEnd, 0}, yytokentype::TK_module, {state::kStart, 0}},
};

// Test that the TransitionState properly transitions states.
TEST_F(TokenScannerTest, TransitionStartToEndTest) {
  for (const auto& test_case : kTransitionStateTests) {
    EXPECT_TRUE(test_case.expected_state ==
                TransitionState(test_case.input_state, test_case.transition))
        << "For test case with input: " << Printer(test_case.input_state)
        << " and transition: " << test_case.transition << std::endl;
  }
}

// Test that Update properly transitions the state machine
TEST_F(TokenScannerTest, UpdateTest) {
  const TokenScannerState initial_state;
  for (const auto& test_case : kTransitionStateTests) {
    current_state_ = test_case.input_state;
    UpdateState(test_case.transition);
    EXPECT_TRUE(current_state_ == test_case.expected_state)
        << "For test case with input: " << Printer(test_case.input_state)
        << " and transition: " << test_case.transition << std::endl;
    Reset();
    EXPECT_TRUE(current_state_ == initial_state);
  }
}

}  // namespace
}  // namespace formatter
}  // namespace verilog
