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

#ifndef VERIBLE_COMMON_TEXT_SYNTAX_TREE_CONTEXT_H_
#define VERIBLE_COMMON_TEXT_SYNTAX_TREE_CONTEXT_H_

#include <algorithm>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <iterator>

#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/util/auto-pop-stack.h"
#include "verible/common/util/iterator-adaptors.h"
#include "verible/common/util/logging.h"

namespace verible {

// Container with a stack of SyntaxTreeNodes and methods to verify the context
// of a SyntaxTreeNode during traversal of a ConcreteSyntaxTree.
// Note: Public methods are named to follow STL convention for std::stack.
// TODO(fangism): implement a general ForwardMatcher and ReverseMatcher
// interface that can express AND/OR/NOT.
// Despite of implementation based on pointers. This class requires
// that managed elements are non-nullptrs.
class SyntaxTreeContext : public AutoPopStack<const SyntaxTreeNode *> {
 public:
  using base_type = AutoPopStack<const SyntaxTreeNode *>;

  // member class to handle push and pop of stack safely
  using AutoPop = base_type::AutoPop;

 protected:
  // restrict access to AutoPopStack<>::top method only to this class
  using base_type::top;

 public:
  // returns the top SyntaxTreeNode of the stack
  const SyntaxTreeNode &top() const {
    return *ABSL_DIE_IF_NULL(base_type::top());
  }

  // IsInside returns true if there is a node of the specified
  // tag on the TreeContext stack.  Search traverses from the top of the
  // stack starting with offset and returns on the first match found.
  // Type parameter E can be a language-specific enum or plain integer type.
  template <typename E>
  bool IsInsideStartingFrom(E tag_enum, size_t reverse_offset) const {
    if (size() <= reverse_offset) return false;
    return std::any_of(
        rbegin() + reverse_offset, rend(),
        [=](const SyntaxTreeNode *node) { return node->MatchesTag(tag_enum); });
  }

  // IsInside returns true if there is a node of the specified
  // tag on the TreeContext stack.  Search occurs from the top of the
  // stack and returns on the first match found.
  // Type parameter E can be a language-specific enum or plain integer type.
  template <typename E>
  bool IsInside(E tag_enum) const {
    return IsInsideStartingFrom(tag_enum, 0);
  }

  // Returns true if current context is directly inside one of the includes
  // node types before any of the excludes node types.  Search starts
  // from the top of the stack.
  template <typename E>
  bool IsInsideFirst(std::initializer_list<E> includes,
                     std::initializer_list<E> excludes) const {
    for (const auto &type : reversed_view(*this)) {
      if (type->MatchesTagAnyOf(includes)) return true;
      if (type->MatchesTagAnyOf(excludes)) return false;
    }
    return false;
  }

  // Returns true if stack is not empty and top of stack matches tag_enum.
  template <typename E>
  bool DirectParentIs(E tag_enum) const {
    if (empty()) {
      return false;
    }
    return E(top().Tag().tag) == tag_enum;
  }

  // Returns true if stack is not empty and top of stack matches
  // one of the tag_enums.
  template <typename E>
  bool DirectParentIsOneOf(std::initializer_list<E> tag_enums) const {
    if (empty()) {
      return false;
    }
    return std::find(tag_enums.begin(), tag_enums.end(), E(top().Tag().tag)) !=
           tag_enums.end();
  }

  // Returns true if the immediate parents are the given sequence (top-down).
  // Sequence should be specified as: direct-parent, direct-grandparent, ...
  // In the degenerate empty-list case, this will return true.
  template <typename E>
  bool DirectParentsAre(std::initializer_list<E> tag_enums) const {
    if (tag_enums.size() > size()) return false;
    // top of stack is back of vector (direct parent)
    return std::equal(tag_enums.begin(), tag_enums.end(), rbegin(),
                      [](E tag, const SyntaxTreeNode *node) {
                        return E(node->Tag().tag) == tag;
                      });
  }

  // Returns the closest ancestor (starting from top of context stack) that
  // matches the given 'predicate' function, or nullptr if no match is found.
  const SyntaxTreeNode *NearestParentMatching(
      const std::function<bool(const SyntaxTreeNode &)> &predicate) const {
    const auto ancestors(reversed_view(*this));
    const auto found = std::find_if(ancestors.begin(), ancestors.end(),
                                    [&predicate](const SyntaxTreeNode *parent) {
                                      return predicate(*parent);
                                    });
    return found != ancestors.end() ? *found : nullptr;
  }

  // Returns the closest ancestor (starting from top of context stack) with the
  // specified node tag (enum).
  template <typename E>
  const SyntaxTreeNode *NearestParentWithTag(E tag) const {
    return NearestParentMatching(
        [tag](const SyntaxTreeNode &node) { return E(node.Tag().tag) == tag; });
  }
};

}  // namespace verible

#endif  // VERIBLE_COMMON_TEXT_SYNTAX_TREE_CONTEXT_H_
