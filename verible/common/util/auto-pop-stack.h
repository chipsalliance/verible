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

#ifndef VERIBLE_COMMON_UTIL_AUTO_POP_STACK_H_
#define VERIBLE_COMMON_UTIL_AUTO_POP_STACK_H_

#include <cstddef>
#include <vector>

#include "absl/base/attributes.h"
#include "verible/common/util/logging.h"

namespace verible {

// AutoPopStack class automatically handles pushing and popping of managed stack
// and provides public read-only access to random elements of stack and
// iteration over stack elements (forward/reversed). It may be useful on
// implementing algorithms based on stack data structures like building
// inheritance stack of traversed tree.
template <typename T>
class AutoPopStack {
 public:
  using value_type = T;
  using this_type = AutoPopStack<value_type>;

  using reference = value_type &;
  using const_reference = const value_type &;

  using stack_type = std::vector<value_type>;
  using iterator = typename stack_type::iterator;
  using const_iterator = typename stack_type::const_iterator;
  using const_reverse_iterator = typename stack_type::const_reverse_iterator;

  // member class which exclusively manages stack.
  // it's the only class that can modify number of elements in stack
  class AutoPop {
   public:
    AutoPop(this_type *stack, value_type &&value) : stack_(stack) {
      stack->Push(std::move(value));
    }
    AutoPop(this_type *stack, const_reference value) : stack_(stack) {
      stack->Push(&value);
    }
    ~AutoPop() { stack_->Pop(); }

    AutoPop &operator=(const AutoPop &) = delete;  // copy-assign
    AutoPop &operator=(AutoPop &&) = delete;       // move-assign
    AutoPop(const AutoPop &) = delete;             // copy-construct
    AutoPop(AutoPop &&) = delete;                  // move-construct

   private:
    this_type *stack_;
  };

  // returns depth of context stack
  size_t size() const { return stack_.size(); }

  // returns true if the stack is empty
  bool empty() const { return stack_.empty(); }

  // returns the top value_type of the stack
  const_reference top() const {
    CHECK(!stack_.empty());
    return stack_.back();
  }

  // Allow read-only random-access into stack:
  const_iterator begin() const { return stack_.begin(); }
  const_iterator end() const { return stack_.end(); }

  // Reverse iterators be useful for searching from the top-of-stack downward.
  const_reverse_iterator rbegin() const { return stack_.rbegin(); }
  const_reverse_iterator rend() const { return stack_.rend(); }

 protected:
  reference top() {
    CHECK(!stack_.empty());
    return stack_.back();
  }

  // Push a value_type onto the stack (takes r-value reference)
  void Push(value_type &&value) { stack_.push_back(std::move(value)); }

  // Push a value_type onto the stack (copy, takes reference)
  void Push(const_reference value) { stack_.push_back(value); }

  void Push(reference value) { stack_.push_back(value); }

  // Pop the top value_type off of the stack
  void Pop() {
    CHECK(!stack_.empty());
    stack_.pop_back();
  }

 private:
  stack_type stack_;
} ABSL_ATTRIBUTE_UNUSED;

}  // namespace verible

#endif  // VERIBLE_COMMON_UTIL_AUTO_POP_STACK_H_
