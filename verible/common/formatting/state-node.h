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

#ifndef VERIBLE_COMMON_FORMATTING_STATE_NODE_H_
#define VERIBLE_COMMON_FORMATTING_STATE_NODE_H_

#include <cstddef>
#include <iosfwd>
#include <memory>
#include <stack>
#include <vector>

#include "verible/common/formatting/basic-format-style.h"
#include "verible/common/formatting/format-token.h"
#include "verible/common/formatting/unwrapped-line.h"
#include "verible/common/util/container-iterator-range.h"

namespace verible {

// A StateNode is used to keep a formatting state as the tokens of an
// UnwrappedLine are searched left to right.  Each StateNode represents one
// formatting decision: wrap or not-wrap.  Each StateNode maintains a pointer
// to its parent state, which is used for backtracking once a solution
// is reached.  StateNode is language-agnostic.
// StateNode is purely an implementation detail of line_wrap_searcher.cc.
struct StateNode {
  using path_type = std::vector<PreFormatToken>;
  using range_type = container_iterator_range<path_type::const_iterator>;

  // The StateNode that has an edge to this StateNode, to backtrack once a final
  // state is reached.
  std::shared_ptr<const StateNode> prev_state;

  // Iterator range marking the unexplored decisions beyond the current token.
  // TODO(fangism): make the iterator type a template parameter.  Might help
  // with mocking and testing.
  range_type undecided_path;

  // Explores one of the SpacingDecision choices.
  SpacingDecision spacing_choice = SpacingDecision::kPreserve;

  // The current column position, this increases with every token that is
  // appended onto the current line, and resets to the indentation level
  // (plus wrapping) with every line break.
  int current_column = 0;

  // The total cost along this decision path.  This monotonically increases
  // with each decision explored.
  int cumulative_cost = 0;

  // Kludge: in the event of a wrapped multi-line token, the current_column
  // position and the raw token text length are insufficient to infer what
  // the spaces before the format token are because current_column is only
  // based on the substring of text after the last newline.  To be able to
  // reconstruct the pre-format-token spacing, we must record it.
  // This is initialized with a sentinel value of -1 as a safety precaution
  // to guard against accidental use.
  int wrap_multiline_token_spaces_before = -1;

  // Keeps track of column positions of every level of wrapping, as determined
  // by balanced group delimiters such as braces, brackets, parentheses.
  // These column positions correspond to either the current indentation level
  // plus wrapping or the column position of the nearest group-opening
  // delimiter.
  // TODO(b/135730018): re-implement to minimize copying of stacks.
  // For example, use pointer or iterator to previous stack update.
  std::stack<int> wrap_column_positions;

  // Constructor for the root node of the search path, with no parent.
  // This automatically places the first token at the beginning of a new line
  // for position tracking purposes.
  // If the UnwrappedLine has only one token or is empty, the initial state
  // will be Done().
  StateNode(const UnwrappedLine &uwline, const BasicFormatStyle &style);

  // Constructor for nodes that represent new wrap decision trees to explore.
  // 'spacing_choice' reflects the decision being explored, e.g. append, wrap,
  // preserve.
  StateNode(const std::shared_ptr<const StateNode> &parent,
            const BasicFormatStyle &style, SpacingDecision spacing_choice);

  // Returns true when the undecided_path is empty.
  // The search is over when there are no more decisions to explore.
  bool Done() const { return undecided_path.begin() == undecided_path.end(); }

  // Returns a reference to the token that is being acted upon in this state.
  const PreFormatToken &GetCurrentToken() const {
    // The undecided_path always starts at the position after the current token.
    return *(undecided_path.begin() - 1);
  }

  // Returns a reference to the token that considered for wrapping vs.
  // appending.
  const PreFormatToken &GetNextToken() const {
    // The undecided_path always starts at the position after the current token.
    return *undecided_path.begin();
  }

  // Returns pointer to previous state before this decision node.
  // This functions as a forward-iterator going up the state ancestry chain.
  const StateNode *next() const { return prev_state.get(); }

  // Returns true if this state was initialized with an unwrapped line and
  // has no parent state.
  bool IsRootState() const { return prev_state == nullptr; }

  // Returns the total number of nodes in state ancestry, including itself.
  // This occurs in O(N) time, and is only suitable for testing/debug.
  size_t Depth() const {
    size_t depth = 1;
    const auto *iter = this;
    while (!iter->IsRootState()) {
      ++depth;
      iter = iter->prev_state.get();
    }
    return depth;
  }

  // Produce next state by appending a token if the result stays under the
  // column limit, or breaking onto a new line if required.
  static std::shared_ptr<const StateNode> AppendIfItFits(
      const std::shared_ptr<const StateNode> &current_state,
      const BasicFormatStyle &style);

  // Repeatedly apply AppendIfItFits() until Done() with formatting.
  // TODO(b/134711965): We may want a variant that preserves spaces too.
  static std::shared_ptr<const StateNode> QuickFinish(
      const std::shared_ptr<const StateNode> &current_state,
      const BasicFormatStyle &style);

  // Comparator provides an ordering of which paths should be explored
  // when maintained in a priority queue.  For Dijsktra-style algorithms,
  // we want to explore the min-cost paths first.
  bool operator<(const StateNode &r) const {
    return cumulative_cost < r.cumulative_cost ||
           // TODO(b/145558510): Favor solutions that use fewer lines.
           // To do that would require counting number of wrap decisions,
           // which is slow, unless we keep track of that number in StateNode.

           // tie-breaker: All else being equal, use terminal column position.
           (cumulative_cost == r.cumulative_cost &&
            current_column < r.current_column);
  }

  // Applies decisions from a path search to the set of format tokens in a
  // FormattedExcerpt.  'this' is the last decision in a tree that encodes
  // wrap decisions (through ancestry chain: prev_state) all the way back to
  // the first token in the original UnwrappedLine (that was used to
  // initialize the root state).
  void ReconstructFormatDecisions(FormattedExcerpt *) const;

 private:
  const PreFormatToken &GetPreviousToken() const;

  int UpdateColumnPosition();
  void UpdateCumulativeCost(const BasicFormatStyle &, int column_for_penalty);
  void OpenGroupBalance(const BasicFormatStyle &);
  void CloseGroupBalance();
};

// Human-readable representation for debugging only.
std::ostream &operator<<(std::ostream &, const StateNode &);

}  // namespace verible

#endif  // VERIBLE_COMMON_FORMATTING_STATE_NODE_H_
