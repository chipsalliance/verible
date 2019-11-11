// Copyright 2017-2019 The Verible Authors.
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
#include <initializer_list>
#include <iterator>
#include <vector>

#include "common/text/concrete_syntax_tree.h"
#include "common/util/iterator_adaptors.h"

namespace verible {

// Container with a stack of SyntaxTreeNodes and methods to verify the context
// of a SyntaxTreeNode during traversal of a ConcreteSyntaxTree.
// Note: Public methods are named to follow STL convention for std::stack.
// TODO(fangism): implement a general ForwardMatcher and ReverseMatcher
// interface that can express AND/OR/NOT.
class SyntaxTreeContext {
 public:
  // member class to handle push and pop of stack safely
  class AutoPop {
   public:
    AutoPop(SyntaxTreeContext*, const SyntaxTreeNode& node);  // pushes
    ~AutoPop();                                               // pops
   private:
    SyntaxTreeContext* context_;
  };

  // All elements of stack are non-null because they refer to traversed nodes.
  typedef std::vector<const SyntaxTreeNode*> stack_type;
  typedef stack_type::const_iterator const_iterator;

  // returns depth of context stack
  size_t size() const { return stack_.size(); }

  // returns true if the stack is empty
  bool empty() const { return stack_.empty(); }

  // returns the top SyntaxTreeNode of the stack
  const SyntaxTreeNode& top() const;

  // Allow read-only random-access into stack:
  const_iterator begin() const { return stack_.begin(); }
  const_iterator end() const { return stack_.end(); }

  // TODO(fangism): reverse iterators, rbegin(), rend().
  // These might be useful for searching from the top-of-stack downward.

  // IsInside returns true if there is a node of the specified
  // tag on the TreeContext stack.  Search occurs from the bottom of the
  // stack and returns on the first match found.
  // Type parameter E can be a language-specific enum or plain integer type.
  template <typename E>
  bool IsInside(E tag_enum) const {
    const auto iter = std::find_if(
        begin(), end(),
        [=](const SyntaxTreeNode* node) { return node->MatchesTag(tag_enum); });
    return iter != end();
  }

  // Returns true if current context is directly inside one of the includes
  // node types before any of the excludes node types.  Search starts
  // from the top of the stack.
  template <typename E>
  bool IsInsideFirst(std::initializer_list<E> includes,
                     std::initializer_list<E> excludes) const {
    for (const auto& type : reversed_view(stack_)) {
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
    if (tag_enums.size() > stack_.size()) return false;
    // top of stack is back of vector (direct parent)
    return std::equal(tag_enums.begin(), tag_enums.end(), stack_.rbegin(),
                      [](E tag, const SyntaxTreeNode* node) {
                        return E(node->Tag().tag) == tag;
                      });
  }

 protected:
  // Pop the top SyntaxTreeNode off of the stack
  void Pop();

  // Push a SyntaxTreeNode onto the stack (takes address)
  void Push(const SyntaxTreeNode& node);

  // Stack of ancestors of the current node that is updated as the tree
  // is traversed. Top of the stack is closest ancestor.
  // A vector is chosen to allow random access and searches from either end of
  // the stack.
  stack_type stack_;
};

}  // namespace verible

#endif  // VERIBLE_COMMON_TEXT_SYNTAX_TREE_CONTEXT_H_
