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

#include "absl/strings/str_join.h"
#include "common/formatting/format_token.h"
#include "common/formatting/line_wrap_searcher.h"
#include "common/formatting/unwrapped_line.h"
#include "common/strings/display_utils.h"
#include "common/strings/range.h"
#include "common/text/tree_utils.h"
#include "common/util/algorithm.h"
#include "common/util/container_iterator_range.h"
#include "common/util/iterator_adaptors.h"
#include "common/util/logging.h"
#include "common/util/spacer.h"
#include "common/util/top_n.h"
#include "common/util/tree_operations.h"
#include "common/util/value_saver.h"
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

  if (!is_leaf(node)) {
    const auto& children = *node.Children();
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
  ApplyPreOrder(tree, [=](const TokenPartitionTree& node) {
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
  ApplyPreOrder(token_partitions,
                [&partitions](const TokenPartitionTree& node) {
                  if (is_leaf(node)) {  // only look at leaf partitions
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

// FIXME(mglb): Duplicate of function from unwrapped_line.cc. Expose the other
// function and remove this one.
static void TokenFormatter(std::string* out, const PreFormatToken& token,
                           bool verbose) {
  if (verbose) {
    std::ostringstream oss;
    token.before.CompactNotation(oss);
    absl::StrAppend(out, oss.str());
  }
  absl::StrAppend(out, token.Text());
}

// TokenPartitionNode visitor which recursively prints tree in textual form.
// TODO(mglb): Consider merging with TokenPartitionTreePrinter
class TokenPartitionNodePrinter {
  static constexpr int kIndentationSpaces = 2;

 public:
  explicit TokenPartitionNodePrinter(
      std::ostream& stream, bool verbose = false,
      bool print_branch_tokens = false, bool print_node_path = false,
      UnwrappedLine::OriginPrinterFunction origin_printer =
          UnwrappedLine::DefaultOriginPrinter)
      : stream_(stream),
        verbose_(verbose),
        print_branch_tokens_(print_branch_tokens),
        print_node_path_(print_node_path),
        origin_printer_(std::move(origin_printer)),
        indent_(0) {}

  void operator()(const UnwrappedLineNode& node) {
    stream_ << Spacer(indent_) << "{ ";

    if (node.Children().empty()) {
      stream_ << '(';
      node.Value().AsCode(&stream_, verbose_, origin_printer_);
      stream_ << ") }";
    } else {
      stream_ << '('
              // similar to UnwrappedLine::AsCode()
              << Spacer(node.Value().IndentationSpaces(),
                        UnwrappedLine::kIndentationMarker);

      stream_ << "[";
      PrintNodeTokens(node, !node.Children().empty());
      stream_ << "], policy: " << node.Value().PartitionPolicy() << ")";

      if (print_node_path_) {
        const auto* var = TokenPartitionVariant::GetFromStoredObject(&node);
        const auto* tpt = static_cast<const TokenPartitionTree*>(var);
        stream_ << " @" << NodePath(*tpt);
      }

      if (node.Value().Origin() != nullptr) {
        stream_ << ", (origin: ";
        origin_printer_(stream_, node.Value().Origin());
        stream_ << ")";
      }
      stream_ << '\n';

      {
        auto indent_saver = ValueSaver(&indent_, indent_ + kIndentationSpaces);
        for (const auto& child : node.Children()) {
          Visit(*this, child);
          stream_ << '\n';
        }
      }

      stream_ << Spacer(indent_) << "}";
    }
  }

  void operator()(const TokenPartitionChoiceNode& node) {
    stream_ << Spacer(indent_) << "Choice { ("
            << Spacer(node.Value().IndentationSpaces(),
                      UnwrappedLine::kIndentationMarker)
            << "[";
    PrintNodeTokens(node, !node.Choices().empty());
    stream_ << "])";

    if (print_node_path_) {
      const auto* var = TokenPartitionVariant::GetFromStoredObject(&node);
      const auto* tpt = static_cast<const TokenPartitionTree*>(var);
      stream_ << " @" << NodePath(*tpt);
    }

    if (!node.Choices().empty()) {
      stream_ << '\n';
      {
        auto indent_saver = ValueSaver(&indent_, indent_ + kIndentationSpaces);
        for (const auto& child : node.Choices()) {
          Visit(*this, child);
          stream_ << '\n';
        }
      }

      stream_ << Spacer(indent_) << "} // Choice";
    } else {
      stream_ << " }";
    }
  }

  void operator()(const TokenPartitionNode& node) {
    stream_ << Spacer(indent_) << "UnknownNode { ([";
    PrintNodeTokens(node, false);
    stream_ << "]) }";
  }

 private:
  void PrintNodeTokens(const TokenPartitionNode& node, bool is_branch) {
    if (print_branch_tokens_ || !is_branch) {
      stream_ << absl::StrJoin(
          node.Tokens(), " ",
          [=](std::string* out, const PreFormatToken& token) {
            TokenFormatter(out, token, this->verbose_);
          });
    } else {
      stream_ << "<auto>";
    }
  }

  std::ostream& stream_;
  bool verbose_;
  bool print_branch_tokens_;
  bool print_node_path_;
  UnwrappedLine::OriginPrinterFunction origin_printer_;
  int indent_;
};

std::ostream& TokenPartitionTreePrinter::PrintTree(std::ostream& stream,
                                                   int indent) const {
  auto printer =
      TokenPartitionNodePrinter(stream, verbose, false, true, origin_printer);
  Visit(printer, node);
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
  ApplyPreOrder(*ABSL_DIE_IF_NULL(tree), [&](TokenPartitionTree& node) {
    const int new_indent =
        std::max<int>(node.Value().IndentationSpaces() + amount, 0);
    node.Value().SetIndentationSpaces(new_indent);
    if (node.IsChoice()) {
      for (auto& choice : node.Choices()) {
        AdjustIndentationRelative(&choice, amount);
      }
    }
  });
}

void AdjustIndentationAbsolute(TokenPartitionTree* tree, int amount) {
  // Compare the indentation difference at the root node.
  const int indent_diff = amount - tree->Value().IndentationSpaces();
  if (indent_diff == 0) return;
  AdjustIndentationRelative(tree, indent_diff);
}

absl::string_view StringSpanOfTokenRange(const FormatTokenRange& range) {
  if (range.empty()) return absl::string_view();
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

void ApplyAlreadyFormattedPartitionPropertiesToTokens(
    TokenPartitionTree* already_formatted_partition_node,
    std::vector<PreFormatToken>* ftokens) {
  CHECK_NOTNULL(already_formatted_partition_node);
  CHECK_NOTNULL(ftokens);

  VLOG(4) << __FUNCTION__ << ": partition:\n"
          << TokenPartitionTreePrinter(*already_formatted_partition_node, true);

  const auto& uwline = already_formatted_partition_node->Value();
  CHECK_EQ(uwline.PartitionPolicy(), PartitionPolicyEnum::kAlreadyFormatted)
      << *already_formatted_partition_node;
  if (uwline.IsEmpty()) {
    CHECK(is_leaf(*already_formatted_partition_node));
    return;
  }

  auto mutable_tokens_begin =
      ConvertToMutableIterator(uwline.TokensRange().begin(), ftokens->begin());

  // Might be replaced with AppendAligned in the loop below.
  mutable_tokens_begin->before.break_decision =
      verible::SpacingOptions::MustWrap;

  for (auto& child : *already_formatted_partition_node->Children()) {
    auto slice = child.Value();
    if (slice.PartitionPolicy() != PartitionPolicyEnum::kInline) {
      VLOG(1) << "Partition policy is not kInline - ignoring. Parent "
                 "partition:\n"
              << *already_formatted_partition_node;
      continue;
    }

    auto token = verible::ConvertToMutableIterator(slice.TokensRange().begin(),
                                                   ftokens->begin());

    token->before.spaces_required = slice.IndentationSpaces();
    token->before.break_decision = verible::SpacingOptions::AppendAligned;
  }

  auto mutable_tokens_end =
      ConvertToMutableIterator(uwline.TokensRange().end(), ftokens->begin());

  for (auto& token : make_range(mutable_tokens_begin, mutable_tokens_end)) {
    auto& decision = token.before.break_decision;
    if (decision == verible::SpacingOptions::Undecided)
      decision = verible::SpacingOptions::MustAppend;
  }
  // Children are no longer needed
  already_formatted_partition_node->Children()->clear();
  VLOG(4) << __FUNCTION__ << ": partition after:\n"
          << TokenPartitionTreePrinter(*already_formatted_partition_node, true);
}

void MergeConsecutiveSiblings(TokenPartitionTree* tree, size_t pos) {
  CHECK_NOTNULL(tree);
  CHECK_LT(pos + 1, tree->Children()->size());
  const auto& current = tree->Children()->at(pos);
  const auto& next = tree->Children()->at(pos + 1);
  CHECK(current.IsPartition() && !current.IsAlreadyFormatted());
  CHECK(next.IsPartition() && !next.IsAlreadyFormatted());
  // Merge of a non-leaf partition and a leaf partition produces a non-leaf
  // partition with token range wider than concatenated token ranges of its
  // children.
  CHECK(is_leaf(current) == is_leaf(next)) << "left:\n"
                                           << current << "\nright:" << next;
  // Effectively concatenate unwrapped line ranges of sibling subpartitions.
  MergeConsecutiveSiblings(
      *tree, pos, [](UnwrappedLine* left, const UnwrappedLine& right) {
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

TokenPartitionTree* GroupLeafWithPreviousLeaf(TokenPartitionTree* leaf) {
  CHECK_NOTNULL(leaf);
  VLOG(4) << "origin leaf:\n" << *leaf;
  auto* previous_leaf = PreviousLeaf(*leaf);
  if (previous_leaf == nullptr) return nullptr;
  VLOG(4) << "previous leaf:\n" << *previous_leaf;

  // If there is no common ancestor, do nothing and return.
  auto& common_ancestor =
      *ABSL_DIE_IF_NULL(NearestCommonAncestor(*leaf, *previous_leaf));
  VLOG(4) << "common ancestor:\n" << common_ancestor;

  // Verify continuity of token ranges between adjacent leaves.
  CHECK(previous_leaf->Value().TokensRange().end() ==
        leaf->Value().TokensRange().begin());

  auto* leaf_parent = leaf->Parent();
  {
    const auto uwline = leaf->Value();
    const auto range_end = uwline.TokensRange().end();
    const auto previous_uwline = previous_leaf->Value();

    TokenPartitionTree group(previous_uwline);
    group.Children()->reserve(2);
    group.Children()->push_back(std::move(*previous_leaf));
    group.Children()->push_back(std::move(*leaf));

    *previous_leaf = std::move(group);
    // Extend the upper-bound of the group partition to cover the
    // partition that is about to be removed.
    UpdateTokenRangeUpperBound(previous_leaf, &common_ancestor, range_end);

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
    RemoveSelfFromParent(*leaf);
    VLOG(4) << "common ancestor (after merging leaf):\n" << common_ancestor;
  }

  // Sanity check invariants.
  VerifyFullTreeFormatTokenRanges(
      common_ancestor,
      LeftmostDescendant(common_ancestor).Value().TokensRange().begin());

  return previous_leaf;
}

TokenPartitionTree* GroupLeafWithNextLeaf(TokenPartitionTree* leaf) {
  CHECK_NOTNULL(leaf);
  VLOG(4) << "origin leaf:\n" << *leaf;
  auto* next_leaf = NextLeaf(*leaf);
  if (next_leaf == nullptr) return nullptr;
  VLOG(4) << "next leaf:\n" << *next_leaf;

  // If there is no common ancestor, do nothing and return.
  auto& common_ancestor =
      *ABSL_DIE_IF_NULL(NearestCommonAncestor(*leaf, *next_leaf));
  VLOG(4) << "common ancestor:\n" << common_ancestor;

  // Verify continuity of token ranges between adjacent leaves.
  CHECK(leaf->Value().TokensRange().end() ==
        next_leaf->Value().TokensRange().begin());

  auto* leaf_parent = leaf->Parent();
  {
    const auto uwline = leaf->Value();
    const auto range_begin = uwline.TokensRange().begin();
    const auto next_uwline = next_leaf->Value();

    TokenPartitionTree group(next_uwline);
    group.Children()->reserve(2);
    group.Children()->push_back(std::move(*leaf));
    group.Children()->push_back(std::move(*next_leaf));

    *next_leaf = std::move(group);

    UpdateTokenRangeLowerBound(next_leaf, &common_ancestor, range_begin);
    if (range_begin < common_ancestor.Value().TokensRange().begin()) {
      common_ancestor.Value().SpanBackToToken(range_begin);
    }
    VLOG(4) << "common ancestor (after updating target):\n" << common_ancestor;

    // Extend the upper-bound of the group partition to cover the
    // partition that is about to be removed.
    UpdateTokenRangeUpperBound(next_leaf, &common_ancestor, range_begin);
    VLOG(4) << "common ancestor (after updating origin):\n" << common_ancestor;

    // Shrink upper-bounds of the originating subtree.
    UpdateTokenRangeUpperBound(leaf_parent, &common_ancestor, range_begin);

    // Remove the obsolete partition, leaf.
    // Caution: Existing references to the obsolete partition (and beyond)
    // will be invalidated!
    RemoveSelfFromParent(*leaf);
    VLOG(4) << "common ancestor (after destroying leaf):\n" << common_ancestor;
  }

  // Sanity check invariants.
  VerifyFullTreeFormatTokenRanges(
      common_ancestor,
      LeftmostDescendant(common_ancestor).Value().TokensRange().begin());

  return next_leaf;
}

// Note: this destroys leaf
TokenPartitionTree* MergeLeafIntoPreviousLeaf(TokenPartitionTree* leaf) {
  // TODO(mglb): find LeafPartition or Choice or AlreadyFormatted instead of
  // leaf
  CHECK_NOTNULL(leaf);
  VLOG(4) << "origin leaf:\n" << *leaf;
  CHECK(leaf->IsLeafPartition() && !leaf->IsAlreadyFormatted());
  auto* target_leaf = PreviousLeaf(*leaf);
  if (target_leaf == nullptr) return nullptr;
  VLOG(4) << "target leaf:\n" << *target_leaf;
  CHECK(target_leaf->IsLeafPartition() && !target_leaf->IsAlreadyFormatted());

  // If there is no common ancestor, do nothing and return.
  auto& common_ancestor =
      *ABSL_DIE_IF_NULL(NearestCommonAncestor(*leaf, *target_leaf));
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
    RemoveSelfFromParent(*leaf);
    VLOG(4) << "common ancestor (after merging leaf):\n" << common_ancestor;
  }

  // Sanity check invariants.
  VerifyFullTreeFormatTokenRanges(
      common_ancestor,
      LeftmostDescendant(common_ancestor).Value().TokensRange().begin());

  return leaf_parent;
}

// Note: this destroys leaf
TokenPartitionTree* MergeLeafIntoNextLeaf(TokenPartitionTree* leaf) {
  CHECK_NOTNULL(leaf);
  VLOG(4) << "origin leaf:\n" << *leaf;
  CHECK(leaf->IsLeafPartition() && !leaf->IsAlreadyFormatted());
  auto* target_leaf = NextLeaf(*leaf);
  if (target_leaf == nullptr) return nullptr;
  VLOG(4) << "target leaf:\n" << *target_leaf;
  CHECK(target_leaf->IsLeafPartition() && !target_leaf->IsAlreadyFormatted());

  // If there is no common ancestor, do nothing and return.
  auto& common_ancestor =
      *ABSL_DIE_IF_NULL(NearestCommonAncestor(*leaf, *target_leaf));
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
    RemoveSelfFromParent(*leaf);
    VLOG(4) << "common ancestor (after destroying leaf):\n" << common_ancestor;
  }

  // Sanity check invariants.
  VerifyFullTreeFormatTokenRanges(
      common_ancestor,
      LeftmostDescendant(common_ancestor).Value().TokensRange().begin());

  return leaf_parent;
}

//
// TokenPartitionTree class wrapper used by AppendFittingSubpartitions and
// ReshapeFittingSubpartitions for partition reshaping purposes.
// These wrappers take single-argument constructors to implicitly convert
// to this wrapper.
//
class TokenPartitionTreeWrapper {
 public:
  /* implicit */ TokenPartitionTreeWrapper(  // NOLINT
      const TokenPartitionTree& node)
      : node_(&node) {}

  // Grouping node with no corresponding TokenPartitionTree node
  /* implicit */ TokenPartitionTreeWrapper(  // NOLINT
      const UnwrappedLine& unwrapped_line)
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

using partition_iterator = TokenPartitionTree::subnodes_type::const_iterator;
using partition_range = verible::container_iterator_range<partition_iterator>;

struct AppendFittingSubpartitionsResult {
  // Indicates that wrapped style has been used.
  bool wrapped;
  // Length of longest line (including indent) in resulting tree. Might be
  // inaccurate when passed subpartitions contain forced line breaks.
  int longest_line_len;
};

// Builds new tree from passed partitions in one of two styles described below.
// The tree is built on `fitted_partitions` node. `header` and at least one
// partition in `subpartitions` are required; `trailer` partition is optional.
// `one_per_line` flag forces line break after each subpartition.
//
// Unwrapped style:
// Used when the header and the first subpartition fit on one line and
// `wrap_first_subpartiton` is false.
//
// All but first line use indent equal to `header` width. `trailer` is appended
// to the last subpartition.
//
// <HEADER><SUBPARTITION 0><SUBPARTITION 1>
//         <SUBPARTITION 2><SUBPARTITION 3>
//         <SUBPARTITION 3><TRAILER>
//
// Wrapped style:
// Line break is forced before first and after last partition. Lines with
// subpartitions use subpartition's existing indent.
//
// <HEADER>
//     <SUBPARTITION 0><SUBPARTITION 1><SUBPARTITION 2>
//     <SUBPARTITION 3><SUBPARTITION 3>
// <TRAILER>
static AppendFittingSubpartitionsResult AppendFittingSubpartitions(
    VectorTree<TokenPartitionTreeWrapper>* fitted_partitions,
    const TokenPartitionTree& header, const partition_range& subpartitions,
    const TokenPartitionTree* trailer, const BasicFormatStyle& style,
    bool one_per_line, bool wrap_first_subpartition) {
  // at least one argument
  CHECK_GE(subpartitions.size(), 1);

  // Create first partition group
  // and populate it with function name (e.g. { [function foo (] })
  fitted_partitions->Children().emplace_back(header.Value());
  auto* group = &fitted_partitions->Children().back();
  group->Children().emplace_back(header);

  int indent;

  // Try appending first argument

  const TokenPartitionTree& first_arg = subpartitions.front();
  verible::UnwrappedLine first_line = group->Value().Value(first_arg);
  if (trailer && subpartitions.size() == 1) {
    first_line.SpanUpToToken(trailer->Value().TokensRange().end());
  }

  int longest_line_len = 0;

  verible::FitResult fit_result = FitsOnLine(first_line, style);
  const bool wrapped_first_subpartition =
      wrap_first_subpartition || !fit_result.fits;
  if (!wrapped_first_subpartition) {
    // Compute new indentation level based on first partition
    const UnwrappedLine& uwline = group->Value().Value();
    indent = FitsOnLine(uwline, style).final_column;

    // Append first argument to current group
    group->Children().emplace_back(subpartitions.front());
    group->Value().Update(&group->Children().back());
    // keep group indentation

    longest_line_len = fit_result.final_column;
  } else {
    // Measure header
    fit_result = FitsOnLine(group->Value().Value(), style);
    longest_line_len = std::max(longest_line_len, fit_result.final_column);

    // Use original indentation of the subpartition
    indent = first_arg.Value().IndentationSpaces();
    // wrap line
    auto& siblings = group->Parent()->Children();
    siblings.emplace_back(first_arg.Value());
    group = &siblings.back();
    group->Children().emplace_back(first_arg);
    group->Value().SetIndentationSpaces(indent);

    // Measure first wrapped line
    fit_result = FitsOnLine(group->Value().Value(), style);
    longest_line_len = std::max(longest_line_len, fit_result.final_column);
  }

  const auto remaining_args =
      make_container_range(subpartitions.begin() + 1, subpartitions.end());
  for (const auto& arg : remaining_args) {
    // Every group should have at least one child
    CHECK_GT(group->Children().size(), 0);

    if (!one_per_line) {
      // Try appending current argument to current line
      UnwrappedLine uwline = group->Value().Value(arg);
      if (trailer && !wrapped_first_subpartition &&
          (&arg == &remaining_args.back())) {
        uwline.SpanUpToToken(trailer->Value().TokensRange().end());
      }

      fit_result = FitsOnLine(uwline, style);
      if (fit_result.fits) {
        // Fits, appending child
        group->Children().emplace_back(arg);
        group->Value().Update(&group->Children().back());
        longest_line_len = std::max(longest_line_len, fit_result.final_column);
        continue;
      }
    }

    // Forced one per line or does not fit, start new group with current child
    auto& siblings = group->Parent()->Children();
    siblings.emplace_back(arg.Value());
    group = &siblings.back();
    group->Children().emplace_back(arg);
    // no need to update because group was created
    // with current child value

    // Fix group indentation
    group->Value().SetIndentationSpaces(indent);

    fit_result = FitsOnLine(group->Value().Value(), style);
    longest_line_len = std::max(longest_line_len, fit_result.final_column);
  }
  if (trailer) {
    if (wrapped_first_subpartition) {
      auto& siblings = group->Parent()->Children();
      siblings.emplace_back(trailer->Value());
      group = &siblings.back();
      group->Children().emplace_back(*trailer);
      group->Value().SetIndentationSpaces(first_line.IndentationSpaces());
    } else {
      group->Children().emplace_back(*trailer);
      group->Value().Update(&group->Children().back());
    }
    fit_result = FitsOnLine(group->Value().Value(), style);
    longest_line_len = std::max(longest_line_len, fit_result.final_column);
  }

  return {wrapped_first_subpartition, longest_line_len};
}

// Reshapes the tree pointed to by `node` using `AppendFittingSubpartitions()`
// function. Function creates VectorTree<> with additional level of grouping for
// each created line. Function expects at least two partitions: first one
// ("header") is used for computing indentation, the second ("subpartitions")
// should contain subpartitions to be appended and aligned. Optional third
// partition ("trailer") is appended to the last subpartition or placed in a new
// line with the same indent as the header. Example input tree:
//
// { (>>[...], policy: append-fitting-sub-partitions)  // `node` tree
//   { (>>[string seq_names [ ] = {]) }                // header
//   { (>>>>[...], policy: always-expand)              // subpartitions
//     { (>>>>["uart_sanity_vseq" ,]) }
//     ...
//     { (>>>>["uart_loopback_vseq"]) }
//   }
//   { (>>[} ;]) }                                     // trailer
// }
//
// When "subpartitions" group has kAlwaysExpand policy, line break is forced
// between each subpartition from the group.
void ReshapeFittingSubpartitions(const BasicFormatStyle& style,
                                 TokenPartitionTree* node) {
  VLOG(4) << __FUNCTION__ << ", before:\n" << *node;
  VectorTree<TokenPartitionTreeWrapper>* fitted_tree = nullptr;

  // Leaf or simple node, e.g. '[function foo ( ) ;]'
  if (node->Children()->size() < 2) {
    // Nothing to do
    return;
  }

  // Partition with arguments should have at least one argument
  const auto& children = *node->Children();
  const auto& header = children[0];
  const auto& args_partition = children[1];
  const auto& subpartitions = *args_partition.Children();
  const auto* trailer = children.size() > 2 ? &children[2] : nullptr;

  const bool one_per_line = args_partition.Value().PartitionPolicy() ==
                            PartitionPolicyEnum::kAlwaysExpand;

  partition_range args_range;
  if (subpartitions.empty()) {
    // Partitions with one argument may have been flattened one level.
    args_range =
        make_container_range(children.begin() + 1, children.begin() + 2);
  } else {
    // Arguments exist in a nested subpartition.
    args_range =
        make_container_range(subpartitions.begin(), subpartitions.end());
  }

  VectorTree<TokenPartitionTreeWrapper> unwrapped_tree(node->Value());
  VectorTree<TokenPartitionTreeWrapper> wrapped_tree(node->Value());

  // Format unwrapped_lines. At first without forced wrap after first line
  const auto result = AppendFittingSubpartitions(
      &unwrapped_tree, header, args_range, trailer, style, one_per_line, false);

  if (result.wrapped && result.longest_line_len < style.column_limit) {
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
    const auto wrapped_result = AppendFittingSubpartitions(
        &wrapped_tree, header, args_range, trailer, style, one_per_line, true);

    // Avoid exceeding column limit if possible
    if (result.longest_line_len > style.column_limit &&
        wrapped_result.longest_line_len <= style.column_limit) {
      fitted_tree = &wrapped_tree;
    } else {
      // Compare number of grouping nodes
      // If number of grouped node is equal then prefer unwrapped result
      if (unwrapped_tree.Children().size() <= wrapped_tree.Children().size()) {
        fitted_tree = &unwrapped_tree;
      } else {
        fitted_tree = &wrapped_tree;
      }
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
    temporary_tree.Children()->emplace_back(uwline);
    auto* group = &temporary_tree.Children()->back();

    // Iterate over partitions in group
    for (const auto& partition : itr.Children()) {
      // access partition_node_type
      const auto* node = partition.Value().Node();

      // Append child (warning contains original indentation)
      group->Children()->push_back(*node);
    }
  }

  // Update grouped childrens indentation in case of expanding grouping
  // partitions
  for (auto& group : *temporary_tree.Children()) {
    for (auto& subpart : *group.Children()) {
      AdjustIndentationAbsolute(&subpart, group.Value().IndentationSpaces());
    }
  }

  // Remove moved nodes
  node->Children()->clear();

  // Move back from temporary tree
  AdoptSubtreesFrom(*node, &temporary_tree);
  VLOG(4) << __FUNCTION__ << ", after:\n" << *node;
}

}  // namespace verible
