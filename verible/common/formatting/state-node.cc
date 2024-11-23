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
#include <ostream>
#include <stack>
#include <vector>

#include "absl/strings/string_view.h"
#include "verible/common/formatting/basic-format-style.h"
#include "verible/common/formatting/format-token.h"
#include "verible/common/formatting/unwrapped-line.h"
#include "verible/common/strings/position.h"
#include "verible/common/text/token-info.h"
#include "verible/common/util/iterator-adaptors.h"
#include "verible/common/util/iterator-range.h"
#include "verible/common/util/logging.h"

namespace verible {

static constexpr absl::string_view kNotForAlignment =
    "Aligned tokens should never use line-wrap optimization!";

static SpacingDecision FrontTokenSpacing(const FormatTokenRange range) {
  if (range.empty()) return SpacingDecision::kAppend;
  const SpacingOptions opt = range.front().before.break_decision;
  // Treat first token as appended, unless explicitly preserving spaces.
  switch (opt) {
    case SpacingOptions::kPreserve:
      return SpacingDecision::kPreserve;
    case SpacingOptions::kAppendAligned:
      LOG(FATAL) << kNotForAlignment;
      break;
    default:
      break;
  }
  return SpacingDecision::kAppend;
}

StateNode::StateNode(const UnwrappedLine &uwline, const BasicFormatStyle &style)
    : prev_state(nullptr),
      undecided_path(uwline.TokensRange().begin(), uwline.TokensRange().end()),
      spacing_choice(FrontTokenSpacing(uwline.TokensRange())),
      // Kludge: This leaks into the resulting FormattedExcerpt, which means
      // additional logic is needed to handle preservation of (vertical) spacing
      // between formatted token partitions.
      current_column(uwline.IndentationSpaces()) {
  // The starting column is relative to the current indentation level.
  VLOG(4) << "initial column position: " << current_column;
  wrap_column_positions.push(current_column + style.wrap_spaces);
  if (!uwline.TokensRange().empty()) {
    VLOG(4) << "token.text: \'" << undecided_path.front().token->text() << '\'';
    // Point undecided_path past the first token.
    undecided_path.pop_front();
    // Place first token on unwrapped line.
    UpdateColumnPosition();
    CHECK_EQ(cumulative_cost, 0);
    OpenGroupBalance(style);
  }
  VLOG(4) << "root: " << *this;
}

StateNode::StateNode(const std::shared_ptr<const StateNode> &parent,
                     const BasicFormatStyle &style,
                     SpacingDecision spacing_choice)
    : prev_state(ABSL_DIE_IF_NULL(parent)),
      undecided_path(prev_state->undecided_path.begin() + 1,  // pop_front()
                     prev_state->undecided_path.end()),
      spacing_choice(spacing_choice),
      // current_column to be computed, depending on spacing_choice
      cumulative_cost(prev_state->cumulative_cost),  // will be adjusted below
      wrap_column_positions(prev_state->wrap_column_positions) {
  CHECK(!prev_state->Done());

  const PreFormatToken &current_format_token(GetCurrentToken());
  VLOG(4) << "token.text: \'" << current_format_token.token->text() << '\'';

  bool called_open_group_balance = false;
  bool called_close_group_balance = false;
  if (spacing_choice == SpacingDecision::kWrap) {
    // When wrapping and closing a balance group, adjust wrap column stack
    // first.
    if (current_format_token.balancing == GroupBalancing::kClose) {
      CloseGroupBalance();
      called_close_group_balance = true;
    }
    // When wrapping after opening a balance group, adjust wrap column stack
    // first.
    if (prev_state->spacing_choice == SpacingDecision::kWrap) {
      OpenGroupBalance(style);
      called_open_group_balance = true;
    }
  }

  // Update column position and add penalty to the cumulative cost.
  const int column_for_penalty = UpdateColumnPosition();
  UpdateCumulativeCost(style, column_for_penalty);

  // Adjusting for open-group is done after updating current column position,
  // and is based on the *previous* open-group token, and the
  // spacing_choice for *this* token.
  if (!called_open_group_balance) {
    OpenGroupBalance(style);
  }

  // When appending and closing a balance group, adjust wrap column stack last.
  if (!called_close_group_balance &&
      (current_format_token.balancing == GroupBalancing::kClose)) {
    CloseGroupBalance();
  }

  VLOG(4) << "new state_node: " << *this;
}

const PreFormatToken &StateNode::GetPreviousToken() const {
  CHECK(!ABSL_DIE_IF_NULL(prev_state)->Done());
  return prev_state->GetCurrentToken();
}

// Returns the effective column position that should be used for determining
// penalty for going over the column limit.  This could be different from
// current_column for multi-line tokens.
int StateNode::UpdateColumnPosition() {
  VLOG(4) << __FUNCTION__ << " spacing decision: " << spacing_choice;
  const PreFormatToken &current_format_token(GetCurrentToken());
  const int token_length = current_format_token.Length();

  {
    // Special handling for multi-line tokens.
    // Account for the length of text *before* the first newline that might
    // overflow the previous line (and should be penalized accordingly).
    const auto text = current_format_token.Text();
    const auto last_newline_pos = text.find_last_of('\n');
    if (last_newline_pos != absl::string_view::npos) {
      // There was a newline, it doesn't matter what the wrapping decision was.
      // The position is the length of the text after the last newline.
      current_column = text.length() - last_newline_pos - 1;
      const auto first_newline_pos = text.find_first_of('\n');
      if (spacing_choice == SpacingDecision::kWrap) {
        // Record the number of spaces preceding this format token because
        // it cannot be simply inferred based on current column and
        // raw text length.
        wrap_multiline_token_spaces_before = wrap_column_positions.top();
        return current_column;
      }
      // Penalize based on the column position that resulted in appending
      // text up to the first newline.
      if (IsRootState()) {
        return first_newline_pos;
      }
      return prev_state->current_column +
             current_format_token.before.spaces_required + first_newline_pos;
    }
  }

  switch (spacing_choice) {
    case SpacingDecision::kAlign:
      LOG(FATAL) << kNotForAlignment;
      break;
    case SpacingDecision::kWrap:
      // If wrapping, new column position is based on the wrap_column_positions
      // top-of-stack.
      current_column = wrap_column_positions.top() + token_length;
      VLOG(4) << "current wrap_position = " << wrap_column_positions.top();
      VLOG(4) << "wrapping, current_column is now " << current_column;
      break;
    case SpacingDecision::kAppend:
      // If appending, new column position is added to previous state's column
      // position.
      if (!IsRootState()) {
        VLOG(4) << " previous column position: " << prev_state->current_column;
        current_column = prev_state->current_column +
                         current_format_token.before.spaces_required +
                         token_length;
      } else {
        VLOG(4) << " old column position: " << current_column;
        // current_column was already initialized, so just add token length.
        current_column += token_length;
      }
      break;
    case SpacingDecision::kPreserve: {
      const absl::string_view original_spacing_text =
          current_format_token.OriginalLeadingSpaces();
      // prev_state is null when the first token of the unwrapped line was
      // marked as SpacingOptions::Preserve, which indicates that formatting
      // was disabled in this range.  In this case, we don't really care about
      // column position accuracy since we are using original spacing.
      current_column =
          prev_state ? AdvancingTextNewColumnPosition(
                           prev_state->current_column, original_spacing_text)
                     : 0;
      current_column += token_length;
      VLOG(4) << " new column position (preserved): " << current_column;
      break;
    }
  }
  return current_column;
}

void StateNode::UpdateCumulativeCost(const BasicFormatStyle &style,
                                     int column_for_penalty) {
  // This must be called after UpdateColumnPosition() to account for
  // the updated current_column.
  // column_for_penalty can be different than current_column in the
  // case of multi-line tokens.
  // Penalize based on column for penalty.
  if (!IsRootState()) {
    CHECK_EQ(cumulative_cost, prev_state->cumulative_cost);
  }
  const PreFormatToken &current_format_token(GetCurrentToken());
  if (spacing_choice == SpacingDecision::kWrap) {
    // Only incur the penalty for breaking before this token.
    // Newly wrapped, so don't bother checking line length and suppress
    // penalty if the first token on a line happens to exceed column limit.
    cumulative_cost += current_format_token.before.break_penalty;
  } else if (spacing_choice == SpacingDecision::kAppend) {
    // Check for line length violation of column_for_penalty, and penalize
    // more for each column over the limit.
    if (column_for_penalty > style.column_limit) {
      cumulative_cost += style.over_column_limit_penalty + column_for_penalty -
                         style.column_limit;
    }
  }
  // no additional cost if Spacing::Preserve
}

void StateNode::OpenGroupBalance(const BasicFormatStyle &style) {
  VLOG(4) << __FUNCTION__;
  // The adjustment to the wrap_column_positions stack based on a token's
  // balance type is delayed until we see the token *after*.
  // If previous token was an open-group, then update indentation of
  // subsequent tokens to line up with the column of the open-group operator.
  // Otherwise, it should wrap to the previous state's column position.
  //
  // Illustrated:
  //
  //     [append-open-group, wrap-next-token]
  //     ...... (
  //         ^--- next wrap should line up here
  //
  //     [append-open-group, append-next-token]
  //     ...... ( ...something...
  //             ^--- next wrap should line up here
  //
  //     [wrap-open-group, wrap-next-token]
  //     ......
  //         (
  //              ^--- next wrap should line up here
  //
  //     [wrap-open-group, append-next-token]
  //     ......
  //         ( ...something...
  //          ^--- next wrap should line up here
  //

  // TODO(fangism): what if previous token is open, and new token is close?
  // Suppress?

  CHECK(!wrap_column_positions.empty());

  if (!IsRootState()) {
    const PreFormatToken &prev_format_token(GetPreviousToken());
    if (prev_format_token.balancing == GroupBalancing::kOpen) {
      VLOG(4) << "previous token is open-group";
      switch (spacing_choice) {
        case SpacingDecision::kWrap:
          VLOG(4) << "current token is wrapped";
          wrap_column_positions.push(prev_state->wrap_column_positions.top() +
                                     style.wrap_spaces);
          break;
        case SpacingDecision::kAlign:
          LOG(FATAL) << kNotForAlignment;
          break;
        case SpacingDecision::kAppend:
          VLOG(4) << "current token is appended or aligned";
          wrap_column_positions.push(prev_state->current_column);
          break;
        case SpacingDecision::kPreserve:
          // TODO(b/134711965): calculate column position using original spaces
          break;
      }
    }
  }
  // TODO(fangism): what if first token on unwrapped line is open-group?
}

void StateNode::CloseGroupBalance() {
  if (wrap_column_positions.size() > 1) {
    // Always maintain at least one element on column position stack.
    wrap_column_positions.pop();
  }

  // TODO(fangism): Align with the corresponding open-group operator,
  // assuming its string length is 1, but only when the open-group operator
  // has text that follows on the same line.
  // This will appear like:
  // ... (...
  //      ...
  //     ) <-- aligned with (
}

std::shared_ptr<const StateNode> StateNode::AppendIfItFits(
    const std::shared_ptr<const StateNode> &current_state,
    const verible::BasicFormatStyle &style) {
  if (current_state->Done()) return current_state;
  const auto &token = current_state->GetNextToken();
  // It seems little wasteful to always create both states when only one is
  // returned, but compiler optimization should be able to leverage this.
  // In any case, this is not a critical path operation, so we're not going to
  // worry about it.
  const auto wrapped =
      std::make_shared<StateNode>(current_state, style, SpacingDecision::kWrap);
  const auto appended = std::make_shared<StateNode>(current_state, style,
                                                    SpacingDecision::kAppend);
  return (token.before.break_decision == SpacingOptions::kMustWrap ||
          appended->current_column > style.column_limit)
             ? wrapped
             : appended;
}

std::shared_ptr<const StateNode> StateNode::QuickFinish(
    const std::shared_ptr<const StateNode> &current_state,
    const verible::BasicFormatStyle &style) {
  std::shared_ptr<const StateNode> latest(current_state);
  // Construct a chain of reference-counted states where the returned pointer
  // "holds on" to all of its ancestors like a singly-linked-list.
  while (!latest->Done()) {
    latest = AppendIfItFits(latest, style);
  }
  return latest;
}

void StateNode::ReconstructFormatDecisions(FormattedExcerpt *result) const {
  // Find all wrap decisions from the greatest ancestor state to this state.

  // This is allowed to work on any intermediate state in the search process,
  // so the depth can be less than the number of format tokens in the
  // UnwrappedLine.
  const size_t depth = Depth();
  CHECK_LE(depth, result->Tokens().size());

  const StateNode *reverse_iter = this;
  auto &format_tokens = result->MutableTokens();
  const auto format_tokens_slice =
      make_range(format_tokens.begin(), format_tokens.begin() + depth);
  for (auto &format_token : reversed_view(format_tokens_slice)) {
    const auto text = format_token.token->text();
    VLOG(3) << "reconstructing: " << text;
    // Apply decision at reverse_iter to (formatted) FormatToken.
    format_token.before.action = ABSL_DIE_IF_NULL(reverse_iter)->spacing_choice;
    if (reverse_iter->wrap_multiline_token_spaces_before >= 0) {
      VLOG(3) << "  wrapped a multi-line token, leading spaces was: "
              << reverse_iter->wrap_multiline_token_spaces_before;
      // This is a special case where a multi-line token was wrapped.
      // This number of spaces can only be inferred if the token that was
      // wrapped did not contain multi-line text.
      // In this case that spacing is not deducible, and had to be recorded.
      CHECK_EQ(reverse_iter->spacing_choice, SpacingDecision::kWrap);
      format_token.before.spaces =
          reverse_iter->wrap_multiline_token_spaces_before;
    } else if (reverse_iter->spacing_choice == SpacingDecision::kWrap) {
      // Mark as inserting a line break.
      // Immediately after a line break, print out the amount of spaces
      // required to honor the indentation and wrapping.
      format_token.before.spaces = reverse_iter->current_column - text.length();
      VLOG(3) << "  wrapped, with " << format_token.before.spaces
              << " leading spaces.";
      CHECK_GE(format_token.before.spaces, 0);
    }  // else: no need to calculate before.spaces.
    reverse_iter = reverse_iter->next();
  }
}

std::ostream &operator<<(std::ostream &stream, const StateNode &state) {
  // Omit information about remaining decisions and parent state.
  CHECK(!state.wrap_column_positions.empty());
  return stream << "spacing:" << state.spacing_choice <<  // noformat
         ", col@" << state.current_column <<              // noformat
         ", cost=" << state.cumulative_cost <<            // noformat
         ", [..." << state.wrap_column_positions.top() << ']';
}

}  // namespace verible
