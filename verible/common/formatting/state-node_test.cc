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

#include "verible/common/formatting/state-node.h"

#include <cstddef>
#include <memory>
#include <sstream>
#include <stack>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "gtest/gtest.h"
#include "verible/common/formatting/basic-format-style.h"
#include "verible/common/formatting/format-token.h"
#include "verible/common/formatting/unwrapped-line-test-utils.h"
#include "verible/common/formatting/unwrapped-line.h"
#include "verible/common/text/token-info.h"
#include "verible/common/util/logging.h"

namespace verible {
namespace {

std::string RenderFormattedText(const StateNode &path,
                                const UnwrappedLine &uwline) {
  FormattedExcerpt formatted_line(uwline);
  // Discard tokens that have not yet been explored in search.
  formatted_line.MutableTokens().resize(path.Depth());
  path.ReconstructFormatDecisions(&formatted_line);
  return formatted_line.Render();
}

struct StateNodeTestFixture : public UnwrappedLineMemoryHandler,
                              public ::testing::Test {
  StateNodeTestFixture() {
    style.column_limit = 30;
    style.indentation_spaces = 2;
    style.wrap_spaces = 4;
    style.over_column_limit_penalty = 200;
  }

  void Initialize(int d, const std::vector<TokenInfo> &tokens) {
    CreateTokenInfos(tokens);
    uwline = std::make_unique<UnwrappedLine>(d * style.indentation_spaces,
                                             pre_format_tokens_.begin());
    AddFormatTokens(uwline.get());
  }

  void InitializeExternalTextBuffer(int d,
                                    const std::vector<TokenInfo> &tokens) {
    CreateTokenInfosExternalStringBuffer(tokens);
    uwline = std::make_unique<UnwrappedLine>(d * style.indentation_spaces,
                                             pre_format_tokens_.begin());
    AddFormatTokens(uwline.get());
  }

  std::string Render(const StateNode &path, const UnwrappedLine &uwline) const {
    return RenderFormattedText(path, uwline);
  }

  BasicFormatStyle style;
  std::unique_ptr<UnwrappedLine> uwline;
};

// Tests that root StateNode of search can be initialized with full
// sequence of FormatTokens.
TEST_F(StateNodeTestFixture, ConstructionWithEmptyFormatTokens) {
  static const int kInitialIndent = 3;
  const std::vector<TokenInfo> tokens;
  Initialize(kInitialIndent, tokens);  // empty tokens
  StateNode s(*uwline, style);
  EXPECT_TRUE(s.Done());  // because there is nothing to search
  EXPECT_EQ(s.current_column, kInitialIndent * style.indentation_spaces);
  EXPECT_EQ(s.wrap_column_positions.size(), 1);
  EXPECT_EQ(s.wrap_column_positions.top(),
            s.current_column + style.wrap_spaces);
  EXPECT_EQ(s.spacing_choice, SpacingDecision::kAppend);
  EXPECT_EQ(s.next(), nullptr);
  EXPECT_EQ(s.cumulative_cost, 0);
  EXPECT_TRUE(s.IsRootState());
}

// Tests that root StateNode of search can initialize to an array
// of FormatTokens.
TEST_F(StateNodeTestFixture, ConstructionWithOneFormatToken) {
  constexpr size_t kInitialIndent = 1;
  const std::vector<TokenInfo> tokens = {{0, "token1"}};
  Initialize(kInitialIndent, tokens);
  StateNode s(*uwline, style);
  EXPECT_TRUE(s.Done());  // nothing to do after first and only token
  EXPECT_EQ(s.current_column, kInitialIndent * style.indentation_spaces +
                                  tokens[0].text().length());
  EXPECT_EQ(s.wrap_column_positions.size(), 1);
  EXPECT_EQ(s.wrap_column_positions.top(),
            kInitialIndent * style.indentation_spaces + style.wrap_spaces);
  EXPECT_EQ(s.spacing_choice, SpacingDecision::kAppend);
  EXPECT_EQ(s.next(), nullptr);
  EXPECT_EQ(s.cumulative_cost, 0);
  EXPECT_TRUE(s.IsRootState());
}

// Tests that the first token can be marked as preserve, when disabling
// formatting.
TEST_F(StateNodeTestFixture, ConstructionWithPreserveLeadingSpace) {
  static const int kInitialIndent = 1;
  const std::vector<TokenInfo> tokens = {{0, "token1"}};
  Initialize(kInitialIndent, tokens);
  // One way of disabling formatting is setting break_decision to Preserve.
  pre_format_tokens_.front().before.break_decision = SpacingOptions::kPreserve;
  StateNode s(*uwline, style);
  EXPECT_TRUE(s.Done());  // nothing to do after first and only token
  EXPECT_EQ(s.current_column, tokens[0].text().length());
  EXPECT_EQ(s.spacing_choice, SpacingDecision::kPreserve);
  EXPECT_EQ(s.next(), nullptr);
  EXPECT_EQ(s.cumulative_cost, 0);
  EXPECT_TRUE(s.IsRootState());
}

// Tests new state can be built on top of previous state, appending token to
// current line.
TEST_F(StateNodeTestFixture, ConstructionAppendingPrevState) {
  const int kInitialIndent = 1;
  const std::vector<TokenInfo> tokens = {{0, "token1"}, {1, "TT2"}};
  Initialize(kInitialIndent, tokens);
  auto &ftokens = pre_format_tokens_;
  ftokens[0].before.spaces_required = 1;
  ftokens[1].before.spaces_required = 1;
  ftokens[1].before.break_penalty = 5;
  auto parent_state = std::make_shared<StateNode>(*uwline, style);
  const int initial_column = kInitialIndent * style.indentation_spaces;  // 2
  EXPECT_EQ(ABSL_DIE_IF_NULL(parent_state)->current_column,
            initial_column + tokens[0].text().length());
  EXPECT_EQ(parent_state->cumulative_cost, 0);
  EXPECT_EQ(parent_state->wrap_column_positions.size(), 1);
  EXPECT_EQ(parent_state->wrap_column_positions.top(),
            initial_column + style.wrap_spaces);
  EXPECT_TRUE(parent_state->IsRootState());

  const auto &child_state = parent_state;
  {
    // Second token, also appended to same line as first:
    auto child2_state = std::make_shared<StateNode>(child_state, style,
                                                    SpacingDecision::kAppend);
    EXPECT_EQ(child2_state->next(), child_state.get());
    EXPECT_EQ(child2_state->current_column,
              child_state->current_column +            // 8 +
                  ftokens[1].before.spaces_required +  // 1 +
                  tokens[1].text().length()            // 3: "TT2"
    );
    EXPECT_EQ(child2_state->cumulative_cost, 0);
    EXPECT_FALSE(child2_state->IsRootState());
    EXPECT_EQ(Render(*child2_state, *uwline), "  token1 TT2");
  }
  {
    // Second token, but wrapped onto next line:
    auto child2_state =
        std::make_shared<StateNode>(child_state, style, SpacingDecision::kWrap);
    EXPECT_EQ(child2_state->next(), child_state.get());
    EXPECT_EQ(child2_state->current_column,
              initial_column +               // 2 +
                  style.wrap_spaces +        // 4 +
                  tokens[1].text().length()  // 3: "TT2"
    );
    EXPECT_EQ(child2_state->cumulative_cost, ftokens[1].before.break_penalty);
    EXPECT_FALSE(child2_state->IsRootState());
    EXPECT_EQ(Render(*child2_state, *uwline),
              "  token1\n"
              "      TT2");
  }
}

// Tests that preserving spaces results in correct column position.
TEST_F(StateNodeTestFixture, ConstructionPreserveSpacesFromPrevStateNoGap) {
  const absl::string_view text("aaabbb");  // no gap between "aaa" and "bbb"
  const int kInitialIndent = 1;
  const std::vector<TokenInfo> tokens = {{0, text.substr(0, 3)},
                                         {1, text.substr(3, 3)}};
  InitializeExternalTextBuffer(kInitialIndent, tokens);
  auto &ftokens = pre_format_tokens_;
  ftokens[0].before.spaces_required = 1;
  ftokens[1].before.spaces_required = 4;  // ignored because of preserving
  ftokens[1].before.preserved_space_start = ftokens[0].Text().end();
  ftokens[1].before.break_penalty = 5;  // ignored because of preserving
  auto parent_state = std::make_shared<StateNode>(*uwline, style);
  const int initial_column = kInitialIndent * style.indentation_spaces;  // 2
  EXPECT_EQ(ABSL_DIE_IF_NULL(parent_state)->current_column,
            initial_column + tokens[0].text().length());  // 2 + 3
  EXPECT_EQ(parent_state->cumulative_cost, 0);
  EXPECT_TRUE(parent_state->IsRootState());

  // Appended with preserved spaces from original text.
  auto child_state = std::make_shared<StateNode>(parent_state, style,
                                                 SpacingDecision::kPreserve);
  EXPECT_EQ(child_state->next(), parent_state.get());
  EXPECT_EQ(child_state->current_column,
            parent_state->current_column +  // 5 +
                tokens[1].text().length()   // 3
  );
  EXPECT_EQ(child_state->cumulative_cost, parent_state->cumulative_cost);
  EXPECT_FALSE(child_state->IsRootState());
  EXPECT_EQ(Render(*child_state, *uwline), "  aaabbb");
}

// Tests that preserving spaces results in correct column position.
TEST_F(StateNodeTestFixture, ConstructionPreserveSpacesFromPrevStateSpaces) {
  const absl::string_view text(
      "aaa    bbb");  // 4 spaces between "aaa" and "bbb"
  const int kInitialIndent = 1;
  const std::vector<TokenInfo> tokens = {{1, text.substr(0, 3)},
                                         {2, text.substr(7, 3)}};
  InitializeExternalTextBuffer(kInitialIndent, tokens);
  auto &ftokens = pre_format_tokens_;
  ftokens[0].before.spaces_required = 1;
  ftokens[1].before.spaces_required = 2;  // ignored because of preserving
  ftokens[1].before.preserved_space_start = ftokens[0].Text().end();
  ftokens[1].before.break_penalty = 5;  // ignored because of preserving

  auto parent_state = std::make_shared<StateNode>(*uwline, style);
  const int initial_column = kInitialIndent * style.indentation_spaces;  // 2
  EXPECT_EQ(ABSL_DIE_IF_NULL(parent_state)->current_column,
            initial_column + tokens[0].text().length());  // 2 + 3
  EXPECT_EQ(parent_state->cumulative_cost, 0);
  EXPECT_TRUE(parent_state->IsRootState());

  // Appended with preserved spaces from original text.
  auto child_state = std::make_shared<StateNode>(parent_state, style,
                                                 SpacingDecision::kPreserve);
  EXPECT_EQ(child_state->next(), parent_state.get());
  EXPECT_EQ(child_state->current_column,
            parent_state->current_column +  // 5 +
                4 +                         // spaces
                tokens[1].text().length()   // 3
  );
  EXPECT_EQ(child_state->cumulative_cost, parent_state->cumulative_cost);
  EXPECT_FALSE(child_state->IsRootState());
  EXPECT_EQ(Render(*child_state, *uwline), "  aaa    bbb");
}

// Tests that preserving spaces results in correct column position.
TEST_F(StateNodeTestFixture, ConstructionPreserveSpacesFromPrevStateNewline) {
  const absl::string_view text("aaa  \n bbb");  // newline in between
  const int kInitialIndent = 1;
  const std::vector<TokenInfo> tokens = {{1, text.substr(0, 3)},
                                         {2, text.substr(7, 3)}};
  InitializeExternalTextBuffer(kInitialIndent, tokens);
  auto &ftokens = pre_format_tokens_;
  ftokens[0].before.spaces_required = 1;
  ftokens[1].before.spaces_required = 2;  // ignored because of preserving
  ftokens[1].before.preserved_space_start = ftokens[0].Text().end();
  ftokens[1].before.break_penalty = 5;  // ignored because of preserving

  auto parent_state = std::make_shared<StateNode>(*uwline, style);
  const int initial_column = kInitialIndent * style.indentation_spaces;  // 2
  EXPECT_EQ(ABSL_DIE_IF_NULL(parent_state)->current_column,
            initial_column + tokens[0].text().length());  // 2 + 3
  EXPECT_EQ(parent_state->cumulative_cost, 0);
  EXPECT_TRUE(parent_state->IsRootState());

  // Appended with preserved spaces from original text.
  auto child_state = std::make_shared<StateNode>(parent_state, style,
                                                 SpacingDecision::kPreserve);
  EXPECT_EQ(child_state->next(), parent_state.get());
  EXPECT_EQ(child_state->current_column,
            1 +                            // space after last newline
                tokens[1].text().length()  // 3
  );
  EXPECT_EQ(child_state->cumulative_cost, parent_state->cumulative_cost);
  EXPECT_FALSE(child_state->IsRootState());
  EXPECT_EQ(Render(*child_state, *uwline), "  aaa  \n bbb");
}

// Tests new state can be built on top of previous state, appending token to
// current line.
TEST_F(StateNodeTestFixture, ConstructionAppendingPrevStateWithGroupBalancing) {
  const int kInitialIndent = 1;
  const std::vector<TokenInfo> tokens = {
      {0, "function_caller"}, {1, "("}, {2, "11"}, {3, ")"}};
  Initialize(kInitialIndent, tokens);
  auto &ftokens = pre_format_tokens_;
  ftokens[0].before.spaces_required = 1;
  ftokens[1].before.spaces_required = 1;
  ftokens[1].before.break_penalty = 10;
  ftokens[1].balancing = verible::GroupBalancing::kOpen;
  ftokens[2].before.spaces_required = 1;
  ftokens[2].before.break_penalty = 2;
  ftokens[3].balancing = verible::GroupBalancing::kClose;
  ftokens[3].before.spaces_required = 1;
  ftokens[3].before.break_penalty = 3;
  auto parent_state = std::make_shared<StateNode>(*uwline, style);
  const int initial_column = kInitialIndent * style.indentation_spaces;
  EXPECT_EQ(ABSL_DIE_IF_NULL(parent_state)->current_column,
            initial_column + tokens[0].text().length());
  EXPECT_EQ(parent_state->cumulative_cost, 0);
  EXPECT_EQ(parent_state->wrap_column_positions.size(), 1);
  EXPECT_EQ(parent_state->wrap_column_positions.top(),
            initial_column + style.wrap_spaces);
  EXPECT_EQ(Render(*parent_state, *uwline), "  function_caller");

  const auto &child_state = parent_state;
  {
    // Second token, also appended to same line as first:
    // > function_caller (
    // >     ^-- next wrap should be here
    auto child2_state = std::make_shared<StateNode>(child_state, style,
                                                    SpacingDecision::kAppend);
    EXPECT_EQ(child2_state->next(), child_state.get());
    EXPECT_EQ(child2_state->current_column,
              child_state->current_column +            // 17 +
                  ftokens[1].before.spaces_required +  // 1 +
                  tokens[1].text().length()            // 1: "("
    );
    EXPECT_EQ(child2_state->cumulative_cost, 0);
    EXPECT_EQ(child2_state->wrap_column_positions.size(), 1);
    EXPECT_EQ(child2_state->wrap_column_positions.top(),
              initial_column + style.wrap_spaces);
    EXPECT_EQ(Render(*child2_state, *uwline), "  function_caller (");
    {
      // Third token, also appended to same line:
      // > function_caller ( 11
      // >                  ^-- next wrap should be here
      auto child3_state = std::make_shared<StateNode>(child2_state, style,
                                                      SpacingDecision::kAppend);
      EXPECT_EQ(child3_state->next(), child2_state.get());
      EXPECT_EQ(child3_state->current_column,
                child2_state->current_column +           // 19 +
                    ftokens[2].before.spaces_required +  // 1 +
                    tokens[2].text().length()            // 2: "11"
      );
      EXPECT_EQ(child3_state->cumulative_cost, 0);
      EXPECT_EQ(child3_state->wrap_column_positions.size(), 2);
      EXPECT_EQ(child3_state->wrap_column_positions.top(),
                child2_state->current_column);
      EXPECT_EQ(Render(*child3_state, *uwline), "  function_caller ( 11");
      {
        // Fourth token, also appended to same line:
        // > function_caller ( 11 )
        // >     ^-- next wrap should be here, after closing balance group
        auto child4_state = std::make_shared<StateNode>(
            child3_state, style, SpacingDecision::kAppend);
        EXPECT_EQ(child4_state->next(), child3_state.get());
        EXPECT_EQ(child4_state->current_column,
                  child3_state->current_column +           // 22 +
                      ftokens[3].before.spaces_required +  // 1 +
                      tokens[3].text().length()            // 1: ")"
        );
        EXPECT_EQ(child4_state->cumulative_cost, 0);
        EXPECT_EQ(child4_state->wrap_column_positions.size(), 1);
        EXPECT_EQ(child4_state->wrap_column_positions.top(),
                  initial_column + style.wrap_spaces);
        EXPECT_EQ(Render(*child4_state, *uwline), "  function_caller ( 11 )");
      }
      {
        // Fourth token, wrapped onto next line:
        // > function_caller ( 11
        // >     )
        // >     ^-- next wrap should be here, after closing balance group
        // TODO(fangism): should this be aligned with corresponding open-group?
        // >                 )  // aligned with open-group
        // As-is, it is not because we pop the column stack on close-group
        // first, which is not an unreasonable choice.
        auto child4_state = std::make_shared<StateNode>(child3_state, style,
                                                        SpacingDecision::kWrap);
        EXPECT_EQ(child4_state->next(), child3_state.get());
        EXPECT_EQ(child4_state->current_column,
                  child2_state->wrap_column_positions
                          .top() +  // not a typo: child2_state
                      tokens[3].text().length());
        EXPECT_EQ(child4_state->cumulative_cost,
                  ftokens[3].before.break_penalty);
        EXPECT_EQ(child4_state->wrap_column_positions.size(), 1);
        EXPECT_EQ(child4_state->wrap_column_positions.top(),
                  initial_column + style.wrap_spaces);
        EXPECT_EQ(Render(*child4_state, *uwline),
                  "  function_caller ( 11\n"
                  "      )");
      }
    }
    {
      // Third token, wrapped onto next line:
      // > function_caller (
      // >     11
      // >         ^-- next wrap should be here
      auto child3_state = std::make_shared<StateNode>(child2_state, style,
                                                      SpacingDecision::kWrap);
      EXPECT_EQ(child3_state->next(), child2_state.get());
      EXPECT_EQ(child3_state->current_column,
                initial_column + style.wrap_spaces + tokens[2].text().length());
      EXPECT_EQ(child3_state->cumulative_cost, ftokens[2].before.break_penalty);
      EXPECT_EQ(child3_state->wrap_column_positions.size(), 2);
      EXPECT_EQ(child3_state->wrap_column_positions.top(),
                initial_column + style.wrap_spaces * 2);
      EXPECT_EQ(Render(*child3_state, *uwline),
                "  function_caller (\n"
                "      11");
      {
        // Fourth token, appended onto same line:
        // > function_caller (
        // >     11 )
        // >     ^-- next wrap should be here
        auto child4_state = std::make_shared<StateNode>(
            child3_state, style, SpacingDecision::kAppend);
        EXPECT_EQ(child4_state->next(), child3_state.get());
        EXPECT_EQ(child4_state->current_column,
                  child3_state->current_column +           // 8
                      ftokens[3].before.spaces_required +  // 1
                      tokens[3].text().length()            // 1: ")"
        );
        EXPECT_EQ(child4_state->cumulative_cost, child3_state->cumulative_cost);
        EXPECT_EQ(child4_state->wrap_column_positions.size(), 1);
        EXPECT_EQ(child4_state->wrap_column_positions.top(),
                  initial_column + style.wrap_spaces);
        EXPECT_EQ(Render(*child4_state, *uwline),
                  "  function_caller (\n"
                  "      11 )");
      }
      {
        // Fourth token, wrapped onto next line:
        // > function_caller (
        // >     11
        // >     )
        // >     ^-- next wrap should be here
        auto child4_state = std::make_shared<StateNode>(child3_state, style,
                                                        SpacingDecision::kWrap);
        EXPECT_EQ(child4_state->next(), child3_state.get());
        EXPECT_EQ(
            child4_state->current_column,
            initial_column + style.wrap_spaces + tokens[3].text().length());
        EXPECT_EQ(
            child4_state->cumulative_cost,
            child3_state->cumulative_cost + ftokens[3].before.break_penalty);
        EXPECT_EQ(child4_state->wrap_column_positions.size(), 1);
        EXPECT_EQ(child4_state->wrap_column_positions.top(),
                  initial_column + style.wrap_spaces);
        EXPECT_EQ(Render(*child4_state, *uwline),
                  "  function_caller (\n"
                  "      11\n"
                  "      )");
      }
    }
  }
  {
    // Second token, but wrapped onto next line:
    // > function_caller
    // >     (
    // >     ^-- next wrap should be here
    auto child2_state =
        std::make_shared<StateNode>(child_state, style, SpacingDecision::kWrap);
    EXPECT_EQ(child2_state->next(), child_state.get());
    EXPECT_EQ(child2_state->current_column,
              initial_column +               // 2 +
                  style.wrap_spaces +        // 4 +
                  tokens[1].text().length()  // 1: "("
    );
    EXPECT_EQ(child2_state->cumulative_cost, ftokens[1].before.break_penalty);
    // wrap_column_positions stack is pushed *after* seeing the next token.
    EXPECT_EQ(child2_state->wrap_column_positions.size(), 1);
    EXPECT_EQ(child2_state->wrap_column_positions.top(),
              initial_column + style.wrap_spaces);
    EXPECT_EQ(Render(*child2_state, *uwline),
              "  function_caller\n"
              "      (");
    {
      // Third token, appended to same line:
      // > function_caller
      // >     ( 11
      // >     ^-- next wrap should be here
      auto child3_state = std::make_shared<StateNode>(child2_state, style,
                                                      SpacingDecision::kAppend);
      EXPECT_EQ(child3_state->next(), child2_state.get());
      EXPECT_EQ(child3_state->current_column,
                child2_state->current_column +           // 7
                    ftokens[2].before.spaces_required +  // 1
                    tokens[2].text().length()            // 2: "11"
      );
      EXPECT_EQ(child3_state->cumulative_cost, child2_state->cumulative_cost);
      EXPECT_EQ(child3_state->wrap_column_positions.size(), 2);
      EXPECT_EQ(child3_state->wrap_column_positions.top(),
                initial_column + style.wrap_spaces + tokens[1].text().length());
      EXPECT_EQ(Render(*child3_state, *uwline),
                "  function_caller\n"
                "      ( 11");
      {
        // Fourth token, appended to same line:
        // > function_caller
        // >     ( 11 )
        // >     ^-- next wrap should be here
        auto child4_state = std::make_shared<StateNode>(
            child3_state, style, SpacingDecision::kAppend);
        EXPECT_EQ(child4_state->next(), child3_state.get());
        EXPECT_EQ(child4_state->current_column,
                  child3_state->current_column +           // 10
                      ftokens[3].before.spaces_required +  // 1
                      tokens[3].text().length()            // 1: ")"
        );
        EXPECT_EQ(child4_state->cumulative_cost, child3_state->cumulative_cost);
        EXPECT_EQ(child4_state->wrap_column_positions.size(), 1);
        EXPECT_EQ(child4_state->wrap_column_positions.top(),
                  initial_column + style.wrap_spaces);
        EXPECT_EQ(Render(*child4_state, *uwline),
                  "  function_caller\n"
                  "      ( 11 )");
      }
      {
        // Fourth token, wrapped onto next line:
        // > function_caller
        // >     ( 11
        // >     )
        // >     ^-- next wrap should be here
        auto child4_state = std::make_shared<StateNode>(child3_state, style,
                                                        SpacingDecision::kWrap);
        EXPECT_EQ(child4_state->next(), child3_state.get());
        EXPECT_EQ(child4_state->current_column,
                  child2_state->wrap_column_positions.top() +
                      tokens[3].text().length()  // 1: ")"
        );
        EXPECT_EQ(
            child4_state->cumulative_cost,
            child3_state->cumulative_cost + ftokens[3].before.break_penalty);
        EXPECT_EQ(child4_state->wrap_column_positions.size(), 1);
        EXPECT_EQ(child4_state->wrap_column_positions.top(),
                  initial_column + style.wrap_spaces);
        EXPECT_EQ(Render(*child4_state, *uwline),
                  "  function_caller\n"
                  "      ( 11\n"
                  "      )");
      }
    }
    {
      // Third token, wrapped onto next line:
      // > function_caller
      // >     (
      // >         11
      // >         ^-- next wrap should be here
      auto child3_state = std::make_shared<StateNode>(child2_state, style,
                                                      SpacingDecision::kWrap);
      EXPECT_EQ(child3_state->next(), child2_state.get());
      EXPECT_EQ(child3_state->current_column,
                initial_column + (style.wrap_spaces * 2) +  // 10
                    tokens[2].text().length()               // 2: "11"
      );
      EXPECT_EQ(
          child3_state->cumulative_cost,
          child2_state->cumulative_cost + ftokens[2].before.break_penalty);
      EXPECT_EQ(child3_state->wrap_column_positions.size(), 2);
      EXPECT_EQ(child3_state->wrap_column_positions.top(),
                initial_column + (style.wrap_spaces * 2));
      EXPECT_EQ(Render(*child3_state, *uwline),
                "  function_caller\n"
                "      (\n"
                "          11");
      {
        // Fourth token, appended to same line:
        // > function_caller
        // >     (
        // >         11 )
        // >     ^-- next wrap should be here
        auto child4_state = std::make_shared<StateNode>(
            child3_state, style, SpacingDecision::kAppend);
        EXPECT_EQ(child4_state->next(), child3_state.get());
        EXPECT_EQ(child4_state->current_column,
                  child3_state->current_column +           // 10
                      ftokens[3].before.spaces_required +  // 1
                      tokens[3].text().length()            // 1: ")"
        );
        EXPECT_EQ(child4_state->cumulative_cost, child3_state->cumulative_cost);
        EXPECT_EQ(child4_state->wrap_column_positions.size(), 1);
        EXPECT_EQ(child4_state->wrap_column_positions.top(),
                  initial_column + style.wrap_spaces);
        EXPECT_EQ(Render(*child4_state, *uwline),
                  "  function_caller\n"
                  "      (\n"
                  "          11 )");
      }
      {
        // Fourth token, wrapped onto next line:
        // > function_caller
        // >     (
        // >         11
        // >     )
        // >     ^-- next wrap should be here
        auto child4_state = std::make_shared<StateNode>(child3_state, style,
                                                        SpacingDecision::kWrap);
        EXPECT_EQ(child4_state->next(), child3_state.get());
        EXPECT_EQ(child4_state->current_column,
                  child_state->wrap_column_positions.top() +
                      tokens[3].text().length()  // 1: ")"
        );
        EXPECT_EQ(
            child4_state->cumulative_cost,
            child3_state->cumulative_cost + ftokens[3].before.break_penalty);
        EXPECT_EQ(child4_state->wrap_column_positions.size(), 1);
        EXPECT_EQ(child4_state->wrap_column_positions.top(),
                  initial_column + style.wrap_spaces);
        EXPECT_EQ(Render(*child4_state, *uwline),
                  "  function_caller\n"
                  "      (\n"
                  "          11\n"
                  "      )");
      }
    }
  }
}

// Tests new state can be built on top of previous state, appending token to
// current line, where second token exceeds line length.
TEST_F(StateNodeTestFixture, ConstructionAppendingPrevStateOverflow) {
  const int kInitialIndent = 1;
  const std::vector<TokenInfo> tokens = {{0, "aaaaaaaaaaaaaaaaaaaaa"},
                                         {1, "bbbbbbbbbbbbbbbbbb"}};
  Initialize(kInitialIndent, tokens);
  auto &ftokens = pre_format_tokens_;
  ftokens[0].before.spaces_required = 1;
  ftokens[1].before.spaces_required = 1;
  ftokens[1].before.break_penalty = 8;

  // First token on line:
  auto parent_state = std::make_shared<StateNode>(*uwline, style);
  const int initial_column = kInitialIndent * style.indentation_spaces;
  EXPECT_EQ(ABSL_DIE_IF_NULL(parent_state)->current_column,
            initial_column + tokens[0].text().length());
  EXPECT_EQ(parent_state->cumulative_cost, 0);

  auto child_state = parent_state;

  {
    // Second token, also appended to same line as first:
    auto child2_state = std::make_shared<StateNode>(child_state, style,
                                                    SpacingDecision::kAppend);
    EXPECT_EQ(child2_state->next(), child_state.get());
    EXPECT_EQ(child2_state->current_column,
              child_state->current_column +            // 8 +
                  ftokens[1].before.spaces_required +  // 1 +
                  tokens[1].text().length()            // 3: "TT2"
    );
    EXPECT_EQ(child2_state->cumulative_cost, style.over_column_limit_penalty +
                                                 child2_state->current_column -
                                                 style.column_limit);
  }
  {
    // Second token, but wrapped onto a new line:
    auto child2_state =
        std::make_shared<StateNode>(child_state, style, SpacingDecision::kWrap);
    EXPECT_EQ(child2_state->next(), child_state.get());
    EXPECT_EQ(child2_state->current_column,
              initial_column +         // 2 +
                  style.wrap_spaces +  // 4 +
                  tokens[1].text().length());
    EXPECT_EQ(child2_state->cumulative_cost, ftokens[1].before.break_penalty);
  }
}

// Tests that column positions account for multiline tokens.
TEST_F(StateNodeTestFixture, MultiLineTokenFront) {
  const int kInitialIndent = 1;
  const std::vector<TokenInfo> tokens = {{0, "a23456789\nb234"}};
  Initialize(kInitialIndent, tokens);
  auto &ftokens = pre_format_tokens_;
  ftokens[0].before.spaces_required = 1;

  // First token on line:
  auto parent_state = std::make_shared<StateNode>(*uwline, style);
  EXPECT_EQ(ABSL_DIE_IF_NULL(parent_state)->current_column,
            4 /* length("b234") */);
  EXPECT_EQ(parent_state->cumulative_cost, 0);
  EXPECT_EQ(RenderFormattedText(*parent_state, *uwline), "  a23456789\nb234");
}

// Tests that newly calculated column positions account for multiline tokens.
TEST_F(StateNodeTestFixture, MultiLineToken) {
  const int kInitialIndent = 1;
  const std::vector<TokenInfo> tokens = {{0, "a23456789012345678901"},
                                         {1, "b2345\nc234567890123"}};
  Initialize(kInitialIndent, tokens);
  auto &ftokens = pre_format_tokens_;
  ftokens[0].before.spaces_required = 1;
  ftokens[1].before.spaces_required = 1;
  ftokens[1].before.break_penalty = 8;

  // First token on line:
  auto parent_state = std::make_shared<StateNode>(*uwline, style);
  const int initial_column = kInitialIndent * style.indentation_spaces;
  EXPECT_EQ(ABSL_DIE_IF_NULL(parent_state)->current_column,
            initial_column + tokens[0].text().length());
  EXPECT_EQ(parent_state->cumulative_cost, 0);

  {
    // Second token, also appended to same line as first:
    auto child_state = std::make_shared<StateNode>(parent_state, style,
                                                   SpacingDecision::kAppend);
    EXPECT_EQ(child_state->next(), parent_state.get());
    EXPECT_EQ(child_state->current_column,
              13  // length("c2345...."), no wrapping indentation
    );
    // no over-column-limit penalty
    EXPECT_EQ(child_state->cumulative_cost, 0);
    EXPECT_EQ(RenderFormattedText(*child_state, *uwline),
              "  a23456789012345678901 b2345\nc234567890123");
  }
  {
    // Second token, but wrapped onto a new line:
    auto child_state = std::make_shared<StateNode>(parent_state, style,
                                                   SpacingDecision::kWrap);
    EXPECT_EQ(child_state->next(), parent_state.get());
    EXPECT_EQ(child_state->current_column,
              13  // length("c2345...."), no wrapping indentation
    );
    // no over-column-limit penalty
    EXPECT_EQ(child_state->cumulative_cost, ftokens[1].before.break_penalty);
    EXPECT_EQ(RenderFormattedText(*child_state, *uwline),
              // indent level 1 + wrapping indentation
              "  a23456789012345678901\n      b2345\nc234567890123");
  }
}

// Tests that multiline tokens are penalized based on overflow before first \n.
TEST_F(StateNodeTestFixture, MultiLineTokenOverflow) {
  const int kInitialIndent = 1;
  const std::vector<TokenInfo> tokens = {{0, "a23456789012345678901"},
                                         {1, "b234567\nc234567890"}};
  Initialize(kInitialIndent, tokens);
  auto &ftokens = pre_format_tokens_;
  ftokens[0].before.spaces_required = 1;
  ftokens[1].before.spaces_required = 1;
  ftokens[1].before.break_penalty = 8;

  // First token on line:
  auto parent_state = std::make_shared<StateNode>(*uwline, style);
  const int initial_column = kInitialIndent * style.indentation_spaces;
  EXPECT_EQ(ABSL_DIE_IF_NULL(parent_state)->current_column,
            initial_column + tokens[0].text().length());
  EXPECT_EQ(parent_state->cumulative_cost, 0);

  {
    // Second token, also appended to same line as first:
    auto child_state = std::make_shared<StateNode>(parent_state, style,
                                                   SpacingDecision::kAppend);
    EXPECT_EQ(child_state->next(), parent_state.get());
    EXPECT_EQ(child_state->current_column,
              10  // length("c2345...."), no wrapping indentation
    );
    // only over by 1
    EXPECT_EQ(child_state->cumulative_cost,
              style.over_column_limit_penalty + parent_state->current_column +
                  ftokens[1].before.spaces_required +
                  7 /* length("b23...7") */ - style.column_limit);
    EXPECT_EQ(RenderFormattedText(*child_state, *uwline),
              "  a23456789012345678901 b234567\nc234567890");
  }
  {
    // Second token, but wrapped onto a new line:
    auto child_state = std::make_shared<StateNode>(parent_state, style,
                                                   SpacingDecision::kWrap);
    EXPECT_EQ(child_state->next(), parent_state.get());
    EXPECT_EQ(child_state->current_column,
              10  // length("c2345...."), no wrapping indentation
    );
    // no over-column-limit penalty
    EXPECT_EQ(child_state->cumulative_cost, ftokens[1].before.break_penalty);
    EXPECT_EQ(RenderFormattedText(*child_state, *uwline),
              // indent level 1 + wrapping indentation
              "  a23456789012345678901\n      b234567\nc234567890");
  }
}

// Tests new state can be built on top of previous state, wrapping token to
// new line.
TEST_F(StateNodeTestFixture, ConstructionWrappingLinePrevState) {
  const int kInitialIndent = 1;
  const std::vector<TokenInfo> tokens = {{0, "token1"}, {1, "token2"}};
  Initialize(kInitialIndent, tokens);
  auto &ftokens = pre_format_tokens_;
  ftokens[1].before.break_penalty = 7;
  auto parent_state = std::make_shared<StateNode>(*uwline, style);
  const int initial_column = kInitialIndent * style.indentation_spaces;
  EXPECT_EQ(ABSL_DIE_IF_NULL(parent_state)->current_column,
            initial_column + tokens[0].text().length());
  EXPECT_EQ(parent_state->cumulative_cost, 0);

  // Wrap the next token onto a new line.
  auto child_state =
      std::make_shared<StateNode>(parent_state, style, SpacingDecision::kWrap);
  EXPECT_EQ(child_state->next(), parent_state.get());
  EXPECT_EQ(child_state->current_column,
            initial_column + style.wrap_spaces + tokens[1].text().length());
  EXPECT_EQ(child_state->cumulative_cost, ftokens[1].before.break_penalty);
  EXPECT_EQ(Render(*child_state, *uwline),
            "  token1\n"
            "      token2");
}

// Tests that the default next-state appends when a token fits (column limit),
// and wraps when it doesn't fit.
TEST_F(StateNodeTestFixture, AppendIfItFitsTryToAppend) {
  const int kInitialIndent = 1;
  // Tokens stay under column limit.
  const std::vector<TokenInfo> tokens = {
      {0, "xxxxxXXXXX"},
      {1, "yyyyyYYYYY"},
      {2, "zzzzzZZZZZ"},
  };
  Initialize(kInitialIndent, tokens);
  auto &ftokens = pre_format_tokens_;
  ftokens[0].before.spaces_required = 1;
  ftokens[1].before.spaces_required = 1;
  ftokens[2].before.spaces_required = 1;
  auto parent_state = std::make_shared<StateNode>(*uwline, style);
  const int initial_column = kInitialIndent * style.indentation_spaces;  // 2
  EXPECT_EQ(ABSL_DIE_IF_NULL(parent_state)->current_column,
            initial_column + tokens[0].text().length());
  EXPECT_EQ(parent_state->wrap_column_positions.size(), 1);
  EXPECT_EQ(parent_state->wrap_column_positions.top(),
            initial_column + style.wrap_spaces);
  EXPECT_TRUE(parent_state->IsRootState());

  // Second token, also appended to same line as first:
  auto child_state = StateNode::AppendIfItFits(parent_state, style);
  EXPECT_EQ(child_state->spacing_choice, SpacingDecision::kAppend);
  EXPECT_EQ(child_state->next(), parent_state.get());
  EXPECT_EQ(child_state->current_column,
            parent_state->current_column +           // 12 +
                ftokens[1].before.spaces_required +  // 1 +
                tokens[1].text().length()            // 10 = 23 (< 30)
  );
  EXPECT_FALSE(child_state->IsRootState());

  // Third token, doesn't fit, and will be wrapped.
  auto child2_state = StateNode::AppendIfItFits(child_state, style);
  EXPECT_EQ(child2_state->spacing_choice, SpacingDecision::kWrap);
  EXPECT_EQ(child2_state->next(), child_state.get());
  EXPECT_EQ(child2_state->current_column,
            initial_column + style.wrap_spaces + tokens[2].text().length());
}

// Tests that the default next-state respects forced line breaks.
TEST_F(StateNodeTestFixture, AppendIfItFitsForcedWrap) {
  const int kInitialIndent = 1;
  const std::vector<TokenInfo> tokens = {
      {0, "xxxxxXXXXX"},
      {1, "yyyyyYYYYY"},
  };
  Initialize(kInitialIndent, tokens);
  auto &ftokens = pre_format_tokens_;
  ftokens[0].before.spaces_required = 1;
  ftokens[1].before.spaces_required = 1;
  // Tokens stay under column limit, but here, we force a wrap.
  ftokens[1].before.break_decision = SpacingOptions::kMustWrap;
  auto parent_state = std::make_shared<StateNode>(*uwline, style);
  const int initial_column = kInitialIndent * style.indentation_spaces;  // 2
  EXPECT_EQ(ABSL_DIE_IF_NULL(parent_state)->current_column,
            initial_column + tokens[0].text().length());
  EXPECT_EQ(parent_state->wrap_column_positions.size(), 1);
  EXPECT_EQ(parent_state->wrap_column_positions.top(),
            initial_column + style.wrap_spaces);
  EXPECT_TRUE(parent_state->IsRootState());

  // Second token, forced to wrap onto new line.
  auto child_state = StateNode::AppendIfItFits(parent_state, style);
  EXPECT_EQ(child_state->spacing_choice, SpacingDecision::kWrap);
  EXPECT_EQ(child_state->next(), parent_state.get());
  EXPECT_EQ(child_state->current_column,
            initial_column + style.wrap_spaces + tokens[0].text().length());
  EXPECT_FALSE(child_state->IsRootState());
}

// Tests that QuickFinish repeatedly applies AppendIfItFits.
TEST_F(StateNodeTestFixture, QuickFinish) {
  const int kInitialIndent = 1;
  // Tokens stay under column limit.
  const std::vector<TokenInfo> tokens = {
      {0, "xxxxxXXXXX"},
      {1, "yyyyyYYYYY"},
      {2, "zzzzzZZZZZ"},
  };
  Initialize(kInitialIndent, tokens);
  auto &ftokens = pre_format_tokens_;
  ftokens[0].before.spaces_required = 1;
  ftokens[1].before.spaces_required = 1;
  ftokens[2].before.spaces_required = 1;
  auto parent_state = std::make_shared<StateNode>(*uwline, style);
  const int initial_column = kInitialIndent * style.indentation_spaces;  // 2
  EXPECT_EQ(ABSL_DIE_IF_NULL(parent_state)->current_column,
            initial_column + tokens[0].text().length());
  EXPECT_EQ(parent_state->wrap_column_positions.size(), 1);
  EXPECT_EQ(parent_state->wrap_column_positions.top(),
            initial_column + style.wrap_spaces);
  EXPECT_TRUE(parent_state->IsRootState());

  auto final_state = StateNode::QuickFinish(parent_state, style);

  // Checking up the ancestry chain of previous states
  // Third token, doesn't fit, and will be wrapped.
  EXPECT_EQ(final_state->spacing_choice, SpacingDecision::kWrap);

  // Second token, also appended to same line as first:
  const auto *child_state = final_state->next();
  EXPECT_EQ(child_state->spacing_choice, SpacingDecision::kAppend);

  // Second state is decended from initial state.
  EXPECT_EQ(child_state->next(), parent_state.get());
}

// Tests that equal cumulative penalty does not count as less.
TEST_F(StateNodeTestFixture, OperatorLessSelf) {
  const std::vector<TokenInfo> tokens;
  Initialize(0, tokens);
  StateNode s(*uwline, style);
  EXPECT_FALSE(s < s);
}

// Tests for inequality of cumulative penalties.
TEST_F(StateNodeTestFixture, OperatorLessUnequal) {
  const std::vector<TokenInfo> tokens;
  Initialize(0, tokens);
  StateNode s(*uwline, style);
  s.cumulative_cost = 3;
  StateNode t(*uwline, style);
  t.cumulative_cost = 4;
  EXPECT_TRUE(s < t);
  EXPECT_FALSE(t < s);
}

// Test that string representation is as expected.
TEST_F(StateNodeTestFixture, Stringify) {
  const std::vector<TokenInfo> tokens;
  Initialize(0, tokens);
  StateNode::path_type path;
  StateNode s(*uwline, style);
  s.spacing_choice = SpacingDecision::kWrap;
  s.current_column = 7;
  s.cumulative_cost = 11;
  s.wrap_column_positions.top() = 3;
  std::ostringstream stream;
  stream << s;
  EXPECT_EQ(stream.str(), "spacing:wrap, col@7, cost=11, [...3]");
}

}  // namespace
}  // namespace verible
