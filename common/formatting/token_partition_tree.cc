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

#include "common/formatting/token_partition_tree.h"

#include <cstddef>
#include <iterator>
#include <vector>

#include "common/formatting/format_token.h"
#include "common/formatting/line_wrap_searcher.h"
#include "common/formatting/unwrapped_line.h"
#include "common/strings/display_utils.h"
#include "common/strings/range.h"
#include "common/text/tree_utils.h"
#include "common/util/algorithm.h"
#include "common/util/container_iterator_range.h"
#include "common/util/logging.h"
#include "common/util/spacer.h"
#include "common/util/top_n.h"
#include "common/util/vector_tree.h"

namespace verible {

using format_token_iterator = std::vector<PreFormatToken>::const_iterator;

void VerifyTreeNodeFormatTokenRanges(const TokenPartitionTree& node,
                                     format_token_iterator base) {
  VLOG(4) << __FUNCTION__ << " @ node path: " << NodePath(node);

  // Converting an iterator to an index is easier for debugging.
  auto TokenIndex = [=](format_token_iterator iter) {
    return std::distance(base, iter);
  };

  const auto& children = node.Children();
  if (!children.empty()) {
    const TokenPartitionTreePrinter node_printer(node);
    {
      // Hierarchy invariant: parent's range == range spanned by children.
      // Check against first child's begin, and last child's end.
      const auto& parent_range = node.Value().TokensRange();
      // Translates ranges' iterators into positional indices.
      const int parent_begin = TokenIndex(parent_range.begin());
      const int parent_end = TokenIndex(parent_range.end());
      const int children_begin =
          TokenIndex(children.front().Value().TokensRange().begin());
      const int children_end =
          TokenIndex(children.back().Value().TokensRange().end());
      CHECK_EQ(parent_begin, children_begin) << "node:\n" << node_printer;
      CHECK_EQ(parent_end, children_end) << "node:\n" << node_printer;
    }
    {
      // Sibling continuity invariant:
      // The end() of one child is the begin() of the next child.
      auto iter = children.begin();
      const auto end = children.end();
      auto prev_upper_bound = iter->Value().TokensRange().end();
      for (++iter; iter != end; ++iter) {
        const auto child_range = iter->Value().TokensRange();
        const int current_begin = TokenIndex(child_range.begin());
        const int previous_end = TokenIndex(prev_upper_bound);
        CHECK_EQ(current_begin, previous_end) << "node:\n" << node_printer;
        prev_upper_bound = child_range.end();
      }
    }
  }
  VLOG(4) << __FUNCTION__ << " (verified)";
}

void VerifyFullTreeFormatTokenRanges(const TokenPartitionTree& tree,
                                     format_token_iterator base) {
  VLOG(4) << __FUNCTION__ << '\n' << TokenPartitionTreePrinter{tree};
  tree.ApplyPreOrder([=](const TokenPartitionTree& node) {
    VerifyTreeNodeFormatTokenRanges(node, base);
  });
}

struct SizeCompare {
  bool operator()(const UnwrappedLine* left, const UnwrappedLine* right) const {
    return left->Size() > right->Size();
  }
};

std::vector<const UnwrappedLine*> FindLargestPartitions(
    const TokenPartitionTree& token_partitions, size_t num_partitions) {
  // Sort UnwrappedLines from leaf partitions by size.
  using partition_set_type = verible::TopN<const UnwrappedLine*, SizeCompare>;
  partition_set_type partitions(num_partitions);
  token_partitions.ApplyPreOrder([&partitions](const TokenPartitionTree& node) {
    if (node.is_leaf()) {  // only look at leaf partitions
      partitions.push(&node.Value());
    }
  });
  return partitions.Take();
}

std::vector<std::vector<int>> FlushLeftSpacingDifferences(
    const TokenPartitionRange& partitions) {
  // Compute per-token differences between original spacings and reference-value
  // spacings.
  std::vector<std::vector<int>> flush_left_spacing_deltas;
  flush_left_spacing_deltas.reserve(partitions.size());
  for (const auto& partition : partitions) {
    flush_left_spacing_deltas.emplace_back();
    std::vector<int>& row(flush_left_spacing_deltas.back());
    FormatTokenRange ftokens(partition.Value().TokensRange());
    if (ftokens.empty()) continue;
    // Skip the first token, because that represents indentation.
    ftokens.pop_front();
    for (const auto& ftoken : ftokens) {
      row.push_back(ftoken.ExcessSpaces());
    }
  }
  return flush_left_spacing_deltas;
}

std::ostream& TokenPartitionTreePrinter::PrintTree(std::ostream& stream,
                                                   int indent) const {
  const auto& value = node.Value();
  const auto& children = node.Children();
  stream << Spacer(indent) << "{ ";
  if (children.empty()) {
    stream << '(';
    value.AsCode(&stream, verbose);
    stream << ") }";
  } else {
    stream << '('
           // similar to UnwrappedLine::AsCode()
           << Spacer(value.IndentationSpaces(),
                     UnwrappedLine::kIndentationMarker)
           // <auto> just means the concatenation of all subpartitions
           << "[<auto>], policy: " << value.PartitionPolicy() << ") @"
           << NodePath(node);
    if (value.Origin() != nullptr) {
      static constexpr int kContextLimit = 25;
      stream << ", (origin: \""
             << AutoTruncate{StringSpanOfSymbol(*value.Origin()), kContextLimit}
             << "\")";
    }
    stream << '\n';
    // token range spans all of children nodes
    for (const auto& child : children) {
      TokenPartitionTreePrinter(child, verbose).PrintTree(stream, indent + 2)
          << '\n';
    }
    stream << Spacer(indent) << '}';
  }
  return stream;
}

std::ostream& operator<<(std::ostream& stream,
                         const TokenPartitionTreePrinter& printer) {
  return printer.PrintTree(stream);
}

// Detects when there is a vertical separation of more than one line between
// two token partitions.
class BlankLineSeparatorDetector {
 public:
  // 'bounds' range must not be empty.
  explicit BlankLineSeparatorDetector(const TokenPartitionRange& bounds)
      : previous_end_(bounds.front()
                          .Value()
                          .TokensRange()
                          .front()
                          .token->text()
                          .begin()) {}

  bool operator()(const TokenPartitionTree& node) {
    const auto range = node.Value().TokensRange();
    if (range.empty()) return false;
    const auto begin = range.front().token->text().begin();
    const auto end = range.back().token->text().end();
    const auto gap = make_string_view_range(previous_end_, begin);
    // A blank line between partitions contains 2+ newlines.
    const bool new_bound = std::count(gap.begin(), gap.end(), '\n') >= 2;
    previous_end_ = end;
    return new_bound;
  }

 private:
  // Keeps track of the end of the previous partition, which is the start
  // of each inter-partition gap (string_view).
  absl::string_view::const_iterator previous_end_;
};

// Subdivides the 'bounds' range into sub-ranges broken up by blank lines.
static std::vector<TokenPartitionIterator>
PartitionTokenPartitionRangesAtBlankLines(const TokenPartitionRange& bounds) {
  VLOG(2) << __FUNCTION__;
  std::vector<TokenPartitionIterator> subpartitions;
  if (bounds.empty()) return subpartitions;
  subpartitions.push_back(bounds.begin());
  // Bookkeeping for the end of the previous token range, used to evaluate
  // the inter-token-range text, looking for blank line.
  verible::find_all(bounds.begin(), bounds.end(),
                    std::back_inserter(subpartitions),
                    BlankLineSeparatorDetector(bounds));
  subpartitions.push_back(bounds.end());
  VLOG(2) << "end of " << __FUNCTION__
          << ", boundaries: " << subpartitions.size();
  return subpartitions;
}

std::vector<TokenPartitionRange> GetSubpartitionsBetweenBlankLines(
    const TokenPartitionRange& outer_partition_bounds) {
  VLOG(2) << __FUNCTION__;
  std::vector<TokenPartitionRange> result;
  {
    const std::vector<TokenPartitionIterator> subpartitions_bounds(
        PartitionTokenPartitionRangesAtBlankLines(outer_partition_bounds));
    CHECK_GE(subpartitions_bounds.size(), 2);
    result.reserve(subpartitions_bounds.size());

    auto prev = subpartitions_bounds.begin();
    // similar pattern to std::adjacent_difference.
    for (auto next = std::next(prev); next != subpartitions_bounds.end();
         prev = next, ++next) {
      result.emplace_back(*prev, *next);
    }
  }
  VLOG(2) << "end of " << __FUNCTION__;
  return result;
}

static absl::string_view StringSpanOfPartitionRange(
    const TokenPartitionRange& range) {
  CHECK(!range.empty());
  const auto front_range = range.front().Value().TokensRange();
  const auto back_range = range.back().Value().TokensRange();
  CHECK(!front_range.empty());
  CHECK(!back_range.empty());
  return make_string_view_range(front_range.front().Text().begin(),
                                back_range.back().Text().end());
}

bool AnyPartitionSubRangeIsDisabled(TokenPartitionRange range,
                                    absl::string_view full_text,
                                    const ByteOffsetSet& disabled_byte_ranges) {
  if (range.empty()) return false;
  const absl::string_view span = StringSpanOfPartitionRange(range);
  VLOG(4) << "text spanned: " << AutoTruncate{span, 40};
  const std::pair<int, int> span_offsets = SubstringOffsets(span, full_text);
  ByteOffsetSet diff(disabled_byte_ranges);  // copy
  diff.Complement(span_offsets);             // enabled range(s)
  const ByteOffsetSet span_set{span_offsets};
  return diff != span_set;
}

void AdjustIndentationRelative(TokenPartitionTree* tree, int amount) {
  ABSL_DIE_IF_NULL(tree)->ApplyPreOrder([&](UnwrappedLine& line) {
    const int new_indent = std::max<int>(line.IndentationSpaces() + amount, 0);
    line.SetIndentationSpaces(new_indent);
  });
}

void AdjustIndentationAbsolute(TokenPartitionTree* tree, int amount) {
  // Compare the indentation difference at the root node.
  const int indent_diff = amount - tree->Value().IndentationSpaces();
  if (indent_diff == 0) return;
  AdjustIndentationRelative(tree, indent_diff);
}

static absl::string_view StringSpanOfTokenRange(const FormatTokenRange& range) {
  CHECK(!range.empty());
  return make_string_view_range(range.front().Text().begin(),
                                range.back().Text().end());
}

void IndentButPreserveOtherSpacing(TokenPartitionRange partition_range,
                                   absl::string_view full_text,
                                   std::vector<PreFormatToken>* ftokens) {
  for (const auto& partition : partition_range) {
    const auto token_range = partition.Value().TokensRange();
    const absl::string_view partition_text =
        StringSpanOfTokenRange(token_range);
    std::pair<int, int> byte_range =
        SubstringOffsets(partition_text, full_text);
    // Tweak byte range to allow the first token to still obey indentation.
    ++byte_range.first;
    PreserveSpacesOnDisabledTokenRanges(ftokens, ByteOffsetSet{byte_range},
                                        full_text);
  }
}

void MergeConsecutiveSiblings(TokenPartitionTree* tree, size_t pos) {
  // Effectively concatenate unwrapped line ranges of sibling subpartitions.
  ABSL_DIE_IF_NULL(tree)->MergeConsecutiveSiblings(
      pos, [](UnwrappedLine* left, const UnwrappedLine& right) {
        // Verify token range continuity.
        CHECK(left->TokensRange().end() == right.TokensRange().begin());
        left->SpanUpToToken(right.TokensRange().end());
      });
}

// From leaf node's parent upwards, update the left bound of the UnwrappedLine's
// TokenRange.  Stop as soon as a node is not a (leftmost) first child.
static void UpdateTokenRangeLowerBound(TokenPartitionTree* leaf,
                                       TokenPartitionTree* last,
                                       format_token_iterator token_iter) {
  for (auto* node = leaf; node != nullptr && node != last;
       node = node->Parent()) {
    node->Value().SpanBackToToken(token_iter);
  }
}

// From leaf node upwards, update the right bound of the UnwrappedLine's
// TokenRange.  Stop as soon as a node is not a (rightmost) last child.
static void UpdateTokenRangeUpperBound(TokenPartitionTree* leaf,
                                       TokenPartitionTree* last,
                                       format_token_iterator token_iter) {
  for (auto* node = leaf; node != nullptr && node != last;
       node = node->Parent()) {
    node->Value().SpanUpToToken(token_iter);
  }
}

// Note: this destroys leaf
TokenPartitionTree* MergeLeafIntoPreviousLeaf(TokenPartitionTree* leaf) {
  VLOG(4) << "origin leaf:\n" << *leaf;
  auto* target_leaf = ABSL_DIE_IF_NULL(leaf)->PreviousLeaf();
  if (target_leaf == nullptr) return nullptr;
  VLOG(4) << "target leaf:\n" << *target_leaf;

  // If there is no common ancestor, do nothing and return.
  auto& common_ancestor =
      *ABSL_DIE_IF_NULL(leaf->NearestCommonAncestor(target_leaf));
  VLOG(4) << "common ancestor:\n" << common_ancestor;

  // Verify continuity of token ranges between adjacent leaves.
  CHECK(target_leaf->Value().TokensRange().end() ==
        leaf->Value().TokensRange().begin());

  auto* leaf_parent = leaf->Parent();
  {
    // Extend the upper-bound of the previous leaf partition to cover the
    // partition that is about to be removed.
    const auto range_end = leaf->Value().TokensRange().end();
    UpdateTokenRangeUpperBound(target_leaf, &common_ancestor, range_end);
    if (range_end > common_ancestor.Value().TokensRange().end()) {
      common_ancestor.Value().SpanUpToToken(range_end);
    }
    VLOG(5) << "common ancestor (after updating target):\n" << common_ancestor;
    // Shrink lower-bounds of the originating subtree.
    UpdateTokenRangeLowerBound(leaf_parent, &common_ancestor, range_end);
    VLOG(5) << "common ancestor (after updating origin):\n" << common_ancestor;

    // Remove the obsolete partition, leaf.
    // Caution: Existing references to the obsolete partition (and beyond)
    // will be invalidated!
    leaf->RemoveSelfFromParent();
    VLOG(4) << "common ancestor (after merging leaf):\n" << common_ancestor;
  }

  // Sanity check invariants.
  VerifyFullTreeFormatTokenRanges(
      common_ancestor,
      common_ancestor.LeftmostDescendant()->Value().TokensRange().begin());

  return leaf_parent;
}

// Note: this destroys leaf
TokenPartitionTree* MergeLeafIntoNextLeaf(TokenPartitionTree* leaf) {
  VLOG(4) << "origin leaf:\n" << *leaf;
  auto* target_leaf = ABSL_DIE_IF_NULL(leaf)->NextLeaf();
  if (target_leaf == nullptr) return nullptr;
  VLOG(4) << "target leaf:\n" << *target_leaf;

  // If there is no common ancestor, do nothing and return.
  auto& common_ancestor =
      *ABSL_DIE_IF_NULL(leaf->NearestCommonAncestor(target_leaf));
  VLOG(4) << "common ancestor:\n" << common_ancestor;

  // Verify continuity of token ranges between adjacent leaves.
  CHECK(target_leaf->Value().TokensRange().begin() ==
        leaf->Value().TokensRange().end());

  auto* leaf_parent = leaf->Parent();
  {
    // Extend the lower-bound of the next leaf partition to cover the
    // partition that is about to be removed.
    const auto range_begin = leaf->Value().TokensRange().begin();
    UpdateTokenRangeLowerBound(target_leaf, &common_ancestor, range_begin);
    if (range_begin < common_ancestor.Value().TokensRange().begin()) {
      common_ancestor.Value().SpanBackToToken(range_begin);
    }
    VLOG(4) << "common ancestor (after updating target):\n" << common_ancestor;
    // Shrink upper-bounds of the originating subtree.
    UpdateTokenRangeUpperBound(leaf_parent, &common_ancestor, range_begin);
    VLOG(4) << "common ancestor (after updating origin):\n" << common_ancestor;

    // Remove the obsolete partition, leaf.
    // Caution: Existing references to the obsolete partition (and beyond)
    // will be invalidated!
    leaf->RemoveSelfFromParent();
    VLOG(4) << "common ancestor (after destroying leaf):\n" << common_ancestor;
  }

  // Sanity check invariants.
  VerifyFullTreeFormatTokenRanges(
      common_ancestor,
      common_ancestor.LeftmostDescendant()->Value().TokensRange().begin());

  return leaf_parent;
}

//
// TokenPartitionTree class wrapper used by AppendFittingSubpartitions and
// ReshapeFittingSubpartitions for partition reshaping purposes.
//
class TokenPartitionTreeWrapper {
 public:
  TokenPartitionTreeWrapper(const TokenPartitionTree& node) : node_(&node) {}

  // Grouping node with no corresponding TokenPartitionTree node
  TokenPartitionTreeWrapper(const UnwrappedLine& unwrapped_line)
      : node_(nullptr) {
    unwrapped_line_ =
        std::unique_ptr<UnwrappedLine>(new UnwrappedLine(unwrapped_line));
  }

  // At least one of node_ or unwrapped_line_ should not be nullptr
  TokenPartitionTreeWrapper() = delete;

  TokenPartitionTreeWrapper(const TokenPartitionTreeWrapper& other) {
    CHECK((other.node_ != nullptr) || (other.unwrapped_line_ != nullptr));

    if (other.node_) {
      node_ = other.node_;
    } else {
      node_ = nullptr;
      unwrapped_line_ = std::unique_ptr<UnwrappedLine>(
          new UnwrappedLine(*other.unwrapped_line_));
    }
  }

  // Return wrapped node value or concatenation of subnodes values
  const UnwrappedLine& Value() const {
    if (node_) {
      return node_->Value();
    } else {
      return *unwrapped_line_;
    }
  }

  // Concatenate subnodes value with other node value
  UnwrappedLine Value(const TokenPartitionTree& other) const {
    CHECK((node_ == nullptr) && (unwrapped_line_ != nullptr));
    UnwrappedLine uw = *unwrapped_line_;
    uw.SpanUpToToken(other.Value().TokensRange().end());
    return uw;
  }

  // Update concatenated value of subnodes
  void Update(const VectorTree<TokenPartitionTreeWrapper>* child) {
    const auto& token_partition_tree_wrapper = child->Value();
    const auto& uwline = token_partition_tree_wrapper.Value();

    unwrapped_line_->SpanUpToToken(uwline.TokensRange().end());
  }

  // Fix concatenated value indentation
  void SetIndentationSpaces(int indent) {
    CHECK((node_ == nullptr) && (unwrapped_line_ != nullptr));
    unwrapped_line_->SetIndentationSpaces(indent);
  }

  // Return address to wrapped node
  const TokenPartitionTree* Node() const { return node_; }

 private:
  // Wrapped node
  const TokenPartitionTree* node_;

  // Concatenated value of subnodes
  std::unique_ptr<UnwrappedLine> unwrapped_line_;
};

using partition_iterator = std::vector<TokenPartitionTree>::const_iterator;
using partition_range = verible::container_iterator_range<partition_iterator>;

// Reshapes unfitted_partitions and returns result via fitted_partitions.
// Function creates VectorTree<> with additional level of grouping.
// Function expects two partitions: first used for computing indentation
// second with subpartitions, e.g.
// { (>>[<auto>]) @{1,0} // tree root
//   { (>>[function fffffffffff (]) } // first subpartition 'header'
//   { (>>>>>>[<auto>]) @{1,0,1}, // root node with subpartitions 'args'
//     { (>>>>>>[type_a aaaa ,]) }
//     { (>>>>>>[type_b bbbbb ,]) }
//     { (>>>>>>[type_c cccccc ,]) }
//     { (>>>>>>[type_d dddddddd ,]) }
//     { (>>>>>>[type_e eeeeeeee ,]) }
//     { (>>>>>>[type_f ffff ) ;]) }
//   }
// }
// Function is iterating over subpartitions and tries to append
// them if fits in line. Parameter wrap_first_subpartition is used to determine
// whether wrap first subpartition or not.
// Return value signalise whether first subpartition was wrapped.
static bool AppendFittingSubpartitions(
    VectorTree<TokenPartitionTreeWrapper>* fitted_partitions,
    const TokenPartitionTree& unfitted_partitions_header,
    const partition_range& unfitted_partitions_args,
    const BasicFormatStyle& style, bool wrap_first_subpartition) {
  bool wrapped_first_subpartition;

  // first with header
  const auto& header = unfitted_partitions_header;

  // arguments
  const auto& args = unfitted_partitions_args;

  // at least one argument
  CHECK_GE(args.size(), 1);

  // Create first partition group
  // and populate it with function name (e.g. { [function foo (] })
  auto* group = fitted_partitions->NewChild(header.Value());
  auto* child = group->NewChild(header);

  int indent;

  // Try appending first argument
  const auto& first_arg = args[0];
  auto group_value_arg = group->Value().Value(first_arg);
  if (wrap_first_subpartition ||
      (FitsOnLine(group_value_arg, style).fits == false)) {
    // Use wrap indentation
    indent = style.wrap_spaces + group_value_arg.IndentationSpaces();

    // wrap line
    group = group->NewSibling(first_arg.Value());  // start new group
    child = group->NewChild(first_arg);  // append not fitting 1st argument
    group->Value().SetIndentationSpaces(indent);

    // Wrapped first argument
    wrapped_first_subpartition = true;
  } else {
    // Compute new indentation level based on first partition
    const auto& group_value = group->Value().Value();
    const UnwrappedLine& uwline = group_value;
    indent = FitsOnLine(uwline, style).final_column;

    // Append first argument to current group
    child = group->NewChild(args[0]);
    group->Value().Update(child);
    // keep group indentation

    // Appended first argument
    wrapped_first_subpartition = false;
  }

  const auto remaining_args =
      make_container_range(args.begin() + 1, args.end());
  for (const auto& arg : remaining_args) {
    // Every group should have at least one child
    CHECK_GT(group->Children().size(), 0);

    // Try appending current argument to current line
    UnwrappedLine uwline = group->Value().Value(arg);
    if (FitsOnLine(uwline, style).fits) {
      // Fits, appending child
      child = group->NewChild(arg);
      group->Value().Update(child);
    } else {
      // Does not fit, start new group with current child
      group = group->NewSibling(arg.Value());
      child = group->NewChild(arg);
      // no need to update because group was created
      // with current child value

      // Fix group indentation
      group->Value().SetIndentationSpaces(indent);
    }
  }

  return wrapped_first_subpartition;
}

void ReshapeFittingSubpartitions(TokenPartitionTree* node,
                                 const BasicFormatStyle& style) {
  VLOG(4) << __FUNCTION__ << ", partition:\n" << *node;
  VectorTree<TokenPartitionTreeWrapper>* fitted_tree = nullptr;

  // Leaf or simple node, e.g. '[function foo ( ) ;]'
  if (node->Children().size() < 2) {
    // Nothing to do
    return;
  }

  // Partition with arguments should have at least one argument
  const auto& children = node->Children();
  const auto& header = children[0];
  const auto& args = children[1].Children();
  partition_range args_range;
  if (args.empty()) {
    // Partitions with one argument may have been flattened one level.
    args_range = make_container_range(children.begin() + 1, children.end());
  } else {
    // Arguments exist in a nested subpartition.
    args_range = make_container_range(args.begin(), args.end());
  }

  VectorTree<TokenPartitionTreeWrapper> unwrapped_tree(node->Value());
  VectorTree<TokenPartitionTreeWrapper> wrapped_tree(node->Value());

  // Format unwrapped_lines. At first without forced wrap after first line
  bool wrapped_first_token = AppendFittingSubpartitions(
      &unwrapped_tree, header, args_range, style, false);

  if (wrapped_first_token) {
    // First token was forced to wrap so there's no need to
    // generate wrapped version (it has to be wrapped)
    fitted_tree = &unwrapped_tree;
  } else {
    // Generate wrapped version to compare results.
    // Below function passes-trough lines that doesn't fit
    // e.g. very looooooooooong arguments with length over column limit
    // and leaves optimization to line_wrap_searcher.
    // In this approach generated result may not be
    // exactly correct beacause of additional line break done later.
    AppendFittingSubpartitions(&wrapped_tree, header, args_range, style, true);

    // Compare number of grouping nodes
    // If number of grouped node is equal then prefer unwrapped result
    if (unwrapped_tree.Children().size() <= wrapped_tree.Children().size()) {
      fitted_tree = &unwrapped_tree;
    } else {
      fitted_tree = &wrapped_tree;
    }
  }

  // Rebuild TokenPartitionTree
  TokenPartitionTree temporary_tree(node->Value());

  // Iterate over partition groups
  for (const auto& itr : fitted_tree->Children()) {
    auto uwline = itr.Value().Value();
    // Partitions groups should fit in line but we're
    // leaving final decision to ExpandableTreeView
    uwline.SetPartitionPolicy(PartitionPolicyEnum::kFitOnLineElseExpand);

    // Create new grouping node
    auto* group = temporary_tree.NewChild(uwline);

    // Iterate over partitions in group
    for (const auto& partition : itr.Children()) {
      // access partition_node_type
      const auto* node = partition.Value().Node();

      // Append child (warning contains original indentation)
      group->NewChild(*node);
    }
  }

  // Update grouped childrens indentation in case of expanding grouping
  // partitions
  for (auto& group : temporary_tree.Children()) {
    for (auto& subpart : group.Children()) {
      AdjustIndentationAbsolute(&subpart, group.Value().IndentationSpaces());
    }
  }

  // Remove moved nodes
  node->Children().clear();

  // Move back from temporary tree
  node->AdoptSubtreesFrom(&temporary_tree);
}

}  // namespace verible
