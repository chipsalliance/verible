// Copyright 2017-2022 The Verible Authors.
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

#ifndef VERIBLE_COMMON_UTIL_CONTAINER_PROXY_H_
#define VERIBLE_COMMON_UTIL_CONTAINER_PROXY_H_

#include <initializer_list>
#include <iterator>
#include <type_traits>
#include <utility>

namespace verible {

// CRTP base class for creating modification tracking STL container proxies.
//
// The class allows safe exposition of a STL container to an user in cases where
// changes to the container must be pre-/post-processed. This is done through
// implementation of several optional event handling methods in a derived class.
// Event handling methods that are not implemented do not introduce overhead.
//
// Example use case: List of child nodes in a tree which sets parent when
// a child is inserted.
//
// Note that the proxy only tracks container changes. It does not track
// individual elements.
//
// Derived class interface
// -----------------------
//
// - Mandatory:
//   - `ContainerType& underlying_container()`,
//     `const ContainerType& underlying_container() const`:
//     Returns reference to the wrapped container.
//
// - Optional:
//   - `void ElementsInserted(iterator first, iterator last)`:
//     Called when new elements were inserted. `first` and `last` (exclusive)
//     refer to inserted elements.
//   - `void ElementsBeingRemoved(iterator first, iterator last)`:
//     Called just before removing elements from `first` to `last` (exclusive).
//   - `void ElementsBeingReplaced()`:
//     Called when all elements are going to be replaced (due to assignment).
//   - `void ElementsWereReplaced()`:
//     Called just after all elements were replaced.
//
// Usage
// -----
//
// 1. Subclass:
//
//        using ContainerType = std::vector<int>
//        class MyProxy: private ContainerProxyBase<MyProxy, ContainerType> {
//          using Base = ContainerProxyBase<MyProxy, ContainerType>;
//          friend Base;
//          // ...
//        };
//
// 2. Implement mandatory interface:
//
//        private:
//          ContainerType& underlying_container() {
//            return container_;
//          }
//          const ContainerType& underlying_container() const {
//            return container_;
//          }
//        // ...
//        ContainerType container_; // not a part of the interface
//
// 3. Inherit public interface (all or only some members):
//
//        public:
//          using typename Base::container_type;
//          using typename Base::value_type;
//          using typename Base::reference;
//          using typename Base::const_reference;
//          // ...
//          using Base::begin;
//          using Base::end;
//          // ...
//
//    Note: inheriting (or implementing) `void swap(MyProxy&)` will also provide
//    standalone function `void swap(MyProxy&, MyProxy&)`.
//
// 4. Implement optional interface:
//
//        private:
//          void ElementsInserted(iterator first, iterator last) {
//            /* ... */
//          }
//          void ElementsBeingRemoved(iterator first, iterator last) {
//            /* ... */
//          }
//          void ElementsBeingReplaced() { /* ... */ }
//          void ElementsWereReplaced() { /* ... */ }
//
// 5. Optional: implement operator=(const MyProxy&) and operator=(MyProxy&&)
//    (base class' `operator=` works only with `ContainerType`):
//
//        public:
//          MyProxy& operator=(const MyProxy& other) {
//            *this = other.underlying_container();
//            return *this;
//          }
//          MyProxy& operator=(MyProxy&& other) {
//            // You probably want to notify `other` about invalidation
//            // of its **whole container** (not just elements).
//            *this = std::move(other.underlying_container());
//            return *this;
//          }
//
// TODO(mglb): The class name is not really conveying what the class does.
// Find a better one. More details:
// https://github.com/chipsalliance/verible/pull/1244#discussion_r821964959
template <class DerivedType, class ContainerType>
class ContainerProxyBase {
  static_assert(!std::is_const_v<ContainerType>);
  static_assert(!std::is_reference_v<ContainerType>);
  static_assert(!std::is_pointer_v<ContainerType>);

 public:
  ContainerProxyBase() = default;

  // Copy/move of the base class doesn't make sense.
  ContainerProxyBase(const ContainerProxyBase &) = delete;
  ContainerProxyBase(ContainerProxyBase &&) = delete;
  ContainerProxyBase &operator=(const ContainerProxyBase &) = delete;
  ContainerProxyBase &operator=(ContainerProxyBase &&) = delete;

  using container_type = ContainerType;

  using value_type = typename ContainerType::value_type;
  using reference = typename ContainerType::reference;
  using const_reference = typename ContainerType::const_reference;
  using iterator = typename ContainerType::iterator;
  using const_iterator = typename ContainerType::const_iterator;
  using difference_type = typename ContainerType::difference_type;
  using size_type = typename ContainerType::size_type;

  using reverse_iterator = typename ContainerType::reverse_iterator;
  using const_reverse_iterator = typename ContainerType::const_reverse_iterator;

  // Iteration

  iterator begin() { return container().begin(); }
  const_iterator begin() const { return container().begin(); }
  const_iterator cbegin() const { return container().cbegin(); }

  iterator end() { return container().end(); }
  const_iterator end() const { return container().end(); }
  const_iterator cend() const { return container().cend(); }

  reverse_iterator rbegin() { return container().rbegin(); }
  const_reverse_iterator rbegin() const { return container().rbegin(); }
  const_reverse_iterator crbegin() const { return container().crbegin(); }

  reverse_iterator rend() { return container().rend(); }
  const_reverse_iterator rend() const { return container().rend(); }
  const_reverse_iterator crend() const { return container().crend(); }

  // Element access

  reference front() { return container().front(); }
  const_reference front() const { return container().front(); }

  reference back() { return container().back(); }
  const_reference back() const { return container().back(); }

  reference operator[](size_type index) noexcept { return container()[index]; }
  const_reference operator[](size_type index) const noexcept {
    return container()[index];
  }

  reference at(size_type index) { return container().at(index); }
  const_reference at(size_type index) const { return container().at(index); }

  // Modifiers (inserting)

  // TODO(mglb): provide methods taking `iterator` instead of `const_iterator`.
  // In most cases `iterator` is convertible to `const_iterator` without any
  // cost, but that is not guaranteed.

  template <typename... Args>
  iterator emplace(const_iterator pos, Args &&...args) {
    const auto iter = container().emplace(pos, std::forward<Args>(args)...);
    CallElementsInserted(iter);
    return iter;
  }

  template <typename... Args>
  void emplace_front(Args &&...args) {
    container().emplace_front(std::forward<Args>(args)...);
    CallElementsInserted(container().begin());
  }

  template <typename... Args>
  void emplace_back(Args &&...args) {
    container().emplace_back(std::forward<Args>(args)...);
    CallElementsInserted(std::prev(container().end()));
  }

  iterator insert(const_iterator pos, const value_type &value) {
    const auto iter = container().insert(pos, value);
    CallElementsInserted(iter);
    return iter;
  }

  iterator insert(const_iterator pos, size_type count,
                  const value_type &value) {
    const auto iter = container().insert(pos, count, value);
    CallElementsInserted(iter, std::next(iter, count));
    return iter;
  }

  iterator insert(const_iterator pos, value_type &&value) {
    const auto iter = container().insert(pos, std::move(value));
    CallElementsInserted(iter);
    return iter;
  }

  template <typename InputIterator>
  iterator insert(const_iterator pos, InputIterator first, InputIterator last) {
    const auto iter = container().insert(pos, first, last);
    const auto end_iter = std::next(iter, std::distance(first, last));
    CallElementsInserted(iter, end_iter);
    return iter;
  }

  iterator insert(const_iterator pos,
                  std::initializer_list<value_type> values) {
    const auto iter = container().insert(pos, values);
    const auto end_iter = std::next(iter, values.size());
    CallElementsInserted(iter, end_iter);
    return iter;
  }

  void push_front(const value_type &value) {
    container().push_front(value);
    CallElementsInserted(container().begin());
  }
  void push_front(value_type &&value) {
    container().push_front(std::move(value));
    CallElementsInserted(container().begin());
  }

  void push_back(const value_type &value) {
    container().push_back(value);
    CallElementsInserted(std::prev(container().end()));
  }
  void push_back(value_type &&value) {
    container().push_back(std::move(value));
    CallElementsInserted(std::prev(container().end()));
  }

  // Modifiers (removing)

  iterator erase(iterator pos) {
    CallElementsBeingRemoved(pos);
    return container().erase(pos);
  }

  iterator erase(const_iterator pos) {
    CallElementsBeingRemoved(ConvertToMutableIterator(pos));
    return container().erase(pos);
  }

  iterator erase(iterator first, iterator last) {
    CallElementsBeingRemoved(first, last);
    return container().erase(first, last);
  }

  iterator erase(const_iterator first, const_iterator last) {
    CallElementsBeingRemoved(ConvertToMutableIterator(first),
                             ConvertToMutableIterator(last));
    return container().erase(first, last);
  }

  void pop_front() {
    CallElementsBeingRemoved(container().begin());
    container().pop_front();
  }

  void pop_back() {
    CallElementsBeingRemoved(std::prev(container().end()));
    container().pop_back();
  }

  void clear() {
    CallElementsBeingRemoved(begin(), end());
    container().clear();
  }

  // Assignment

  template <typename InputIterator>
  void assign(InputIterator first, InputIterator last) {
    CallElementsBeingReplaced();
    container().assign(first, last);
    CallElementsWereReplaced();
  }

  void assign(std::initializer_list<value_type> values) {
    CallElementsBeingReplaced();
    container().assign(values);
    CallElementsWereReplaced();
  }

  void assign(size_type count, const value_type &value) {
    CallElementsBeingReplaced();
    container().assign(count, value);
    CallElementsWereReplaced();
  }

  // Intended to be exposed via `using` in `DerivedType`.
  // NOLINTNEXTLINE(misc-unconventional-assign-operator)
  DerivedType &operator=(const ContainerType &other_container) {
    CallElementsBeingReplaced();
    container() = other_container;
    CallElementsWereReplaced();
    return *derived();
  }

  // Intended to be exposed via `using` in `DerivedType`.
  // NOLINTNEXTLINE(misc-unconventional-assign-operator)
  DerivedType &operator=(ContainerType &&other_container) noexcept {
    CallElementsBeingReplaced();
    container() = std::move(other_container);
    CallElementsWereReplaced();
    return *derived();
  }

  // Intended to be exposed via `using` in `DerivedType`.
  // NOLINTNEXTLINE(misc-unconventional-assign-operator)
  DerivedType &operator=(std::initializer_list<value_type> values) {
    CallElementsBeingReplaced();
    container() = values;
    CallElementsWereReplaced();
    return *derived();
  }

  void swap(ContainerType &other) noexcept {
    CallElementsBeingReplaced();
    container().swap(other);
    CallElementsWereReplaced();
  }

  void swap(DerivedType &other) noexcept {
    CallElementsBeingReplaced();
    other.CallElementsBeingReplaced();
    container().swap(other.container());
    CallElementsWereReplaced();
    other.CallElementsWereReplaced();
  }

  // Standalone `swap` function available only if `DerivedType` implements
  // `swap(DerivedType&)` method (either through importing this class' method
  // via `using` or implementing their own version).
  template <class DT_ = DerivedType,
            class = decltype(std::declval<DT_>().swap(std::declval<DT_ &>()))>
  friend void swap(DerivedType &a, DerivedType &b) noexcept {
    a.swap(b);
  }

  // Capacity

  size_type size() const { return container().size(); }
  size_type max_size() const { return container().max_size(); }
  bool empty() const { return container().empty(); }

  size_type capacity() const { return container().capacity(); }

  void reserve(size_type count) { container().reserve(count); }

  void resize(size_type count) {
    const auto initial_size = container().size();
    if (count < initial_size) {
      const iterator first_removed = std::next(container().begin(), count);
      CallElementsBeingRemoved(first_removed, container().end());
    }
    container().resize(count);
    if (count > initial_size) {
      const iterator first_inserted =
          std::next(container().begin(), initial_size);
      CallElementsInserted(first_inserted, container().end());
    }
  }

  void resize(size_type count, const value_type &value) {
    const auto initial_size = container().size();
    if (count < initial_size) {
      const iterator first_removed = std::next(container().begin(), count);
      CallElementsBeingRemoved(first_removed, container().end());
    }
    container().resize(count, value);
    if (count > initial_size) {
      const iterator first_inserted =
          std::next(container().begin(), initial_size);
      CallElementsInserted(first_inserted, container().end());
    }
  }

 protected:
  // Derived class interface

  // Mandatory:
  // ContainerType& underlying_container();
  // const ContainerType& underlying_container() const;

  // Optional:
  void ElementsInserted(iterator first, iterator last) {}
  void ElementsBeingRemoved(iterator first, iterator last) {}
  void ElementsBeingReplaced() {}
  void ElementsWereReplaced() {}

 private:
  iterator ConvertToMutableIterator(const_iterator iter) {
    return std::next(container().begin(),
                     std::distance(container().cbegin(), iter));
  }

  auto *derived() { return static_cast<DerivedType *>(this); }
  const auto *derived() const { return static_cast<const DerivedType *>(this); }

  ContainerType &container() { return derived()->underlying_container(); }
  const ContainerType &container() const {
    return derived()->underlying_container();
  }

  void CallElementsInserted(iterator element) {
    derived()->ElementsInserted(element, std::next(element));
  }
  void CallElementsInserted(iterator first, iterator last) {
    derived()->ElementsInserted(first, last);
  }
  void CallElementsBeingRemoved(iterator element) {
    derived()->ElementsBeingRemoved(element, std::next(element));
  }
  void CallElementsBeingRemoved(iterator first, iterator last) {
    derived()->ElementsBeingRemoved(first, last);
  }
  void CallElementsBeingReplaced() { derived()->ElementsBeingReplaced(); }
  void CallElementsWereReplaced() { derived()->ElementsWereReplaced(); }
};

// Macros for importing common sets of members in classes deriving from
// ContainerProxyBase.
// A class deriving from ContainerProxyBase can optionally use **one**
// `USING_CONTAINER_PROXY_*_MEMBERS` macro.

// Imports (via `using`) members required by C++ Container.
// Can be called only inside body of a class deriving from ContainerProxyBase.
// `base_type` must be inherited ContainerProxyBase class type.
#define USING_CONTAINER_PROXY_CONTAINER_MEMBERS(base_type) \
  using typename base_type::value_type;                    \
  using typename base_type::reference;                     \
  using typename base_type::const_reference;               \
  using typename base_type::iterator;                      \
  using typename base_type::const_iterator;                \
  using typename base_type::difference_type;               \
  using typename base_type::size_type;                     \
  using base_type::begin;                                  \
  using base_type::end;                                    \
  using base_type::cbegin;                                 \
  using base_type::cend;                                   \
  using base_type::size;                                   \
  using base_type::max_size;                               \
  using base_type::empty;                                  \
  using base_type::operator=;                              \
  using base_type::swap;

// **INTERNAL, DO NOT USE DIRECTLY IN CLASSES!**
// Imports (via `using`) extra members required by C++ Reversible Container.
// `base_type` must be inherited ContainerProxyBase class type.
#define INTERNAL_USING_CONTAINER_PROXY_REVERSIBLE_CONTAINER_MEMBERS(base_type) \
  using typename base_type::reverse_iterator;                                  \
  using typename base_type::const_reverse_iterator;                            \
  using base_type::rbegin;                                                     \
  using base_type::rend;                                                       \
  using base_type::crbegin;                                                    \
  using base_type::crend;

// Imports (via `using`) additional members required by C++ Sequence Container.
// Can be called only inside body of a class deriving from ContainerProxyBase.
// `base_type` must be inherited ContainerProxyBase class type.
#define USING_CONTAINER_PROXY_SEQUENCE_CONTAINER_MEMBERS(base_type) \
  USING_CONTAINER_PROXY_CONTAINER_MEMBERS(base_type)                \
  using base_type::emplace;                                         \
  using base_type::insert;                                          \
  using base_type::erase;                                           \
  using base_type::clear;                                           \
  using base_type::assign;                                          \
  using base_type::front;

// Imports (via `using`) members supported by std::vector.
// Can be called only inside body of a class deriving from ContainerProxyBase.
// `base_type` must be inherited ContainerProxyBase class type.
#define USING_CONTAINER_PROXY_STD_VECTOR_MEMBERS(base_type)              \
  USING_CONTAINER_PROXY_SEQUENCE_CONTAINER_MEMBERS(base_type)            \
  INTERNAL_USING_CONTAINER_PROXY_REVERSIBLE_CONTAINER_MEMBERS(base_type) \
  using base_type::back;                                                 \
  using base_type::emplace_front;                                        \
  using base_type::emplace_back;                                         \
  using base_type::push_front;                                           \
  using base_type::push_back;                                            \
  using base_type::pop_front;                                            \
  using base_type::pop_back;                                             \
  using base_type::operator[];                                           \
  using base_type::at;                                                   \
  using base_type::capacity;                                             \
  using base_type::reserve;                                              \
  using base_type::resize;

// Imports (via `using`) members supported by std::list.
// Can be called only inside body of a class deriving from ContainerProxyBase.
// `base_type` must be inherited ContainerProxyBase class type.
#define USING_CONTAINER_PROXY_STD_LIST_MEMBERS(base_type)                \
  USING_CONTAINER_PROXY_SEQUENCE_CONTAINER_MEMBERS(base_type)            \
  INTERNAL_USING_CONTAINER_PROXY_REVERSIBLE_CONTAINER_MEMBERS(base_type) \
  using base_type::back;                                                 \
  using base_type::emplace_front;                                        \
  using base_type::emplace_back;                                         \
  using base_type::push_front;                                           \
  using base_type::push_back;                                            \
  using base_type::pop_front;                                            \
  using base_type::pop_back;                                             \
  using base_type::resize;

}  // namespace verible

#endif  // VERIBLE_COMMON_UTIL_CONTAINER_PROXY_H_
