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

// Improve code formatting by optimizing token partitions layout with
// algorithm documented in https://research.google/pubs/pub44667/
// (similar tool for R language: https://github.com/google/rfmt)

#ifndef VERIBLE_VERILOG_FORMATTING_LAYOUT_OPTIMIZER_H_
#define VERIBLE_VERILOG_FORMATTING_LAYOUT_OPTIMIZER_H_

#include <deque>
#include <vector>

#include "common/formatting/basic_format_style.h"
#include "common/formatting/token_partition_tree.h"
#include "common/formatting/unwrapped_line.h"
#include "common/util/vector_tree.h"

namespace verible {

// Layout type
enum class LayoutType {
  // Holds UnwrappedLine
  kLayoutLine,

  // Merges sublayouts, horizontally
  kLayoutHorizontalMerge,

  // Merges sublayout, vertically
  kLayoutVerticalMerge,

  // Indent sublayout
  // ----------------
  // Why do we introduce new layout instead of using
  // kLayoutHorizontalMerge + kLayoutLine(empty + indent)?
  // Because:
  // 1. It does not introduce Knot at column_limit - indent
  // 2. No need to check layouts type in HorizontalJoin() whether to
  //    skip or not before_spaces (when merging indent line with normal
  //    line/layout)
  // 3. Wrapping layout forces GetSpacesBefore() return 0
  kLayoutIndent,
};

std::ostream& operator<<(std::ostream& stream, const LayoutType& type);

// Intermediate partition tree layout
class Layout {
 public:
  Layout(LayoutType type, int spacing)
      : type_(type), indentation_(0), spaces_before_(spacing) {}

  Layout(const UnwrappedLine& uwline)
      : type_(LayoutType::kLayoutLine), indentation_(0) {
    tokens_ = uwline.TokensRange();
    if (tokens_.size() > 0) {
      assert(tokens_.size() > 0);
      spaces_before_ = tokens_.front().before.spaces_required;
    } else {
      spaces_before_ = 0;
    }
  }

  Layout(int indent)
      : type_(LayoutType::kLayoutIndent),
        indentation_(indent),
        spaces_before_(0) {}

  ~Layout() = default;
  Layout(const Layout&) = default;

  // Deleting standard interfaces
  Layout() = delete;
  Layout(Layout&&) = delete;
  Layout& operator=(const Layout&) = delete;
  Layout& operator=(Layout&&) = delete;

  LayoutType GetType() const { return type_; }

  int GetIndentationSpaces() const { return indentation_; }

  int SpacesBeforeLayout() const { return spaces_before_; }

  std::string Text() const {
    return absl::StrJoin(tokens_, " ",
                         [=](std::string* out, const PreFormatToken& token) {
                           absl::StrAppend(out, token.Text());
                         });
  }

  UnwrappedLine AsUnwrappedLine() const {
    assert(type_ == LayoutType::kLayoutLine);
    UnwrappedLine uwline(0, tokens_.begin());
    uwline.SpanUpToToken(tokens_.end());
    uwline.SetPartitionPolicy(PartitionPolicyEnum::kAlwaysExpand);
    return uwline;
  }

  int Length() const {
    assert(type_ == LayoutType::kLayoutLine);
    if (tokens_.size() == 0) {
      return 0;
    }
    int len = 0;
    for (const auto& token : tokens_) {
      len += token.before.spaces_required;
      len += token.Length();
    }
    len -= tokens_.front().before.spaces_required;
    return len;
  }

  static int UnwrappedLineLength(const UnwrappedLine& uwline) {
    int len = 0;
    for (const auto& token : uwline.TokensRange()) {
      len += token.before.spaces_required;
      len += token.Length();
    }
    len -= uwline.TokensRange().front().before.spaces_required;
    return len;
  }

  SpacingOptions SpacingOptionsLayout() const {
    assert(type_ == LayoutType::kLayoutLine);
    assert(tokens_.size() > 0);
    return tokens_.front().before.break_decision;
  }

  bool MustWrapLayout() const {
    return SpacingOptionsLayout() == SpacingOptions::MustWrap;
  }

  bool MustAppendLayout() const {
    return SpacingOptionsLayout() == SpacingOptions::MustAppend;
  }

 private:
  const LayoutType type_;

  const int indentation_;

  /* const */ FormatTokenRange tokens_;

  /* const */ int spaces_before_;
};

std::ostream& operator<<(std::ostream& stream, const Layout& layout);

using LayoutTree = VectorTree<const Layout>;

class Knot {
  friend std::ostream& operator<<(std::ostream&, const Knot&);

 public:
  explicit Knot(int column, int span, float intercept, int gradient,
                LayoutTree layout, int before_spaces,
                SpacingOptions break_decision)
      : column_(column),
        span_(span),
        intercept_(intercept),
        gradient_(gradient),
        layout_(layout),
        before_spaces_(before_spaces),
        break_decision_(break_decision) {}

  Knot(const Knot&) = default;
  Knot(Knot&&) = default;

  ~Knot() = default;

  // Deleting standard interfaces:
  Knot() = delete;
  Knot& operator=(const Knot&) = delete;
  Knot& operator=(Knot&&) = delete;

  int GetColumn() const { return column_; }
  int GetSpan() const { return span_; }
  float GetIntercept() const { return intercept_; }
  int GetGradient() const { return gradient_; }
  LayoutTree GetLayout() const { return layout_; }
  int GetSpacesBefore() const { return before_spaces_; }
  SpacingOptions GetSpacingOptions() const { return break_decision_; }

  bool MustWrap() const { return break_decision_ == SpacingOptions::MustWrap; }

  bool MustAppend() const {
    return break_decision_ == SpacingOptions::MustAppend;
  }

  // Total cost if this knot if placed at column 'm'
  float ValueAt(int m) const {
    assert(m >= 0 && m >= column_);
    return intercept_ +                // static cost
           gradient_ * (m - column_);  // plus gradient (over column_limit)
  }

 private:
  // Start column
  const int column_;

  // Span of Knot
  const int span_;

  // Constant cost of this knot
  const float intercept_;

  // Cost of over limit characters from this knot
  // cost = intercept_ + (over_limit_characters) * gradient_
  const int gradient_;

  // Layout (subsolution)
  const LayoutTree layout_;

  const int before_spaces_;

  const SpacingOptions break_decision_;
};

class KnotSet {
  friend std::ostream& operator<<(std::ostream&, const KnotSet&);

 public:
  KnotSet() = default;
  ~KnotSet() = default;

  KnotSet(KnotSet&&) = default;
  KnotSet(const KnotSet&) = default;
  KnotSet& operator=(KnotSet&&) = default;

  // Deleting standard interfaces:
  KnotSet& operator=(const KnotSet&) = delete;

  const Knot& operator[](size_t idx) const {
    assert(idx < knots_.size());
    return knots_[idx];
  }

  int Size() const { return knots_.size(); }

  bool MustWrap() const {
    return std::find_if(knots_.begin(), knots_.end(), [](const Knot& knot) {
             return knot.MustWrap();
           }) != knots_.end();
  }

  void AppendKnot(const Knot& knot) { knots_.push_back(knot); }

  static KnotSet FromUnwrappedLine(const UnwrappedLine& uwline,
                                   const BasicFormatStyle& style);

  static KnotSet IndentBlock(const KnotSet right, int indent,
                             const BasicFormatStyle& style);

  KnotSet InterceptPlusConst(float const_val) const;

 private:
  std::vector</* const */ Knot> knots_;
};

std::ostream& operator<<(std::ostream& stream, const KnotSet& knot_set);

class KnotSetIterator {
 public:
  explicit KnotSetIterator(const KnotSet& knot_set)
      : knot_set_(knot_set), index_(0) {}

  void Advance() { index_ += 1; }

  void Reset() { index_ = 0; }

  bool Done() const { return index_ >= knot_set_.Size(); }

  int CurrentColumn() const {
    if (index_ >= knot_set_.Size()) {
      return std::numeric_limits<int>::max();
    } else {
      return knot_set_[index_].GetColumn();
    }
  }

  int NextKnotColumn() const {
    if ((index_ + 1) >= knot_set_.Size()) {
      return std::numeric_limits<int>::max();
    } else {
      return knot_set_[index_ + 1].GetColumn();
    }
  }

  void MoveToMargin(int m) {
    if (CurrentColumn() > m) {
      while (CurrentColumn() > m) {
        index_ -= 1;
      }
    } else {
      while (NextKnotColumn() <= m) {
        index_ += 1;
      }
    }
  }

  float CurrentKnotValueAt(int m) const {
    assert(index_ < knot_set_.Size());
    return knot_set_[index_].ValueAt(m);
  }

  const Knot& CurrentKnot() const { return knot_set_[index_]; }

  int GetIndex() const { return index_; }

  int Size() const { return knot_set_.Size(); }

 private:
  const KnotSet& knot_set_;

  int index_;
};

class SolutionSet : public std::deque<KnotSet> {
 public:
  SolutionSet() = default;
  ~SolutionSet() = default;

  // SolutionSet{} constructor
  using std::deque<KnotSet>::deque;

  // Deleting standard interfaces:
  SolutionSet(const SolutionSet&) = delete;
  SolutionSet(SolutionSet&&) = delete;
  SolutionSet& operator=(const SolutionSet&) = delete;
  SolutionSet& operator=(SolutionSet&&) = delete;

  KnotSet VerticalJoin(const BasicFormatStyle& style);

  KnotSet HorizontalJoin(const BasicFormatStyle& style);

  KnotSet MinimalSet(const BasicFormatStyle& style);

  KnotSet WrapSet(const BasicFormatStyle& style);

 private:
  KnotSet HorizontalJoin(const KnotSet& left, const KnotSet& right,
                         const BasicFormatStyle& style);

  void ResetIteratorSet() {
    iterator_set_.clear();
    std::transform(
        begin(), end(), std::back_inserter(iterator_set_),
        [](const KnotSet& knot_set) { return KnotSetIterator(knot_set); });
  }

  void MoveIteratorSet(int margin) {
    std::for_each(iterator_set_.begin(), iterator_set_.end(),
                  [&margin](KnotSetIterator& s) { s.MoveToMargin(margin); });
  }

  std::vector<int> IteratorSetCurrentGradients() const {
    std::vector<int> gradients;
    std::transform(iterator_set_.begin(), iterator_set_.end(),
                   std::back_inserter(gradients), [](const KnotSetIterator& s) {
                     return s.CurrentKnot().GetGradient();
                   });
    return gradients;
  }

  std::vector<float> IteratorSetValuesAt(int column) const {
    std::vector<float> values;
    std::transform(iterator_set_.begin(), iterator_set_.end(),
                   std::back_inserter(values),
                   [&column](const KnotSetIterator& s) {
                     return s.CurrentKnotValueAt(column);
                   });
    return values;
  }

  int IteratorSetMinimalNextColumn() const {
    return std::min_element(
               iterator_set_.begin(), iterator_set_.end(),
               [](const KnotSetIterator& a, const KnotSetIterator& b) {
                 return a.NextKnotColumn() < b.NextKnotColumn();
               })
        ->NextKnotColumn();
  }

  // Group of KnotSetIterators on which above functions operate
  std::vector<KnotSetIterator> iterator_set_;
};

std::ostream& operator<<(std::ostream& ostream,
                         const SolutionSet& solution_set);

class TreeReconstructor {
 public:
  TreeReconstructor(int indentation_spaces, const BasicFormatStyle& style)
      : current_indentation_spaces_(indentation_spaces), style_(style) {}
  ~TreeReconstructor() = default;

  // Delete standard interfaces
  TreeReconstructor(const TreeReconstructor&) = delete;
  TreeReconstructor(TreeReconstructor&&) = delete;
  TreeReconstructor& operator=(const TreeReconstructor&) = delete;
  TreeReconstructor& operator=(TreeReconstructor&&) = delete;

  void TraverseTree(const LayoutTree& layout_tree);

  void ReplaceTokenPartitionTreeNode(TokenPartitionTree* node) const {
    const auto& first_line = unwrapped_lines_.front();
    const auto& last_line = unwrapped_lines_.back();

    node->Value() = UnwrappedLine(first_line);
    node->Value().SpanUpToToken(last_line.TokensRange().end());
    node->Value().SetIndentationSpaces(current_indentation_spaces_);

    node->Children().clear();
    for (const auto& uwline : unwrapped_lines_) {
      node->AdoptSubtree(uwline);
    }
  }

 private:
  std::vector<UnwrappedLine> unwrapped_lines_;

  UnwrappedLine* active_unwrapped_line_ = nullptr;

  int current_indentation_spaces_;

  const BasicFormatStyle& style_;
};

void OptimizeTokenPartitionTree(TokenPartitionTree* node,
                                const BasicFormatStyle& style);

}  // namespace verible

#endif  // VERIBLE_VERILOG_FORMATTING_LAYOUT_OPTIMIZER_H_
