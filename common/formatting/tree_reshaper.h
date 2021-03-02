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

#ifndef VERIBLE_COMMON_TREE_UNWRAPPER_H_
#define VERIBLE_COMMON_TREE_UNWRAPPER_H_

#include <vector>

#include "common/formatting/basic_format_style.h"
#include "common/formatting/token_partition_tree.h"
#include "common/formatting/unwrapped_line.h"
#include "common/util/vector_tree.h"

namespace verible {

class TreeReshaper {
 public:

  class Knot;
  class KnotSet;
  class Layout;

  using SolutionSet = std::vector<const KnotSet*>;
  using MutableSolutionSet = std::vector<KnotSet*>;
  using LayoutTree = VectorTree<Layout>;

  using Block = Layout;
  using BlockTree = VectorTree<Layout>;

  enum class LayoutType {
      // primitive
      kText,

      // basic abstract
      kLine,
      kStack,

      // 'higher' level abstract
      kChoice,
      kWrap,
  };

  struct Layout {
    Layout(LayoutType type) : type_(type) {}
    Layout(const UnwrappedLine& uwline)
        : uwline_(uwline), type_(LayoutType::kText) {}

    Layout(const Layout&) = default;
    Layout(Layout&&) = default;
    Layout& operator=(const Layout&) = default;
    Layout& operator=(Layout&&) = default;
    ~Layout() = default;

    Layout() = delete;

    // Deleting standard interfaces:

    UnwrappedLine uwline_;
    LayoutType type_;
  };

  struct Knot {
    Knot(int column, int span, int intercept, int gradient,
         const LayoutTree* layout, int before_spaces = 0)
        : column_(column), span_(span), intercept_(intercept),
          gradient_(gradient), layout_(layout), before_spaces_(before_spaces)  {
      if (layout_ != nullptr && layout_->Value().type_ == LayoutType::kText) {
        const auto& uwline = layout_->Value().uwline_;
        if (uwline.Size() > 0) {
          before_spaces_ = uwline.TokensRange().front().before.spaces_required;
        }
      }
    }

    Knot(const Knot&) = default;
    Knot(Knot&&) = default;
    ~Knot() = default;

    // Deleting standard interfaces:
    Knot() = delete;
    Knot& operator=(const Knot&) = delete;
    Knot& operator=(Knot&&) = delete;

    int column_;
    int span_;
    int intercept_;
    int gradient_;
    // FIXME(ldk): Replace with std::unique_ptr<>?
    const LayoutTree* layout_;

    int before_spaces_;
  };

  // Horizontal merge
  static KnotSet* HPlusSolution(const KnotSet& left,
                                const KnotSet& right,
                                const BasicFormatStyle& style);

  // Vertical merge
  static KnotSet* VSumSolution(const SolutionSet& solution_set,
                               const BasicFormatStyle& style);

  // Find minimal solution
  static KnotSet* MinSolution(const SolutionSet& solution_set,
                              const BasicFormatStyle& style);

  static KnotSet* ComputeSolution(const BlockTree& tree,
                                  const KnotSet& rest_of_line,
                                  const BasicFormatStyle& style);

  static LayoutTree* BuildLayoutTreeFromTokenPartitionTree(
      const TokenPartitionTree& token_partition_tree);

  static void ReshapeTokenPartitionTree(TokenPartitionTree* tree,
                                        const BasicFormatStyle style);

  // Generate reshaped TokenPartitionTree from LayoutTree
  static TokenPartitionTree* BuildTokenPartitionTree(
      const LayoutTree& layout);

  class KnotSet {
   public:
    KnotSet() = default;
    ~KnotSet() = default;

    // Deleting standard interfaces:
    KnotSet(KnotSet&&) = delete;
    KnotSet(const KnotSet&) = delete;
    KnotSet& operator=(const KnotSet&) = delete;
    KnotSet& operator=(KnotSet&&) = delete;

    void AppendKnot(const Knot& knot) {
      knots_.push_back(knot);
    }

    size_t size() const {
      return knots_.size();
    }

    const Knot& operator[](int idx) const {
      // FIXME(ldk): assert/check
      return knots_[idx];
    }

    // FIXME(ldk): Rename as KnotSetIterator (or KnotIterator) and
    //     use as iterator (e.g. usign iterator = KnotSetIterator)?
    class iterator : public std::iterator<std::forward_iterator_tag,
                                          const Knot> {
      friend class KnotSet;
     private:
      explicit iterator(const KnotSet& knot_set, int index)
          : knot_set_(knot_set), index_(index) { }

     public:
      iterator(const iterator&) = default;

      // pre-increment
      iterator& operator++() {
        index_ += 1;
        return *this;
      }

      // post-increment
      iterator operator++(int) {
        auto iter = *this;
        ++(*this);
        return iter;
      }

      bool operator==(const iterator& rhs) const { return index_ == rhs.index_; }

      bool operator!=(const iterator& rhs) const { return !(*this == rhs); }

      const Knot& operator*() const { return knot_set_.knots_[index_]; }
      const Knot* operator->() const { return &knot_set_.knots_[index_]; }

      int ValueAt(int m) const {
        auto& iter = *this;
        return iter->intercept_ + iter->gradient_ * (m - iter->column_);
      }

      int NextKnot() const {
        if ((index_+1) >= static_cast<signed>(knot_set_.knots_.size())) {
          return std::numeric_limits<int>::max();
        } else {
          return knot_set_.knots_[index_+1].column_;
        }
      }

      // FIXME(ldk): Test if index_ is in ranage of elements_.size()
      void MoveToMargin(int m) {
        if (CurrentColumn() > m) {
          while (CurrentColumn() > m) {
            index_ -= 1;
          }
        } else {
          while (NextKnot() <= m) {
            index_ += 1;
          }
        }
      }

     private:
      int CurrentColumn() const {
        return knot_set_.knots_[index_].column_;
      }

      const KnotSet& knot_set_;
      int index_;
    };

    using const_iterator = iterator;

    // Iteration

    iterator begin() const { return iterator(*this, 0); }
    iterator end() const { return iterator(*this, knots_.size()); }

    // FIXME(ldk): Remove this?
    KnotSet* Clone() const {
      KnotSet* ret = new KnotSet{};
      for (auto& itr : knots_) {
        ret->knots_.push_back(itr);  // copy
      }
      return ret;
    }

    // Creates new Solution with added const_value
    // FIXME(ldk): Maybe we could avoid creating new solution and
    //     modify this one?
    KnotSet* PlusConst(int const_value) const {
      KnotSet* ret = Clone();
      for (auto& itr : ret->knots_) {
        itr.intercept_ += const_value;
      }
      return ret;
    }

    KnotSet* WithRestOfLine(const TreeReshaper::KnotSet& rest_of_line,
                            const BasicFormatStyle& style) const {
      if (rest_of_line.size() == 0) {
        return Clone();
      } else {
        return HPlusSolution(*this, rest_of_line, style);
      }
    }

   private:
    std::vector<Knot> knots_;
  };  // class KnotSet

 private:
};

std::ostream& operator<<(std::ostream& stream, const TreeReshaper::Layout& layout);

}  // namespace verible

#endif /* VERIBLE_COMMON_TREE_UNWRAPPER_H_ */
