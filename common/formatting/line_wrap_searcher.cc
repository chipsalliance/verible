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

#include "common/formatting/line_wrap_searcher.h"

#include <memory>
#include <queue>
#include <vector>

#include "absl/strings/string_view.h"
#include "common/formatting/basic_format_style.h"
#include "common/formatting/format_token.h"
#include "common/formatting/state_node.h"
#include "common/formatting/unwrapped_line.h"
#include "common/text/token_info.h"
#include "common/util/logging.h"

namespace verible {
namespace {

// Wrapped class around StateNode for the sake of adapting to a
// std::priority_queue interface.
// TODO(fangism): if performance of memory allocations is an issue,
// use some sort of pool-allocator.
struct SearchState {
  std::shared_ptr<const StateNode> state;

  explicit SearchState(const std::shared_ptr<const StateNode>& s) : state(s) {}

  // Inverted to min-heap: *lowest* penalty has the highest search priority.
  bool operator<(const SearchState& r) const { return *r.state < *state; }
};
}  // namespace

FormattedExcerpt SearchLineWraps(const UnwrappedLine& uwline,
                                 const BasicFormatStyle& style,
                                 int max_search_states) {
  // Dijkstra's algorithm for now: prioritize searching minimum penalty path
  // until destination is reached.

  VLOG(2) << "SearchLineWraps on: " << uwline;
  if (uwline.TokensRange().empty()) return FormattedExcerpt();

  // Worklist for decision searching, ordered by cumulative penalty.
  // Note: a heap-based priority-queue will not guarantee stable ordering
  // among equal-valued keys.  If first-come-first-serve tie-breaking is
  // important, consider switching to a std::map.
  std::priority_queue<SearchState> worklist;

  // Seed worklist with a NodeState that should have 0 penalty.
  SearchState seed(std::make_shared<StateNode>(uwline, style));
  worklist.push(seed);

  bool aborted_search = false;
  std::shared_ptr<const StateNode> winning_path;
  int state_count = 0;
  while (!worklist.empty()) {
    ++state_count;

    SearchState next(worklist.top());
    worklist.pop();

    VLOG(4) << "\n---- line wrapping search state " << state_count << " ----"
            << "\ncurrent cost: " << next.state->cumulative_cost
            << "\ncurrent column: " << next.state->current_column;

    // Check for done condition: reached the end of the UnwrappedLine's
    // FormatTokens.
    // First to reach the end has the lowest penalty and wins.
    // TODO(fangism): if we compare against uwline.end() iterator, we could save
    // some space from each StateNode object.
    if (next.state->Done()) {
      winning_path = next.state;
      VLOG(3) << "winning path cost: " << next.state->cumulative_cost;
      break;
    }

    if (state_count >= max_search_states) {
      // Search limit exceeded, abandon search.
      // Greedily finish formatting this partition, and return it.
      winning_path = StateNode::QuickFinish(next.state, style);
      aborted_search = true;
      break;
    }

    // Consider the new penalties incurred for the next decision:
    // break, or no break.  Calculate new penalties.
    // Push one or both branches into the worklist.
    const auto& token = next.state->GetNextToken();
    if (token.before.break_decision == SpacingOptions::Preserve) {
      VLOG(4) << "preserving spaces before \'" << token.token->text << '\'';
      SearchState preserved(std::make_shared<StateNode>(
          next.state, style, SpacingDecision::Preserve));
      worklist.push(preserved);
    } else {
      // Remaining options are: Undecided, MustWrap, MustAppend
      // Explore one or both: SpacingDecision::Wrap/Append
      if (token.before.break_decision != SpacingOptions::MustWrap) {
        VLOG(4) << "considering appending \'" << token.token->text << '\'';
        // Consider cost of appending token to current line.
        SearchState appended(std::make_shared<StateNode>(
            next.state, style, SpacingDecision::Append));
        worklist.push(appended);
        VLOG(4) << "  cost: " << appended.state->cumulative_cost;
        VLOG(4) << "  column: " << appended.state->current_column;
      }
      if (token.before.break_decision != SpacingOptions::MustAppend) {
        VLOG(4) << "considering wrapping \'" << token.token->text << '\'';
        // Consider cost of line wrapping here.
        SearchState wrapped(std::make_shared<StateNode>(next.state, style,
                                                        SpacingDecision::Wrap));
        worklist.push(wrapped);
        VLOG(4) << "  cost: " << wrapped.state->cumulative_cost;
        VLOG(4) << "  column: " << wrapped.state->current_column;
      }
    }

    // TODO(fangism): Use an admissibility heuristic to prune search space from
    // paths whose best-case outcome is worse than a conservatively achievable
    // goal.  Without a heuristic, this is just Dijkstra.
    // With heuristic pruning, this is A* (A-star).
  }  // while (!worklist.empty())

  if (VLOG_IS_ON(2) && !aborted_search) {
    // Count the number of equally good solutions without using them.
    // Having to arbitrarily pick among equal solutions can make integration
    // testing slightly unpredictable and fragile.
    // It is also an indicator that penalty costs are too similar in value,
    // which is a sign that the search state space may grow too quickly.
    int ties = 1;  // count the winning_path as one
    while (!worklist.empty()) {
      SearchState next(worklist.top());
      worklist.pop();
      if (winning_path->cumulative_cost == next.state->cumulative_cost) {
        if (next.state->Done()) {
          ++ties;
        }
      } else {
        break;  // Stop as soon as a state has higher cost.
      }
    }
    LOG(INFO) << "There is/are " << ties << " paths with equally minimal cost.";
    // TODO(b/145615062): In a special mode, show what these equal-cost
    // solutions look like, to make it easier to improve differentiation by
    // tuning penalty values.
  }

  // Reconstruct the unwrapped_line to reflect the decisions made to reach the
  // winning_path.  Return a modified copy of the original UnwrappedLine.
  FormattedExcerpt result(uwline);
  CHECK_EQ(winning_path->Depth(), result.Tokens().size());
  winning_path->ReconstructFormatDecisions(&result);
  if (aborted_search) {
    result.MarkIncomplete();
  }
  return result;
}

bool FitsOnLine(const UnwrappedLine& uwline, const BasicFormatStyle& style) {
  VLOG(3) << __FUNCTION__;
  // Leverage search functionality to compute effective line length of a slice
  // of tokens, taking into account minimum spacing requirements.
  // Similar to SearchLineWraps, but only calculates by appending tokens until
  // a line break is required.

  if (uwline.TokensRange().empty()) return true;

  // Initialize on first token.
  // This accounts for space consumed by left-indentation.
  auto state = std::make_shared<const StateNode>(uwline, style);

  while (!state->Done()) {
    const auto& token = state->GetNextToken();
    // If a line break is required before this token, return false.
    if (token.before.break_decision == SpacingOptions::MustWrap) {
      return false;
    }

    // Append token onto same line while it fits.
    state = std::make_shared<StateNode>(state, style, SpacingDecision::Append);
    if (state->current_column > style.column_limit) {
      return false;
    }
  }  // while (!state->Done())

  // Reached the end of token-range, thus, it fits.
  return true;
}

}  // namespace verible
