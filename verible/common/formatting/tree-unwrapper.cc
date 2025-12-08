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

#include "verible/common/formatting/tree-unwrapper.h"

#include <functional>
#include <iterator>
#include <memory>
#include <ostream>
#include <vector>

#include "verible/common/formatting/format-token.h"
#include "verible/common/formatting/token-partition-tree.h"
#include "verible/common/formatting/unwrapped-line.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/syntax-tree-context.h"
#include "verible/common/text/text-structure.h"
#include "verible/common/text/token-info.h"
#include "verible/common/text/token-stream-view.h"
#include "verible/common/util/logging.h"
#include "verible/common/util/tree-operations.h"
#include "verible/common/util/value-saver.h"
#include "verible/common/util/vector-tree.h"

namespace verible {

void TreeUnwrapper::VerifyFullTreeFormatTokenRanges() const {
  verible::VerifyFullTreeFormatTokenRanges(unwrapped_lines_,
                                           preformatted_tokens_.begin());
}

static TokenPartitionTree MakeInitialUnwrappedLines(
    int indentation, FormatTokenRange::const_iterator first_token) {
  // Root node spanning the entire file
  auto unwrapped_lines = TokenPartitionTree(UnwrappedLine(
      indentation, first_token, PartitionPolicyEnum::kAlwaysExpand));
  // First unwrapped line
  unwrapped_lines.Children().emplace_back(UnwrappedLine(
      indentation, first_token
      // TODO(fangism): set partition policy here?
      ));
  return unwrapped_lines;
}

TreeUnwrapper::TreeUnwrapper(
    const TextStructureView &view,
    const preformatted_tokens_type &preformatted_tokens)
    : text_structure_view_(view),
      preformatted_tokens_(preformatted_tokens),
      next_unfiltered_token_(text_structure_view_.TokenStream().begin()),
      unwrapped_lines_(MakeInitialUnwrappedLines(current_indentation_spaces_,
                                                 preformatted_tokens_.begin())),
      // The "top-most" UnwrappedLine spans the entire file, so the first
      // unwrapped line should be a considered a partition (child) thereof.
      // This acts like 'pushing' the first child onto a stack.
      active_unwrapped_lines_(&unwrapped_lines_.Children().front()) {
  // Every new unwrapped line will be initially empty, but the range
  // will point to the correct starting position in the preformatted_tokens_
  // array, and be able to 'extend' into the array of preformatted_tokens_.
}

const TokenPartitionTree *TreeUnwrapper::Unwrap() {
  // Collect tokens that appear before first syntax tree leaf, e.g. comments.
  CollectLeadingFilteredTokens();

  // Traverse the concrete syntax tree to build up token partitions.
  ABSL_DIE_IF_NULL(text_structure_view_.SyntaxTree())->Accept(this);

  // After traversing the ConcreteSyntaxTree, collect possible tokens filtered
  // after the right-most leaf until the end-of-file.
  CollectTrailingFilteredTokens();

  // No action needed to close out the most recent UnwrappedLine.
  if (!preformatted_tokens_.empty()) {
    const auto iter = CurrentFormatTokenIterator() - 1;
    const auto back = preformatted_tokens_.end() - 1;
    // Ensure that we have spanned the last significant token (used for
    // formatting).  It is possible that unfiltered tokens include trailing
    // newlines after the last leaf, which is why the iterators may not
    // necessarily line up exactly.
    CHECK(iter >= back) << "missing " << std::distance(iter, back)
                        << " format tokens at the end.  got: " << *iter->token
                        << " vs. " << *back->token;
  }

  {
    // This 'pops' the tree node stack once more to balance the initial
    // child 'push' that was done in the constructor's initialization
    // of active_unwrapped_lines_.
    active_unwrapped_lines_ = active_unwrapped_lines_->Parent();
    // Confirm that tree visitation is balanced.
    CHECK_EQ(active_unwrapped_lines_, &unwrapped_lines_);
    FinishUnwrappedLine();
  }

  VerifyFullTreeFormatTokenRanges();

  return &unwrapped_lines_;
}

std::vector<UnwrappedLine> TreeUnwrapper::FullyPartitionedUnwrappedLines()
    const {
  // If a node of the ExpandedTree<UnwrappedLine> has children,
  // visit only the node's children.
  std::vector<UnwrappedLine> result;
  verible::ApplyPostOrder(unwrapped_lines_,
                          [&result](const TokenPartitionTree &node) {
                            if (is_leaf(node)) {
                              result.push_back(node.Value());
                            }
                          });

  // Filter out trailing blank UnwrappedLines.
  while (!result.empty() && result.back().IsEmpty()) {
    result.pop_back();
  }
  return result;
}

TokenSequence::const_iterator TreeUnwrapper::NextUnfilteredToken() const {
  const auto &origin_tokens = text_structure_view_.TokenStream();
  CHECK(next_unfiltered_token_ >= origin_tokens.begin());
  CHECK(next_unfiltered_token_ <= origin_tokens.end());
  return next_unfiltered_token_;
}

TreeUnwrapper::preformatted_tokens_type::const_iterator
TreeUnwrapper::CurrentFormatTokenIterator() const {
  // Caution to caller: this could return preformatted_tokens_.end()
  return CurrentUnwrappedLine().TokensRange().end();
}

UnwrappedLine &TreeUnwrapper::CurrentUnwrappedLine() {
  return ABSL_DIE_IF_NULL(CurrentTokenPartition())->Value();
}

const UnwrappedLine &TreeUnwrapper::CurrentUnwrappedLine() const {
  return ABSL_DIE_IF_NULL(CurrentTokenPartition())->Value();
}

void TreeUnwrapper::RemoveTrailingEmptyPartitions(TokenPartitionTree *node) {
  auto &children = node->Children();
  while (!children.empty() && children.back().Value().IsEmpty()) {
    children.pop_back();
  }
}

void TreeUnwrapper::CloseUnwrappedLineTreeNode(
    TokenPartitionTree *node,
    preformatted_tokens_type::const_iterator token_iter) {
  const auto &children = node->Children();
  if (!children.empty()) {
    const auto last_child_end = children.back().Value().TokensRange().end();
    CHECK(last_child_end >= token_iter)
        << "Child range should never have to catch up to parent.";
    if (token_iter < last_child_end) {
      // Parent needs to catch up to child.
      // This can occur because we're only updating one active_unwrapped_line_
      // node at a time, so this is needed to maintain the parent-child
      // range equivalence.
      node->Value().SpanUpToToken(last_child_end);
    }
  }
}

void TreeUnwrapper::FinishUnwrappedLine() {
  RemoveTrailingEmptyPartitions(active_unwrapped_lines_);

  const auto iter = CurrentFormatTokenIterator();
  CloseUnwrappedLineTreeNode(active_unwrapped_lines_, iter);

  // At this point, the current active_unwrapped_lines_ is finalized because
  // we are starting a new one.  Now is the right time to verify invariants.
  VerifyTreeNodeFormatTokenRanges(*active_unwrapped_lines_,
                                  preformatted_tokens_.begin());
}

void TreeUnwrapper::StartNewUnwrappedLine(PartitionPolicyEnum partitioning,
                                          const Symbol *origin) {
  // TODO(fangism): Take an optional indentation depth override parameter.
  auto &current_unwrapped_line = CurrentUnwrappedLine();
  if (current_unwrapped_line.IsEmpty()) {  // token range is empty
    // Re-use previously created unwrapped line.
    current_unwrapped_line.SetIndentationSpaces(current_indentation_spaces_);
    current_unwrapped_line.SetPartitionPolicy(partitioning);
    current_unwrapped_line.SetOrigin(origin);
    VLOG(4) << "re-using node at " << NodePath(*active_unwrapped_lines_) << ": "
            << current_unwrapped_line;
    // There may have been subtrees created with empty ranges, e.g.
    // for the sake of being able to correctly indent comments inside blocks.
    // If so, delete those so that token partition range invariants are
    // maintained through re-use of an existing node.
    if (!is_leaf(*active_unwrapped_lines_)) {
      VLOG(4) << "removed pre-existing child partitions.";
      active_unwrapped_lines_->Children().clear();
    }
  } else {
    // To maintain the invariant that a parent range's upper-bound is equal
    // to the upper-bound of its last child, we may have to add one more
    // child whose range spans up to the parent's upper-bound.
    // The right time to do this is when an UnwrappedLine is finalized,
    // which is the same time that a new UnwrappedLine is added, here.
    FinishUnwrappedLine();

    // Create new sibling to current unwrapped line, maintaining same level.
    auto &siblings = active_unwrapped_lines_->Parent()->Children();
    siblings.emplace_back(UnwrappedLine(current_indentation_spaces_,
                                        CurrentFormatTokenIterator(),
                                        partitioning));
    active_unwrapped_lines_ = &siblings.back();
    CurrentUnwrappedLine().SetOrigin(origin);
    VLOG(4) << "new sibling node " << NodePath(*active_unwrapped_lines_) << ": "
            << CurrentUnwrappedLine();
  }
}

void TreeUnwrapper::AddTokenToCurrentUnwrappedLine() {
  CHECK(NextUnfilteredTokenIsRetained());
  // Advance CurrentFormatTokenIterator().
  CurrentUnwrappedLine().SpanNextToken();
  VLOG(4) << "appended: " << *CurrentUnwrappedLine().TokensRange().back().token;
  ++next_unfiltered_token_;
}

bool TreeUnwrapper::NextUnfilteredTokenIsRetained() const {
  const auto iter = CurrentFormatTokenIterator();
  // (iter->token == &*next_unfiltered_token_) implies that is was one of the
  // tokens preserved in the subset array of filtered tokens, as determined
  // by a predication function (keeper), but without having to re-check
  // (and maintain a copy/reference of) the keeper predicate, nor perform
  // a set membership check (e.g. binary search).
  // This works only because we maintain that next_unfiltered_token_
  // will never lead nor lag CurrentFormatTokenIterator() by more than one
  // filtered token.
  return iter != preformatted_tokens_.end() &&  // possible if array is empty
         iter->token == &*next_unfiltered_token_;
}

void TreeUnwrapper::SkipUnfilteredTokens(
    const std::function<bool(const verible::TokenInfo &)> &predicate) {
  while (predicate(*next_unfiltered_token_)) {
    ++next_unfiltered_token_;
  }
}

// Advances next_unfiltered_token_ and also places the token into the
// CurrentUnwrappedLine() if it is a non-whitespace token, like a comment.
void TreeUnwrapper::AdvanceNextUnfilteredToken() {
  if (next_unfiltered_token_->isEOF()) return;
  if (NextUnfilteredTokenIsRetained()) {
    // This is a non-syntax-tree token, such as a comment or attribute.
    // Based on the KeepNonWhitespace predicate, next_unfiltered_token_
    // must point to the next PreFormatToken.
    // This already advances next_unfiltered_token_.
    AddTokenToCurrentUnwrappedLine();
  } else {
    // The inverse condition implies that the token pointed to was filtered
    // out, e.g. whitespace.
    ++next_unfiltered_token_;
  }
}

void TreeUnwrapper::TraverseChildren(const verible::SyntaxTreeNode &node) {
  // Can't just use TreeContextVisitor::Visit(node) because we need to
  // call a visit hook between children.
  const verible::SyntaxTreeContext::AutoPop p(&current_context_, &node);
  InterChildNodeHook(node);
  for (const auto &child : node.children()) {
    if (child) {
      child->Accept(this);
      InterChildNodeHook(node);
    }
  }
}

TreeUnwrapper::preformatted_tokens_type::const_iterator
TreeUnwrapper::VisitIndentedChildren(const SyntaxTreeNode &node,
                                     int indentation_delta,
                                     PartitionPolicyEnum partitioning) {
  // Visit subtree with increased indentation level.
  const ValueSaver<int> depth_saver(
      &current_indentation_spaces_,
      current_indentation_spaces_ + indentation_delta);

  // Mark a new sibling at the new indentation level, apply partition policy.
  StartNewUnwrappedLine(partitioning, &node);

  // Start first child right away.
  active_unwrapped_lines_->Children().emplace_back(
      UnwrappedLine(current_indentation_spaces_, CurrentFormatTokenIterator(),
                    PartitionPolicyEnum::kFitOnLineElseExpand /* default */));
  const ValueSaver<TokenPartitionTree *> tree_saver(
      &active_unwrapped_lines_, &active_unwrapped_lines_->Children().back());
  VLOG(3) << __FUNCTION__ << ", new child node "
          << NodePath(*active_unwrapped_lines_) << ": "
          << CurrentUnwrappedLine();
  TraverseChildren(node);

  return CurrentFormatTokenIterator();

  // To maintain the invariant that a parent range's upper-bound is equal
  // to the upper-bound of its last child, we may have to add one more
  // child whose range spans up to the parent's upper-bound.
  // The right time to do this is when an UnwrappedLine is finalized,
  // which is the same time that a new UnwrappedLine is added.
  // See StartNewUnwrappedLine().
}

void TreeUnwrapper::VisitIndentedSection(const SyntaxTreeNode &node,
                                         int indentation_delta,
                                         PartitionPolicyEnum partitioning) {
  const auto last_ftoken_iter =
      VisitIndentedChildren(node, indentation_delta, partitioning);

  // TODO(fangism): do we ever need to remove trailing empty partitions here?

  // Update parent's end() format token iterator to match that of
  // its last child.  It can still be advanced later.
  active_unwrapped_lines_->Value().SpanUpToToken(last_ftoken_iter);

  // Start new empty UnwrappedLine at the previous indentation level.
  StartNewUnwrappedLine(PartitionPolicyEnum::kUninitialized, nullptr);
}

std::ostream &operator<<(std::ostream &stream, const TreeUnwrapper &unwrapper) {
  for (const auto &uwline : unwrapper.FullyPartitionedUnwrappedLines()) {
    stream << uwline << std::endl;
  }
  return stream;
}

}  // namespace verible
