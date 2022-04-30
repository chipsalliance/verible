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

#ifndef VERIBLE_COMMON_FORMATTING_TOKEN_PARTITION_TREE_H_
#define VERIBLE_COMMON_FORMATTING_TOKEN_PARTITION_TREE_H_

#include <cstddef>
#include <iosfwd>
#include <utility>
#include <vector>

#include "common/formatting/basic_format_style.h"
#include "common/formatting/format_token.h"
#include "common/formatting/unwrapped_line.h"
#include "common/strings/position.h"  // for ByteOffsetSet
#include "common/util/container_iterator_range.h"
#include "common/util/container_proxy.h"
#include "common/util/variant.h"
#include "common/util/vector_tree.h"

namespace verible {

class TokenPartitionNode;
class TokenPartitionBranchNode;

class UnwrappedLineNode;
class TokenPartitionChoiceNode;

class TokenPartitionTree;

// Base node classes with common functionality
// ===========================================

// Base class for all TokenPartition nodes.
class TokenPartitionNode {
 public:
  // Constructors & assignment operators

  TokenPartitionNode() = default;
  TokenPartitionNode(const TokenPartitionNode&) = default;
  TokenPartitionNode(TokenPartitionNode&&) = default;

  TokenPartitionNode& operator=(const TokenPartitionNode& rhs) {
    if (this == &rhs) {
      return *this;
    }

    // Intentionally keeping current parent
    value_ = rhs.value_;
    return *this;
  }

  TokenPartitionNode& operator=(TokenPartitionNode&& rhs) noexcept {
    // Intentionally keeping current parent
    value_ = rhs.value_;
    return *this;
  }

  //

  TokenPartitionTree* Parent() { return parent_; }
  const TokenPartitionTree* Parent() const { return parent_; }

  FormatTokenRange Tokens() const { return value_.TokensRange(); }

 protected:
  void SetParent(TokenPartitionTree* parent) { parent_ = parent; }

  static void SetParent(TokenPartitionNode& node, TokenPartitionTree* parent) {
    node.parent_ = parent;
  }

  // Value-related interface
  // -----------------------

  explicit TokenPartitionNode(const UnwrappedLine& uwline) : value_(uwline) {}

  UnwrappedLine& Value() { return value_; }
  const UnwrappedLine& Value() const { return value_; }

 private:
  TokenPartitionTree* parent_ = nullptr;
  // TODO(mglb): this is here just to provide Tokens(), which is a common
  // thing in all nodes. Break UnwrappedLine into separate node members and
  // move node-specific ones to appropriate subnodes.
  UnwrappedLine value_;
};

// Base class for TokenPartition nodes providing child nodes list.
class TokenPartitionBranchNode : public TokenPartitionNode {
 public:
  using subnodes_type = std::vector<TokenPartitionTree>;

  // Constructors & assignment operators

  TokenPartitionBranchNode() = default;
  TokenPartitionBranchNode(const TokenPartitionBranchNode& other) = default;
  TokenPartitionBranchNode(TokenPartitionBranchNode&& other) = default;

  TokenPartitionBranchNode& operator=(const TokenPartitionBranchNode& other) =
      default;

  TokenPartitionBranchNode& operator=(TokenPartitionBranchNode&& other) =
      default;

  //

  class ChildrenList : private ContainerProxyBase<ChildrenList, subnodes_type> {
    using Base = ContainerProxyBase<ChildrenList, subnodes_type>;
    friend Base;

   public:
    using typename Base::container_type;

    // Import (via `using`) ContainerProxy members supported by std::vector.
    USING_CONTAINER_PROXY_STD_VECTOR_MEMBERS(Base)

    // Move-cast to wrapped container's type. Moves out the container.
    explicit operator container_type() && { return std::move(container_); }

   protected:
    // ContainerProxy interface
    // ------------------------

    container_type& underlying_container() { return container_; }
    const container_type& underlying_container() const { return container_; }

    void ElementsInserted(iterator first, iterator last) {
      LinkChildrenToParent(iterator_range(first, last));
    }

    // void ElementsBeingRemoved(iterator first, iterator last) {}

    // void ElementsBeingReplaced() {}

    void ElementsWereReplaced() { LinkChildrenToParent(container_); }

   private:
    // Sets parent pointer of nodes from `children` range to address of `node_`.
    template <class Range>
    void LinkChildrenToParent(Range&& children);

    // Allow construction, assignment and direct access to `container_` inside
    // TokenPartitionBranchNode.
    friend TokenPartitionBranchNode;

    ChildrenList() = default;

    ChildrenList(const ChildrenList& other) : container_(other.container_) {
      LinkChildrenToParent(container_);
    }

    ChildrenList(ChildrenList&& other) noexcept
        : container_(std::move(other.container_)) {
      // Note: `other` is not notified about the change because it ends up in
      // undefined state as a result of the move.
      LinkChildrenToParent(container_);
    }

    ChildrenList& operator=(const ChildrenList& other) {
      container_ = other.container_;
      LinkChildrenToParent(container_);
      return *this;
    }

    ChildrenList& operator=(ChildrenList&& other) noexcept {
      // Note: `other` is not notified about the change because it ends up in
      // undefined state as a result of the move.
      container_ = std::move(other.container_);
      LinkChildrenToParent(container_);
      return *this;
    }

    // Actual data container where child nodes are stored.
    subnodes_type container_;
  };

  //

  ChildrenList& Children() { return children_; };
  const ChildrenList& Children() const { return children_; };

  //

 protected:
  explicit TokenPartitionBranchNode(const UnwrappedLine& uwline)
      : TokenPartitionNode(uwline) {}

  //

 private:
  static constexpr size_t ChildrenMemberOffset() {
    // TODO(mglb): Explain this (copy-paste related comment from map_tree.h)
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#endif
    return offsetof(TokenPartitionBranchNode, children_);
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
  }

  ChildrenList children_;
};

// Concrete node classes
// =====================

class UnwrappedLineNode : public TokenPartitionBranchNode {
 public:
  UnwrappedLineNode() = default;
  UnwrappedLineNode(const UnwrappedLineNode&) = default;
  UnwrappedLineNode(UnwrappedLineNode&&) = default;

  UnwrappedLineNode& operator=(const UnwrappedLineNode&) = default;
  UnwrappedLineNode& operator=(UnwrappedLineNode&&) = default;

  explicit UnwrappedLineNode(const UnwrappedLine& uwline)
      : TokenPartitionBranchNode(uwline) {}

  using TokenPartitionNode::Value;
};

class TokenPartitionChoiceNode : public TokenPartitionNode {
  using subnodes_type = std::vector<TokenPartitionTree>;

 public:
  TokenPartitionChoiceNode() = default;
  TokenPartitionChoiceNode(const TokenPartitionChoiceNode&) = default;
  TokenPartitionChoiceNode(TokenPartitionChoiceNode&&) = default;

  TokenPartitionChoiceNode& operator=(const TokenPartitionChoiceNode&) =
      default;
  TokenPartitionChoiceNode& operator=(TokenPartitionChoiceNode&&) = default;

  explicit TokenPartitionChoiceNode(const UnwrappedLine& uwline)
      : TokenPartitionNode(uwline) {}

  class ChoicesList : ContainerProxyBase<ChoicesList, subnodes_type> {
    using Base = ContainerProxyBase<ChoicesList, subnodes_type>;
    friend Base;

   public:
    using typename Base::container_type;

    // Import (via `using`) ContainerProxy members supported by std::vector.
    USING_CONTAINER_PROXY_STD_VECTOR_MEMBERS(Base)

    // Move-cast to wrapped container's type. Moves out the container.
    explicit operator subnodes_type() && { return std::move(container_); }

   protected:
    // ContainerProxy interface

    container_type& underlying_container() { return container_; }
    const container_type& underlying_container() const { return container_; }

    void ElementsInserted(iterator first, iterator last) {
      VerifyAndProcessNewChoices(iterator_range(first, last));
    }

    // void ElementsBeingRemoved(iterator first, iterator last) {}

    // void ElementsBeingReplaced() {}

    void ElementsWereReplaced() { VerifyAndProcessNewChoices(container_); }

   private:
    template <class Range>
    void VerifyAndProcessNewChoices(Range&& choice_subtrees);

    // Allow assignment inside TokenPartitionChoiceNode.
    friend TokenPartitionChoiceNode;

    ChoicesList() = default;

    ChoicesList(const ChoicesList& other) : container_(other.container_) {
      VerifyAndProcessNewChoices(container_);
    }

    ChoicesList(ChoicesList&& other) noexcept
        : container_(std::move(other.container_)) {
      // Note: `other` is not notified about the change because it ends up in
      // undefined state as a result of the move.
      VerifyAndProcessNewChoices(container_);
    }

    ChoicesList& operator=(const ChoicesList& other) {
      container_ = other.container_;
      VerifyAndProcessNewChoices(container_);
      return *this;
    }

    ChoicesList& operator=(ChoicesList&& other) noexcept {
      // Note: `other` is not notified about the change because it ends up in
      // undefined state as a result of the move.
      container_ = std::move(other.container_);
      VerifyAndProcessNewChoices(container_);
      return *this;
    }

    // Actual data container where choice subtrees are stored.
    subnodes_type container_;
  };

  ChoicesList& Choices() { return choices_; }
  const ChoicesList& Choices() const { return choices_; }

  using TokenPartitionNode::Value;

 private:
  static constexpr size_t ChoicesMemberOffset() {
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#endif
    return offsetof(TokenPartitionChoiceNode, choices_);
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
  }

  ChoicesList choices_;
};

// TokenPartitionTreePrinter
// =========================

// Custom printer, alternative to the default stream operator<<.
// Modeled after VectorTree<>::PrintTree, but suppresses printing of the
// tokens for non-leaf nodes because a node's token range always spans
// that of all of its children.
// Usage: stream << TokenPartitionTreePrinter(tree) << std::endl;
struct TokenPartitionTreePrinter {
  explicit TokenPartitionTreePrinter(
      const TokenPartitionTree& n, bool verbose = false,
      UnwrappedLine::OriginPrinterFunction origin_printer =
          UnwrappedLine::DefaultOriginPrinter)
      : node(n), verbose(verbose), origin_printer(std::move(origin_printer)) {}

  std::ostream& PrintTree(std::ostream& stream, int indent = 0) const;

  // The (sub)tree to display.
  const TokenPartitionTree& node;
  // If true, display inter-token information.
  bool verbose;

  UnwrappedLine::OriginPrinterFunction origin_printer;
};

std::ostream& operator<<(std::ostream& stream,
                         const TokenPartitionTreePrinter& printer);

// TokenPartitionTree
// ==================

using TokenPartitionVariant =
    verible::Variant<UnwrappedLineNode, TokenPartitionChoiceNode>;

// Opaque type alias for a hierarchically partitioned format token stream.
// Objects of this type maintain the following invariants:
//   1) The format token range spanned by any tree node (UnwrappedLine) is
//      equal to that of its children.
//   2) Adjacent siblings begin/end iterators are equal (continuity).
//
class TokenPartitionTree : public TokenPartitionVariant {
 public:
  // Type alias for TreeNode interface
  using subnodes_type = TokenPartitionBranchNode::subnodes_type;

  // TODO(mglb): replace using class hierarchy
  enum class Kind : int8_t {
    kPartition,
    kChoice,
  };

  friend std::ostream& operator<<(std::ostream& s, Kind kind) {
    switch (kind) {
      case Kind::kPartition:
        return s << "partition";
      case Kind::kChoice:
        return s << "choice";
      default:
        return s << "<unknown>";
    }
  }

  TokenPartitionTree() = default;

  explicit TokenPartitionTree(Kind kind)
      : TokenPartitionVariant(
            (kind == Kind::kPartition)
                ? TokenPartitionVariant(in_place_type<UnwrappedLineNode>)
                : TokenPartitionVariant(
                      in_place_type<TokenPartitionChoiceNode>)) {}

  TokenPartitionTree(const TokenPartitionTree& other) = default;

  TokenPartitionTree(TokenPartitionTree&& other) = default;

  // This constructor can be used to recursively build trees.
  // e.g.
  //   // looks like function-call, but just invokes constructor:
  //   typedef TokenPartitionTree<Foo> FooNode;
  //   auto foo_tree = FooNode({value-initializer},
  //        FooNode({value-initializer}, /* children nodes... */ ),
  //        FooNode({value-initializer}, /* children nodes... */ )
  //   );
  template <typename... Args>
  explicit TokenPartitionTree(const UnwrappedLine& v, Args&&... args)
      : TokenPartitionVariant(in_place_type<UnwrappedLineNode>, v) {
    auto& node = get<UnwrappedLineNode>(*this);
    node.Children().reserve(sizeof...(args));
    (node.Children().emplace_back(std::forward<Args>(args)), ...);
  }

  template <typename... Args>
  explicit TokenPartitionTree(UnwrappedLine&& v, Args&&... args)
      : TokenPartitionVariant(in_place_type<UnwrappedLineNode>, v) {
    auto& node = get<UnwrappedLineNode>(*this);
    node.Children().reserve(sizeof...(args));
    (node.Children().emplace_back(std::forward<Args>(args)), ...);
  }

  ~TokenPartitionTree() {}

  // Swaps values and subtrees of two nodes.
  // This operation is safe for unrelated trees (no common ancestor).
  // This operation is safe when the two nodes share a common ancestor,
  // excluding the case where one node is a direct ancestor of the other.
  // TODO(fangism): Add a proper check for this property, and test.
  using TokenPartitionVariant::swap;

  TokenPartitionTree& operator=(const TokenPartitionTree& source) {
    TokenPartitionVariant::operator=(source);
    return *this;
  }

  TokenPartitionTree& operator=(TokenPartitionTree&& source) noexcept {
    TokenPartitionVariant::operator=(std::move(source));
    return *this;
  }

  // TreeNode interface:

  UnwrappedLine& Value() {
    return Visit([](auto& node) -> UnwrappedLine& { return node.Value(); },
                 *this);
  }
  const UnwrappedLine& Value() const {
    return Visit(
        [](const auto& node) -> const UnwrappedLine& { return node.Value(); },
        *this);
  }

  TokenPartitionTree* Parent() {
    return Visit([](TokenPartitionNode& node) { return node.Parent(); }, *this);
  }
  const TokenPartitionTree* Parent() const {
    return Visit([](const TokenPartitionNode& node) { return node.Parent(); },
                 *this);
  }

  TokenPartitionBranchNode::ChildrenList* Children() {
    using ChildrenList = TokenPartitionBranchNode::ChildrenList;
    return Visit(
        Overload{
            [](TokenPartitionBranchNode& node) -> ChildrenList* {
              return &node.Children();
            },
            [](TokenPartitionNode& node) -> ChildrenList* { return nullptr; },
        },
        *this);
  }
  const TokenPartitionBranchNode::ChildrenList* Children() const {
    using ChildrenList = TokenPartitionBranchNode::ChildrenList;
    return Visit(
        Overload{
            [](const TokenPartitionBranchNode& node) -> const ChildrenList* {
              return &node.Children();
            },
            [](const TokenPartitionNode& node) -> const ChildrenList* {
              return nullptr;
            },
        },
        *this);
  }

  // Choice node interface

  auto& Choices() {
    using ChoicesList = TokenPartitionChoiceNode::ChoicesList;
    return *Visit(Overload{
                      [](TokenPartitionChoiceNode& node) -> ChoicesList* {
                        return &node.Choices();
                      },
                      [](TokenPartitionNode& node) -> ChoicesList* {
                        LOG(FATAL) << "Unexpected node type";
                        return nullptr;
                      },
                  },
                  *this);
  }

  const auto& Choices() const {
    using ChoicesList = TokenPartitionChoiceNode::ChoicesList;
    return *Visit(
        Overload{
            [](const TokenPartitionChoiceNode& node) -> const ChoicesList* {
              return &node.Choices();
            },
            [](const TokenPartitionNode& node) -> const ChoicesList* {
              LOG(FATAL) << "Unexpected node type";
              return nullptr;
            },
        },
        *this);
  }

  // Node type checks:

  Kind kind() const {
    return Visit(
        Overload{
            [](const TokenPartitionChoiceNode&) { return Kind::kChoice; },
            [](const UnwrappedLineNode&) { return Kind::kPartition; },
        },
        *this);
  }

  bool IsChoice() const { return kind() == Kind::kChoice; }

  bool IsPartition() const { return kind() == Kind::kPartition; }

  bool IsLeafPartition() const {
    return kind() == Kind::kPartition && Children()->empty();
  }

  bool IsAlreadyFormatted() const {
    return kind() == Kind::kPartition &&
           Value().PartitionPolicy() == PartitionPolicyEnum::kAlreadyFormatted;
  }
};

// Node implementations
// ====================

template <class Range>
void TokenPartitionBranchNode::ChildrenList::LinkChildrenToParent(
    Range&& children) {
  // FIXME(mglb): Fragile magic - document!

  static constexpr size_t offset_in_node =
      TokenPartitionBranchNode::ChildrenMemberOffset();

  std::byte* const this_ptr = reinterpret_cast<std::byte*>(this);
  std::byte* const node_ptr = this_ptr - offset_in_node;

  TokenPartitionBranchNode* const node =
      reinterpret_cast<TokenPartitionBranchNode*>(node_ptr);

  TokenPartitionTree* variant = std::launder(static_cast<TokenPartitionTree*>(
      TokenPartitionTree::GetFromStoredObject(node)));

  for (auto& child : children) {
    Visit(
        [variant](TokenPartitionNode& node) {
          TokenPartitionNode::SetParent(node, variant);
        },
        child);
  }
}

template <class Range>
void TokenPartitionChoiceNode::ChoicesList::VerifyAndProcessNewChoices(
    Range&& choice_subtrees) {
  // FIXME(mglb): Fragile magic - document!

  static constexpr size_t offset_in_node =
      TokenPartitionChoiceNode::ChoicesMemberOffset();

  std::byte* const this_ptr = reinterpret_cast<std::byte*>(this);
  std::byte* const node_ptr = this_ptr - offset_in_node;

  TokenPartitionChoiceNode* const node =
      std::launder(reinterpret_cast<TokenPartitionChoiceNode*>(node_ptr));

  for (auto& subtree : choice_subtrees) {
    Visit(
        [node](TokenPartitionNode& subnode) {
          CHECK(subnode.Tokens() == node->Tokens())
              << "Each choice subtree must cover the same tokens as the Choice "
                 "node.";
          TokenPartitionNode::SetParent(subnode, nullptr);
        },
        subtree);
  }
}

//

// using TokenPartitionTree = VectorTree<UnwrappedLine>;
using TokenPartitionIterator = TokenPartitionTree::subnodes_type::iterator;
using TokenPartitionRange = container_iterator_range<TokenPartitionIterator>;

// Analyses (non-modifying):

// Verifies the invariant properties of TokenPartitionTree at a single node.
// The 'base' argument is used to calculate iterator distances relative to the
// start of the format token array that is the basis for UnwrappedLine token
// ranges.  This function fails with a fatal-error like CHECK() if the
// invariants do not hold true.
void VerifyTreeNodeFormatTokenRanges(
    const TokenPartitionTree& node,
    std::vector<PreFormatToken>::const_iterator base);

// Verifies TokenPartitionTree invariants at every node in the tree,
// which covers and entire hierarchical partition of format tokens.
void VerifyFullTreeFormatTokenRanges(
    const TokenPartitionTree& tree,
    std::vector<PreFormatToken>::const_iterator base);

// Returns the largest leaf partitions of tokens, ordered by number of tokens
// spanned.
std::vector<const UnwrappedLine*> FindLargestPartitions(
    const TokenPartitionTree& token_partitions, size_t num_partitions);

// Compute (original_spacing - spaces_required) for every format token,
// except the first token in each partition.
// A perfectly formatted (flushed-left) original spacing will return all zeros.
std::vector<std::vector<int>> FlushLeftSpacingDifferences(
    const TokenPartitionRange& partitions);

// Returns true if any substring range spanned by the token partition 'range' is
// disabled by 'disabled_byte_ranges'.
// 'full_text' is the original string_view that spans the text from which
// tokens were lexed, and is used in byte-offset calculation.
bool AnyPartitionSubRangeIsDisabled(TokenPartitionRange range,
                                    absl::string_view full_text,
                                    const ByteOffsetSet& disabled_byte_ranges);

// Return ranges of subpartitions separated by blank lines.
// This does not modify the partition, but does return ranges of mutable
// iterators of partitions.
std::vector<TokenPartitionRange> GetSubpartitionsBetweenBlankLines(
    const TokenPartitionRange&);

// Transformations (modifying):

// Adds or removes a constant amount of indentation from entire token
// partition tree.  Relative indentation amount may be positive or negative.
// Final indentation will be at least 0, and never go negative.
void AdjustIndentationRelative(TokenPartitionTree* tree, int amount);

// Adjusts indentation to align root of partition tree to new indentation
// amount.
void AdjustIndentationAbsolute(TokenPartitionTree* tree, int amount);

// Returns the range of text spanned by tokens range.
absl::string_view StringSpanOfTokenRange(const FormatTokenRange& range);

// Mark ranges of tokens (corresponding to formatting-disabled lines) to
// have their original spacing preserved, except allow the first token
// to follow the formatter's calculated indentation.
void IndentButPreserveOtherSpacing(TokenPartitionRange partition_range,
                                   absl::string_view full_text,
                                   std::vector<PreFormatToken>* ftokens);

// Finalizes formatting of a partition with kAlreadyFormatted policy and
// optional kTokenModifer subpartitions.
// Spacing described by the partitions is applied to underlying tokens. All
// subpartitions of the passed node are removed.
void ApplyAlreadyFormattedPartitionPropertiesToTokens(
    TokenPartitionTree* already_formatted_partition_node,
    std::vector<PreFormatToken>* ftokens);

// Merges the two subpartitions of tree at index pos and pos+1.
void MergeConsecutiveSiblings(TokenPartitionTree* tree, size_t pos);

// Groups this leaf with the leaf partition that preceded it, which could be
// a distant relative. The grouping node is created in place of the preceding
// leaf.
// Both grouped partitions retain their indentation level and partition
// policies. The group partition's indentation level and partition policy is
// copied from the first partition in the group.
// Returns the grouped partition if operation succeeded, else nullptr.
TokenPartitionTree* GroupLeafWithPreviousLeaf(TokenPartitionTree* leaf);

// Merges this leaf into the leaf partition that preceded it, which could be
// a distant relative.  The leaf is destroyed in the process.
// The destination partition retains its indentation level and partition
// policies, but those of the leaf are discarded.
// (If you need that information, save it before moving the leaf.)
// Returns the parent of the leaf partition that was moved if the move
// occurred, else nullptr.
TokenPartitionTree* MergeLeafIntoPreviousLeaf(TokenPartitionTree* leaf);

// Merges this leaf into the leaf partition that follows it, which could be
// a distant relative.  The leaf is destroyed in the process.
// The destination partition retains its indentation level and partition
// policies, but those of the leaf are discarded.
// (If you need that information, save it before moving the leaf.)
// Returns the parent of the leaf partition that was moved if the move
// occurred, else nullptr.
TokenPartitionTree* MergeLeafIntoNextLeaf(TokenPartitionTree* leaf);

// Evaluates two partitioning schemes wrapped and appended first
// subpartition. Then reshapes node tree according to scheme with less
// grouping nodes (if both have same number of grouping nodes uses one
// with appended first subpartition).
//
// Example input:
// --------------
// { (>>[<auto>]) @{1,0}, policy: append-fitting-sub-partitions
//   { (>>[function fffffffffff (]) }
//   { (>>>>>>[<auto>]) @{1,0,1}, policy: fit-else-expand
//     { (>>>>>>[type_a aaaa ,]) }
//     { (>>>>>>[type_b bbbbb ,]) }
//     { (>>>>>>[type_c cccccc ,]) }
//     { (>>>>>>[type_d dddddddd ,]) }
//     { (>>>>>>[type_e eeeeeeee ,]) }
//     { (>>>>>>[type_f ffff ) ;]) }
//   }
// }
//
// The special case of a singleton argument being flattened is also supported.
// --------------
// { (>>[<auto>]) @{1,0}, policy: append-fitting-sub-partitions
//   { (>>[function fffffffffff (]) }
//   { (>>>>>>[type_a aaaa ) ;]) }
// }
//
// Example outputs:
// ----------------
//
// style.column_limit = 100:
//
// { (>>[<auto>]) @{1,0}, policy: append-fitting-sub-partitions
//   { (>>[<auto>]) @{1,0,0}, policy: fit-else-expand
//     { (>>[function fffffffffff (]) }
//     { (>>>>>>[type_a aaaa ,]) }
//     { (>>>>>>[type_b bbbbb ,]) }
//     { (>>>>>>[type_c cccccc ,]) }
//     { (>>>>>>[type_d dddddddd ,]) }
//     { (>>>>>>[type_e eeeeeeee ,]) }
//   }
//   { (>>>>>>>>>>>>>>>>>>>>>>>[<auto>]) @{1,0,1}, policy: fit-else-expand
//     { (>>>>>>[type_f ffff ) ;]) }
//   }
// }
//
// style.column_limit = 56:
//
// { (>>[<auto>]) @{1,0}, policy: append-fitting-sub-partitions
//   { (>>[<auto>]) @{1,0,0}, policy: fit-else-expand
//     { (>>[function fffffffffff (]) }
//     { (>>>>>>[type_a aaaa ,]) }
//     { (>>>>>>[type_b bbbbb ,]) }
//   }
//   { (>>>>>>>>>>>>>>>>>>>>>>>[<auto>]) @{1,0,1}, policy: fit-else-expand
//     { (>>>>>>[type_c cccccc ,]) }
//     { (>>>>>>[type_d dddddddd ,]) }
//   }
//   { (>>>>>>>>>>>>>>>>>>>>>>>[<auto>]) @{1,0,2}, policy: fit-else-expand
//     { (>>>>>>[type_e eeeeeeee ,]) }
//     { (>>>>>>[type_f ffff ) ;]) }
//   }
// }
//
// style.column_limit = 40:
//
// { (>>[<auto>]) @{1,0}, policy: append-fitting-sub-partitions
//   { (>>[<auto>]) @{1,0,0}, policy: fit-else-expand
//     { (>>[function fffffffffff (]) }
//   }
//   { (>>>>>>[<auto>]) @{1,0,1}, policy: fit-else-expand
//     { (>>>>>>[type_a aaaa ,]) }
//     { (>>>>>>[type_b bbbbb ,]) }
//   }
//   { (>>>>>>[<auto>]) @{1,0,2}, policy: fit-else-expand
//     { (>>>>>>[type_c cccccc ,]) }
//     { (>>>>>>[type_d dddddddd ,]) }
//   }
//   { (>>>>>>[<auto>]) @{1,0,3}, policy: fit-else-expand
//     { (>>>>>>[type_e eeeeeeee ,]) }
//     { (>>>>>>[type_f ffff ) ;]) }
//   }
// }
void ReshapeFittingSubpartitions(const verible::BasicFormatStyle& style,
                                 TokenPartitionTree* node);

}  // namespace verible

#endif  // VERIBLE_COMMON_FORMATTING_TOKEN_PARTITION_TREE_H_
