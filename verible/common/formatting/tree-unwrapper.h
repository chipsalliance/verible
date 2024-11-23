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

#ifndef VERIBLE_COMMON_FORMATTING_TREE_UNWRAPPER_H_
#define VERIBLE_COMMON_FORMATTING_TREE_UNWRAPPER_H_

#include <functional>
#include <iosfwd>
#include <vector>

#include "absl/strings/string_view.h"
#include "verible/common/formatting/format-token.h"
#include "verible/common/formatting/token-partition-tree.h"
#include "verible/common/formatting/unwrapped-line.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/text-structure.h"
#include "verible/common/text/token-info.h"
#include "verible/common/text/token-stream-view.h"
#include "verible/common/text/tree-context-visitor.h"
#include "verible/common/util/tree-operations.h"

namespace verible {
// Base class for building unwrapped lines. TreeUnwrapper is a concrete syntax
// tree visitor interleaved with a raw, unfiltered token stream. This allows the
// visitor to visit tokens between tree leaves, such as comments from the raw
// token stream, while building the unwrapped lines.
// For more information about the (unfiltered) TokenStreamView, see
// design documentation.
class TreeUnwrapper : public TreeContextVisitor {
 protected:
  using preformatted_tokens_type = std::vector<verible::PreFormatToken>;

 public:
  explicit TreeUnwrapper(const TextStructureView &view,
                         const preformatted_tokens_type &);

  // Deleted standard interfaces:
  TreeUnwrapper() = delete;
  TreeUnwrapper(const TreeUnwrapper &) = delete;
  TreeUnwrapper(TreeUnwrapper &&) = delete;
  TreeUnwrapper &operator=(const TreeUnwrapper &) = delete;
  TreeUnwrapper &operator=(TreeUnwrapper &&) = delete;

  ~TreeUnwrapper() override = default;  // not yet final.

  // Partitions the token stream (in text_structure_view_) into
  // unwrapped_lines_ by traversing the syntax tree representation.
  // TODO(fangism): rename this Partition.
  const TokenPartitionTree *Unwrap();

  // Returns a flattened copy of all of the deepest nodes in the tree of
  // unwrapped lines, which represents maximal partitioning into the smallest
  // partitions of format token ranges one might work with.
  std::vector<UnwrappedLine> FullyPartitionedUnwrappedLines() const;

  // Collects filtered tokens *before* the first syntax tree leaf.
  virtual void CollectLeadingFilteredTokens() = 0;

  // Collects filtered tokens *after* the last syntax tree leaf, up to EOF.
  virtual void CollectTrailingFilteredTokens() = 0;

  // Refers to the UnwrappedLine that is currently being built (const).
  const UnwrappedLine &CurrentUnwrappedLine() const;

  const TokenPartitionTree *CurrentTokenPartition() const {
    return active_unwrapped_lines_;
  }

  TokenPartitionTree *CurrentTokenPartition() {
    return active_unwrapped_lines_;
  }

  // Returns text spanned by the syntax tree being traversed.
  absl::string_view FullText() const { return text_structure_view_.Contents(); }

  // Transformation

  // Apply a mutating transformation to this class tree, pre-order traversal.
  void ApplyPreOrder(const std::function<void(TokenPartitionTree &)> &f) {
    verible::ApplyPreOrder(unwrapped_lines_, f);
  }

  // Apply a mutating transformation to this class tree, post-order traversal.
  void ApplyPostOrder(const std::function<void(TokenPartitionTree &)> &f) {
    verible::ApplyPostOrder(unwrapped_lines_, f);
  }

 protected:
  // Begins a new UnwrappedLine to span a new sub-range of format tokens.
  void StartNewUnwrappedLine(PartitionPolicyEnum partitioning,
                             const Symbol *origin);

  // Traverses the children of a node in postorder, recursively accepting this
  // visitor.
  void TraverseChildren(const verible::SyntaxTreeNode &node);

  // Override-able hook for actions that should be taken while in the
  // context of traversing children.
  virtual void InterChildNodeHook(const verible::SyntaxTreeNode &) {}

  // Visits a subtree with (possibly) additional indentation.
  // TODO(fangism): NOW: rename this to VisitSubPartition.
  void VisitIndentedSection(const verible::SyntaxTreeNode &node,
                            int indentation_delta, PartitionPolicyEnum);

  // Adds a token to CurrentUnwrappedLine() by advancing the end-iterator
  // of the range spanned by the current unwrapped line, and advances the
  // next_unfiltered_token_.
  // \precondition next_unfiltered_token_ and CurrentFormatTokenIterator point
  // to the same token in text_structure_view_.TokenStream().
  void AddTokenToCurrentUnwrappedLine();

  // Refers to the UnwrappedLine that is currently being built (mutable).
  UnwrappedLine &CurrentUnwrappedLine();

  // Iterator pointing to the most recent position in the preformatted_tokens_
  // array, that is accounted for in the CurrentUnwrappedLine().
  preformatted_tokens_type::const_iterator CurrentFormatTokenIterator() const;

  // Returns iterator into text_structure_view_.TokenStream().
  TokenSequence::const_iterator NextUnfilteredToken() const;

  // Consumes one unfiltered token by advancing the next_unfiltered_token_
  // iterator, and extending the CurrentUnwrappedLine() to cover it.
  void AdvanceNextUnfilteredToken();

  // Skip over uninteresting tokens, those for which the predicate is true.
  // For example, this could be used to skip over spaces, but not newlines.
  void SkipUnfilteredTokens(
      const std::function<bool(const verible::TokenInfo &)> &predicate);

  // Returns true of next_unfiltered_tokens_ points to a token that was kept
  // in preformatted_tokens_.
  bool NextUnfilteredTokenIsRetained() const;

  // Translate format token iterator into a numeric index, relative to
  // the start of preformatted_tokens_.
  // Mainly used for diagnostics and debugging.
  int TokenIndex(preformatted_tokens_type::const_iterator) const;

  // Return the EOFToken correpsonding to this text_structure_view_.
  TokenInfo EOFToken() const {
    return text_structure_view_.TokenStream().back();
    // Should be equivalent to text_structure_view_.EOFToken();
  }

 private:
  // Removes subtrees that represent empty token ranges, from the back.
  static void RemoveTrailingEmptyPartitions(TokenPartitionTree *);

  // Maintain invariant that parent range's end is equal to last-child's end.
  static void CloseUnwrappedLineTreeNode(
      TokenPartitionTree *, preformatted_tokens_type::const_iterator);

  // Finalizes an UnwrappedLine, prior to starting the next one.
  void FinishUnwrappedLine();

  // Returns the last iterator position from visiting a set of children.
  // This automatically restores active_unwrapped_lines_ on return.
  preformatted_tokens_type::const_iterator VisitIndentedChildren(
      const verible::SyntaxTreeNode &node, int indentation_delta,
      PartitionPolicyEnum);

  // Verifies parent-child token range equivalence in the entire tree of
  // unwrapped_lines_.
  void VerifyFullTreeFormatTokenRanges() const;

  // Data members (private):
  // written in construction-initialization order

  // The TextStructureView includes all of the information about the contents
  // of the file, including a syntax tree, raw token stream, and filtered
  // token stream
  const TextStructureView &text_structure_view_;

  // This is an annotated representation of tokens that require formatting
  // decisions, such as spaces and line breaks.  UnwrappedLines (in
  // unwrapped_lines_) will reference sub-ranges of this array
  // (thus, this member should outlive those UnwrappedLines).
  // CurrentFormatTokenIterator() always points to iterators in this
  // container's range.
  const preformatted_tokens_type &preformatted_tokens_;

  // Iterator pointing into text_structure_view_.TokenStream().
  // This covers non-whitespace tokens like comments and attributes
  // which will be between the leaves of the syntax tree.
  // At any time, this may lead or lag behind the token referenced by
  // CurrentFormatTokenIterator().
  TokenSequence::const_iterator next_unfiltered_token_;

  // current_indentation_spaces_ corresponds to the current left-indentation
  // number of spaces.
  int current_indentation_spaces_ = 0;

  // Hierarchical set of UnwrappedLines.
  // Implemented as a tree structure so that a separate pass can decide
  // which nodes of the tree should be operated on expanded/unexpanded.
  //
  // Critical invariant properties:
  //   1) The format token range spanned by any tree node (UnwrappedLine) is
  //      equal to that of its children.
  //   2) Adjacent siblings begin/end iterators are equal (continuity).
  TokenPartitionTree unwrapped_lines_;

  // Pointer to currently growing set of UnwrappedLines.
  // At any given time, this points to unwrapped_lines_, or a subtree node
  // thereof.  This is maintained in a stack-like manner where this pointer
  // represents the top of a stack of tree nodes that is balanced during
  // tree traversal.
  // No container is actually needed because popping the stack is a matter
  // of replacing this pointer with its Parent().
  TokenPartitionTree *active_unwrapped_lines_ = nullptr;
};

// Prints all of the unwrapped_lines_.  Used for diagnostics only.
std::ostream &operator<<(std::ostream &, const TreeUnwrapper &);

}  // namespace verible

#endif  // VERIBLE_COMMON_FORMATTING_TREE_UNWRAPPER_H_
