// Copyright 2017-2021 The Verible Authors.
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

// Implementation details of layout_optimizer.cc exported for tests.

#ifndef VERIBLE_VERILOG_FORMATTING_LAYOUT_OPTIMIZER_INTERNAL_H_
#define VERIBLE_VERILOG_FORMATTING_LAYOUT_OPTIMIZER_INTERNAL_H_

#include <algorithm>
#include <iterator>
#include <ostream>
#include <type_traits>
#include <vector>

#include "absl/container/fixed_array.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "common/formatting/basic_format_style.h"
#include "common/formatting/token_partition_tree.h"
#include "common/formatting/unwrapped_line.h"
#include "common/util/tree_operations.h"
#include "common/util/vector_tree.h"

namespace verible {

// LayoutItem type
enum class LayoutType {
  // Single line. LayoutItem of this type is always a leaf in LayoutTree.
  kLine,

  // Joins child items horizontally. See also:
  // LayoutFunctionFactory::Juxtaposition.
  kJuxtaposition,

  // Stacks child items vertically. See also: LayoutFunctionFactory::Stack.
  kStack,
};

std::ostream& operator<<(std::ostream& stream, LayoutType type);

// LayoutTree node data
class LayoutItem {
 public:
  // Prevent creation of uninitialized LayoutItem
  LayoutItem() = delete;

  LayoutItem(LayoutType type, int spacing, bool must_wrap, int indentation = 0)
      : type_(type),
        indentation_(indentation),
        spaces_before_(spacing),
        must_wrap_(must_wrap) {
    CHECK_GE(indentation_, 0);
    CHECK_GE(spaces_before_, 0);
  }

  // Creates Line item from UnwrappedLine.
  explicit LayoutItem(const UnwrappedLine& uwline, int indentation = 0)
      : type_(LayoutType::kLine),
        indentation_(indentation),
        tokens_(uwline.TokensRange()),
        spaces_before_(SpacesRequiredBeforeUnwrappedLine(uwline)),
        must_wrap_(UnwrappedLineMustWrap(uwline)) {
    CHECK_GE(indentation_, 0);
    CHECK_GE(spaces_before_, 0);
  }

  // Creates Line item from UnwrappedLine.
  explicit LayoutItem(const UnwrappedLine& uwline, bool must_wrap,
                      int indentation)
      : type_(LayoutType::kLine),
        indentation_(indentation),
        tokens_(uwline.TokensRange()),
        spaces_before_(SpacesRequiredBeforeUnwrappedLine(uwline)),
        must_wrap_(must_wrap) {
    CHECK_GE(indentation_, 0);
    CHECK_GE(spaces_before_, 0);
  }

  // Multiple LayoutFunctionSegments can store copies of the same layout.
  // The objects are copied mostly in LayoutFunctionFactory::* functions.
  LayoutItem(const LayoutItem&) = default;
  LayoutItem& operator=(const LayoutItem&) = default;

  LayoutItem(LayoutItem&&) = default;
  LayoutItem& operator=(LayoutItem&&) = default;

  LayoutType Type() const { return type_; }

  // Indentation used for a layout when it is placed at the beginning of a line.
  // Effective indentation in this case is a sum of the item's and its
  // ancestors' indetation
  int IndentationSpaces() const { return indentation_; }

  // Sets indentation used for a layout when it is placed at the beginning of
  // a line.
  void SetIndentationSpaces(int indent) { indentation_ = indent; }

  // Returns number of spaces required before the first token. The spaces are
  // used when the layout is appended to a non-empty line.
  int SpacesBefore() const { return spaces_before_; }

  // Returns whether to force line break just before this layout.
  bool MustWrap() const { return must_wrap_; }

  // Sets whether to force line break just before this layout.
  void SetMustWrap(bool must_wrap) { must_wrap_ = must_wrap; }

  // Returns textual representation of spanned tokens for Line items, empty
  // string for other item types.
  std::string Text() const {
    return absl::StrJoin(tokens_, " ",
                         [](std::string* out, const PreFormatToken& token) {
                           absl::StrAppend(out, token.Text());
                         });
  }

  // Returns length of the line in columns.
  // Can be called only on Line items.
  int Length() const {
    CHECK_EQ(type_, LayoutType::kLine);

    if (tokens_.empty()) return 0;
    int len = 0;
    for (const auto& token : tokens_) {
      // TODO (mglb): support all possible break_decisions
      len += token.before.spaces_required;
      if (const auto line_break_pos = token.Text().find('\n');
          line_break_pos != std::string_view::npos) {
        // Multiline tokens are not really supported.
        // Use number of characters up to the first line break.
        len += line_break_pos;
        DVLOG(5) << __FUNCTION__ << ": WARNING: Token contains '\\n':\n"
                 << *token.token;
      } else {
        len += token.Length();
      }
    }
    len -= tokens_.front().before.spaces_required;
    return len;
  }

  // Returns tokens range spanned by the Line item.
  // Can be called only on Line items.
  FormatTokenRange TokensRange() const {
    CHECK_EQ(type_, LayoutType::kLine);
    return tokens_;
  }

  friend bool operator==(const LayoutItem& lhs, const LayoutItem& rhs) {
    return (lhs.type_ == rhs.type_ && lhs.indentation_ == rhs.indentation_ &&
            lhs.tokens_ == rhs.tokens_ &&
            lhs.spaces_before_ == rhs.spaces_before_ &&
            lhs.must_wrap_ == rhs.must_wrap_);
  }

 private:
  static bool UnwrappedLineMustWrap(const UnwrappedLine& uwline) {
    if (uwline.TokensRange().empty()) return false;

    const auto policy = uwline.PartitionPolicy();
    if (policy == PartitionPolicyEnum::kInline) return false;
    if (policy == PartitionPolicyEnum::kAlreadyFormatted) return true;

    auto break_decision = uwline.TokensRange().front().before.break_decision;
    return (break_decision == SpacingOptions::MustWrap);
  }

  static int SpacesRequiredBeforeUnwrappedLine(const UnwrappedLine& uwline) {
    const auto tokens = uwline.TokensRange();
    const auto policy = uwline.PartitionPolicy();
    const auto indentation = uwline.IndentationSpaces();

    if (policy == PartitionPolicyEnum::kInline) return indentation;
    if (tokens.empty()) return 0;
    return tokens.front().before.spaces_required;
  }

  LayoutType type_;
  int indentation_;
  FormatTokenRange tokens_;
  int spaces_before_;
  bool must_wrap_;
};

std::ostream& operator<<(std::ostream& stream, const LayoutItem& layout);

// Intermediate partition tree layout
using LayoutTree = VectorTree<LayoutItem>;

// Single segment of LayoutFunction
// Maps starting column to a linear cost function and its optimal layout.
struct LayoutFunctionSegment {
  // Starting column.
  // AKA: knot.
  int column;

  // Optimal layout for an interval starting at the column.
  // AKA: layout expression
  LayoutTree layout;

  // Width of the last line of the layout in columns.
  int span;

  // Intercept (a constant) of linear cost function.
  float intercept;
  // Gradient (rate of change) of linear cost function.
  int gradient;

  // Returns cost of placing the layout at 'margin' column.
  float CostAt(int margin) const {
    CHECK_GE(margin, 0);
    CHECK_GE(margin, column);
    return intercept + gradient * (margin - column);
  }

  LayoutFunctionSegment(const LayoutFunctionSegment&) = default;
  LayoutFunctionSegment& operator=(const LayoutFunctionSegment&) = default;

  LayoutFunctionSegment(LayoutFunctionSegment&&) = default;
  LayoutFunctionSegment& operator=(LayoutFunctionSegment&&) = default;
};

template <bool IsConstIterator>
class LayoutFunctionIterator;

// Piecewise-linear layout function.
//
// The layout function represents one or more layouts for a single fragment of
// code and a cost function used for picking the most optimal layout.
//
// The class is a set containing LayoutFunctionSegments. Each segment starts at
// its starting column and ends at the next segment's starting column. The last
// segment spans up to infinity.
//
// AKA: KnotSet, Block
class LayoutFunction {
 public:
  using iterator = LayoutFunctionIterator<false>;
  using const_iterator = LayoutFunctionIterator<true>;

  LayoutFunction() = default;
  LayoutFunction(std::initializer_list<LayoutFunctionSegment> segments)
      : segments_(segments) {
    CHECK(AreSegmentsSorted());
    if (!segments_.empty()) CHECK_EQ(segments_.front().column, 0);
  }

  LayoutFunction(LayoutFunction&&) = default;
  LayoutFunction& operator=(LayoutFunction&&) = default;

  LayoutFunction(const LayoutFunction&) = default;
  LayoutFunction& operator=(const LayoutFunction&) = default;

  void push_back(const LayoutFunctionSegment& segment) {
    if (!segments_.empty())
      CHECK_LT(segments_.back().column, segment.column);
    else
      CHECK_EQ(segment.column, 0);
    segments_.push_back(segment);
  }
  void push_back(LayoutFunctionSegment&& segment) {
    if (!segments_.empty())
      CHECK_LT(segments_.back().column, segment.column);
    else
      CHECK_EQ(segment.column, 0);
    segments_.push_back(segment);
  }

  bool empty() const { return segments_.empty(); }

  int size() const { return segments_.size(); }

  iterator begin();
  const_iterator begin() const;

  iterator end();
  const_iterator end() const;

  // Returns iterator pointing to a segment starting at or to the left of
  // 'column'.
  // AKA: x-
  const_iterator AtOrToTheLeftOf(int column) const;

  LayoutFunctionSegment& front() { return segments_.front(); }
  const LayoutFunctionSegment& front() const { return segments_.front(); }

  LayoutFunctionSegment& back() { return segments_.back(); }
  const LayoutFunctionSegment& back() const { return segments_.back(); }

  const LayoutFunctionSegment& operator[](size_t index) const {
    CHECK_GE(index, 0);
    CHECK_LT(index, segments_.size());
    return segments_[index];
  }
  LayoutFunctionSegment& operator[](size_t index) {
    CHECK_GE(index, 0);
    CHECK_LT(index, segments_.size());
    return segments_[index];
  }

  // Returns whether to force line break just before this layout.
  bool MustWrap() const {
    if (empty()) return false;
    const bool must_wrap = segments_.front().layout.Value().MustWrap();
    // If for some reason not all layouts have the same "MustWrap" status, it
    // should be taken into account in the code that uses this method. This
    // shouldn't be the case, as every layout should wrap the same token range.
    CHECK(std::all_of(segments_.begin(), segments_.end(),
                      [must_wrap](const auto& segment) {
                        return segment.layout.Value().MustWrap() == must_wrap;
                      }));
    return must_wrap;
  }

  // Sets whether to force line break just before this layout.
  void SetMustWrap(bool must_wrap) {
    for (auto& segment : segments_)
      segment.layout.Value().SetMustWrap(must_wrap);
  }

 private:
  bool AreSegmentsSorted() const {
    return std::is_sorted(
        segments_.begin(), segments_.end(),
        [](const LayoutFunctionSegment& a, const LayoutFunctionSegment& b) {
          return a.column < b.column;
        });
  }
  // Elements in 'segments_' must have unique columns and be sorted by column.
  // The first segment must start at column 0.
  // std::set would be more appropriate generally, but due to really
  // small amount of elements the container has to hold and ordered inserts, it
  // probably wouldn't help in anything.
  std::vector<LayoutFunctionSegment> segments_;
};

template <bool IsConst, typename T>
using ConditionalConst = std::conditional_t<IsConst, std::add_const_t<T>, T>;

// Iterator used by LayoutFunction.
template <bool IsConstIterator>
class LayoutFunctionIterator {
  using iterator = LayoutFunctionIterator<IsConstIterator>;
  using container = ConditionalConst<IsConstIterator, LayoutFunction>;
  // Make friends with the-other-constness iterator (for comparison operators)
  friend class LayoutFunctionIterator<!IsConstIterator>;

 public:
  LayoutFunctionIterator() = default;

  explicit LayoutFunctionIterator(container& layout_function, int index = 0)
      : lf_(&layout_function), index_(index) {
    CHECK_LE(index_, lf_->size());
  }

  LayoutFunctionIterator(const iterator&) = default;
  LayoutFunctionIterator& operator=(const iterator&) = default;

  // Helper methods

  // Returns reference to iterated container
  container& Container() const { return *lf_; }

  // Returns index of current element
  int Index() const { return index_; }

  // Return whether iterator points to container's end()
  bool IsEnd() const { return index_ == lf_->size(); }

  // Moves iterator to a segment starting at or to the left of 'column'.
  void MoveToKnotAtOrToTheLeftOf(int column) {
    CHECK_GE(column, 0);
    if (Container().empty()) return;
    CHECK_EQ(Container().front().column, 0);

    auto& this_ref = *this;
    if (this_ref->column > column) {
      while (this_ref->column > column) --this_ref;
    } else {
      // Find first segment to the right of the 'column'...
      while (!IsEnd() && this_ref->column <= column) ++this_ref;
      // ... and go one segment back.
      --this_ref;
    }
  }

  // RandomAccessIterator interface

  using iterator_category = std::random_access_iterator_tag;
  using difference_type = std::ptrdiff_t;
  using value_type = LayoutFunctionSegment;
  using pointer = ConditionalConst<IsConstIterator, LayoutFunctionSegment>*;
  using reference = ConditionalConst<IsConstIterator, LayoutFunctionSegment>&;

  reference operator*() const { return (*lf_)[index_]; }

  pointer operator->() const { return &(*lf_)[index_]; }

  reference operator[](size_t index) const {
    CHECK_LT(index, lf_->size() - index_);
    return (*lf_)[index_ + index];
  }

  iterator& operator+=(difference_type rhs) {
    CHECK_LE(rhs, lf_->size() - index_);
    index_ += rhs;
    return *this;
  }
  iterator& operator-=(difference_type rhs) {
    CHECK_LE(rhs, index_);
    index_ -= rhs;
    return *this;
  }

  iterator& operator++() { return *this += 1; }

  iterator& operator--() { return *this -= 1; }

  iterator operator++(int) {
    auto tmp = *this;
    ++(*this);
    return tmp;
  }

  iterator operator--(int) {
    auto tmp = *this;
    --(*this);
    return tmp;
  }

  iterator operator+(difference_type rhs) const {
    return iterator(*lf_, index_ + rhs);
  }
  iterator operator-(difference_type rhs) const {
    return iterator(*lf_, index_ - rhs);
  }

  friend iterator operator+(difference_type lhs, const iterator& rhs) {
    return iterator(*rhs.lf_, lhs + rhs.index_);
  }
  friend iterator operator-(difference_type lhs, const iterator& rhs) {
    return iterator(*rhs.lf_, lhs - rhs.index_);
  }

  template <bool RhsIsConst>
  difference_type operator+(
      const LayoutFunctionIterator<RhsIsConst>& rhs) const {
    return this->index_ + rhs.index_;
  }
  template <bool RhsIsConst>
  difference_type operator-(
      const LayoutFunctionIterator<RhsIsConst>& rhs) const {
    return this->index_ - rhs.index_;
  }

  template <bool RhsIsConst>
  bool operator==(const LayoutFunctionIterator<RhsIsConst>& rhs) const {
    return (lf_ == rhs.lf_) && (index_ == rhs.index_);
  }
  template <bool RhsIsConst>
  bool operator<(const LayoutFunctionIterator<RhsIsConst>& rhs) const {
    return (lf_ == rhs.lf_) && (index_ < rhs.index_);
  }
  template <bool RhsIsConst>
  bool operator>(const LayoutFunctionIterator<RhsIsConst>& rhs) const {
    return (lf_ == rhs.lf_) && (index_ > rhs.index_);
  }
  template <bool RhsIsConst>
  bool operator!=(const LayoutFunctionIterator<RhsIsConst>& rhs) const {
    return !(*this == rhs);
  }
  template <bool RhsIsConst>
  bool operator<=(const LayoutFunctionIterator<RhsIsConst>& rhs) const {
    return !(*this > rhs);
  }
  template <bool RhsIsConst>
  bool operator>=(const LayoutFunctionIterator<RhsIsConst>& rhs) const {
    return !(*this < rhs);
  }

 private:
  container* lf_;
  int index_;
};

std::ostream& operator<<(std::ostream& stream,
                         const LayoutFunctionSegment& segment);

std::ostream& operator<<(std::ostream& stream, const LayoutFunction& lf);

template <bool IsConstIterator>
std::ostream& operator<<(std::ostream& stream,
                         const LayoutFunctionIterator<IsConstIterator>& it) {
  return stream << &it.Container() << "[" << it.Index() << "/"
                << it.Container().size() << "]";
}

template <typename Iterator, typename ValueType>
inline constexpr bool IsIteratorDereferencingTo = std::is_same_v<
    std::remove_const_t<typename std::iterator_traits<Iterator>::value_type>,
    ValueType>;

// Methods for creating and combining LayoutFunctions
class LayoutFunctionFactory {
 public:
  explicit LayoutFunctionFactory(const BasicFormatStyle& style)
      : style_(style) {}

  // Creates LayoutFunction for a single line from UnwrappedLine 'uwline'.
  LayoutFunction Line(const UnwrappedLine& uwline) const;

  // Creates LayoutFunction from a single line with tokens wrapped using Wrap().
  LayoutFunction WrappedLine(const UnwrappedLine& uwline) const;

  // Combines two or more layouts vertically.
  // All combined layouts start at the same column. The first line of layout
  // n+1 is immediately below the last line of layout n.
  LayoutFunction Stack(std::initializer_list<LayoutFunction> lfs) const {
    return Stack(lfs.begin(), lfs.end());
  }

  // See Stack(std::initializer_list<LayoutFunction> lfs).
  //
  // Iterator: iterator type that dereferences to LayoutFunction.
  template <class Iterator>
  LayoutFunction Stack(const Iterator begin, const Iterator end) const {
    static_assert(IsIteratorDereferencingTo<Iterator, LayoutFunction>,
                  "Iterator's value type must be LayoutFunction.");
    const auto lfs = make_container_range(begin, end);

    if (lfs.empty()) return LayoutFunction();
    if (lfs.size() == 1) return lfs.front();

    // Create a segment iterator for each LayoutFunction.
    auto segments =
        absl::FixedArray<LayoutFunction::const_iterator>(lfs.size());
    std::transform(lfs.begin(), lfs.end(), segments.begin(),
                   [](const LayoutFunction& lf) {
                     CHECK(!lf.empty());
                     return lf.begin();
                   });

    return Stack(&segments);
  }

  // Combines two or more layouts so that the layout N+1 is directy to the
  // right of the last line of layout N.
  //
  // EXAMPLE:
  //
  // Layout 1:
  //     First First First First First First
  //     First First First
  //
  // Layout 2:
  //     Second Second Second
  //     Second Second
  //
  // Juxtaposition:
  //     First First First First First First
  //     First First First Second Second Second
  //                       Second Second
  LayoutFunction Juxtaposition(
      std::initializer_list<LayoutFunction> lfs) const {
    return Juxtaposition(lfs.begin(), lfs.end());
  }

  // See Juxtaposition(std::initializer_list<LayoutFunction> lfs).
  //
  // Iterator: iterator type that dereferences to LayoutFunction.
  template <class Iterator>
  LayoutFunction Juxtaposition(Iterator begin, Iterator end) const {
    static_assert(IsIteratorDereferencingTo<Iterator, LayoutFunction>,
                  "Iterator's value type must be LayoutFunction.");

    auto lfs_container = make_container_range(begin, end);

    if (lfs_container.empty()) return LayoutFunction();
    if (lfs_container.size() == 1) return lfs_container.front();

    LayoutFunction incremental = lfs_container.front();
    lfs_container.pop_front();
    for (auto& lf : lfs_container) {
      incremental = Juxtaposition(incremental, lf);
    }

    return incremental;
  }

  // Creates the piecewise minimum function of a set of LayoutFunctions.
  //
  // The combinator is intended to choose optimal layout from a set of
  // different layouts of the same code fragment.
  //
  // When two layouts have the same cost, the function favors the layout with
  // lower gradient. When gradients are equal too, earlier element is used.
  LayoutFunction Choice(std::initializer_list<LayoutFunction> lfs) const {
    return Choice(lfs.begin(), lfs.end());
  }

  // See Choice(std::initializer_list<LayoutFunction> lfs).
  //
  // Iterator: iterator type that dereferences to LayoutFunction.
  template <class Iterator>
  LayoutFunction Choice(const Iterator begin, const Iterator end) const {
    static_assert(IsIteratorDereferencingTo<Iterator, LayoutFunction>,
                  "Iterator's value type must be LayoutFunction.");
    const auto lfs = make_container_range(begin, end);

    if (lfs.empty()) return LayoutFunction();
    if (lfs.size() == 1) return lfs.front();

    // Create a segment iterator for each LayoutFunction.
    auto segments =
        absl::FixedArray<LayoutFunction::const_iterator>(lfs.size());
    std::transform(lfs.begin(), lfs.end(), segments.begin(),
                   [](const LayoutFunction& lf) {
                     CHECK(!lf.empty());
                     return lf.begin();
                   });

    return Choice(&segments);
  }

  // Returns LayoutFunction 'lf' with layout indented using 'indent' spaces.
  LayoutFunction Indent(const LayoutFunction& lf, int indent) const;

  // Joins layouts horizontally and wraps them into multiple lines to stay under
  // column limit. Kind of like a words in a paragraph.
  LayoutFunction Wrap(std::initializer_list<LayoutFunction> lfs,
                      bool use_tokens_break_penalty = false,
                      int hanging_indentation = 0) const {
    return Wrap(lfs.begin(), lfs.end(), use_tokens_break_penalty,
                hanging_indentation);
  }

  // See Wrap(std::initializer_list<LayoutFunction> lfs).
  //
  // Iterator: iterator type that dereferences to LayoutFunction.
  template <class Iterator>
  LayoutFunction Wrap(const Iterator begin, const Iterator end,
                      bool use_tokens_break_penalty = false,
                      int hanging_indentation = 0) const {
    static_assert(IsIteratorDereferencingTo<Iterator, LayoutFunction>,
                  "Iterator's value type must be LayoutFunction.");

    const auto lfs = make_container_range(begin, end);

    if (lfs.empty()) return LayoutFunction();
    if (lfs.size() == 1) return lfs.front();

    absl::FixedArray<LayoutFunction> results(lfs.size());

    const int size = lfs.size();
    for (int i = size - 1; i >= 0; --i) {
      absl::FixedArray<LayoutFunction> results_i(size - i);
      LayoutFunction incremental = lfs[i];
      for (int j = i; j < size - 1; ++j) {
        results_i[j - i] = Stack({
            incremental,
            (i == 0) ? Indent(results[j + 1], hanging_indentation)
                     : results[j + 1],
        });

        const auto& next_element = lfs[j + 1];
        if (use_tokens_break_penalty) {
          // Deprioritize token-level wrapping
          // TODO(mglb): Find a better way to do this. This ratio has been
          // chosen using only a few test cases.
          const int wrapping_penalty = style_.over_column_limit_penalty;
          const auto& second_layout = results[j + 1].front().layout;
          const auto& first_line = LeftmostDescendant(second_layout).Value();
          const auto& first_token = first_line.TokensRange().front();
          const int token_break_penalty = first_token.before.break_penalty;

          for (auto& segment : results_i[j - i])
            segment.intercept += wrapping_penalty + token_break_penalty;
        }

        if (next_element.MustWrap())
          incremental = Stack({
              std::move(incremental),
              Indent(next_element, hanging_indentation),
          });
        else
          // TODO(mglb): use Stack for invervals where lfs[j] is multiline (i.e.
          // has any stack sublayouts)
          incremental = Juxtaposition({std::move(incremental), next_element});
      }
      results_i.back() = std::move(incremental);

      // Using reverse range to favor layouts with elements packed in earlier
      // lines.
      results[i] = Choice(results_i.rbegin(), results_i.rend());
    }
    return results[0];
  }

 private:
  LayoutFunction Juxtaposition(const LayoutFunction& left,
                               const LayoutFunction& right) const;

  LayoutFunction Stack(
      absl::FixedArray<LayoutFunction::const_iterator>* segments) const;

  static LayoutFunction Choice(
      absl::FixedArray<LayoutFunction::const_iterator>* segments);

  const BasicFormatStyle& style_;
};

class TokenPartitionsLayoutOptimizer {
 public:
  explicit TokenPartitionsLayoutOptimizer(const BasicFormatStyle& style)
      : factory_(style) {}

  TokenPartitionsLayoutOptimizer(const TokenPartitionsLayoutOptimizer&) =
      delete;
  TokenPartitionsLayoutOptimizer(TokenPartitionsLayoutOptimizer&&) = delete;
  TokenPartitionsLayoutOptimizer& operator=(
      const TokenPartitionsLayoutOptimizer&) = delete;
  TokenPartitionsLayoutOptimizer& operator=(TokenPartitionsLayoutOptimizer&&) =
      delete;

  void Optimize(int indentation, TokenPartitionTree* node) const;

  LayoutFunction CalculateOptimalLayout(const TokenPartitionTree& node) const;

 private:
  const LayoutFunctionFactory factory_;
};

class TreeReconstructor {
 public:
  explicit TreeReconstructor(int indentation_spaces)
      : current_indentation_spaces_(indentation_spaces) {}
  ~TreeReconstructor() = default;

  TreeReconstructor(const TreeReconstructor&) = delete;
  TreeReconstructor(TreeReconstructor&&) = delete;
  TreeReconstructor& operator=(const TreeReconstructor&) = delete;
  TreeReconstructor& operator=(TreeReconstructor&&) = delete;

  void TraverseTree(const LayoutTree& layout_tree);

  void ReplaceTokenPartitionTreeNode(TokenPartitionTree* node);

 private:
  TokenPartitionTree tree_;
  TokenPartitionTree* current_node_ = nullptr;

  int current_indentation_spaces_;
};

}  // namespace verible

#endif  // VERIBLE_VERILOG_FORMATTING_LAYOUT_OPTIMIZER_INTERNAL_H_
