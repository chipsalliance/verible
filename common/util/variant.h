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

#ifndef VERIBLE_COMMON_UTIL_VARIANT_H_
#define VERIBLE_COMMON_UTIL_VARIANT_H_

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <new>
#include <type_traits>

#include "common/util/logging.h"
#include "common/util/type_traits.h"

namespace verible {

template <typename... TypeN>
class Variant;

// Internal helpers
// ================

namespace variant_internal {

template <class T, class... TypeN>
constexpr T& GetWithoutChecks(verible::Variant<TypeN...>& var) {
  return var.template GetWithoutChecks<T>();
}

template <class T, class... TypeN>
constexpr T&& GetWithoutChecks(verible::Variant<TypeN...>&& var) {
  return std::move(var.template GetWithoutChecks<T>());
}

template <class T, class... TypeN>
constexpr const T& GetWithoutChecks(const verible::Variant<TypeN...>& var) {
  return var.template GetWithoutChecks<T>();
}

template <class T, class... TypeN>
constexpr const T&& GetWithoutChecks(const verible::Variant<TypeN...>&& var) {
  return std::move(var.template GetWithoutChecks<T>());
}

template <uint8_t I, class... TypeN>
constexpr auto& GetWithoutChecks(verible::Variant<TypeN...>& var) {
  return var.template GetWithoutChecks<I>();
}

template <uint8_t I, class... TypeN>
constexpr auto&& GetWithoutChecks(verible::Variant<TypeN...>&& var) {
  return std::move(var.template GetWithoutChecks<I>());
}

template <uint8_t I, class... TypeN>
constexpr const auto& GetWithoutChecks(const verible::Variant<TypeN...>& var) {
  return var.template GetWithoutChecks<I>();
}

template <uint8_t I, class... TypeN>
constexpr const auto&& GetWithoutChecks(
    const verible::Variant<TypeN...>&& var) {
  return std::move(var.template GetWithoutChecks<I>());
}

template <class U, class... TN>
struct type_index {};

template <class U, class... TN>
struct type_index<U, U, TN...> : std::integral_constant<uint8_t, 0> {};

template <class U, class T0, class... TN>
struct type_index<U, T0, TN...>
    : std::integral_constant<uint8_t, 1 + type_index<U, TN...>::value> {};

template <class U, class T0, class... TN>
inline constexpr uint8_t type_index_v = type_index<U, T0, TN...>::value;

template <uint8_t I, class T0, class... TN>
struct type_at_index {
  using type = typename type_at_index<I - 1, TN...>::type;
};

template <class T0, class... TN>
struct type_at_index<0, T0, TN...> {
  using type = T0;
};

template <uint8_t I, class... TN>
using type_at_index_t = typename type_at_index<I, TN...>::type;

}  // namespace variant_internal

// Overload
// ========
// TODO(mglb): move to overload.h

template <typename... LambdaN>
class Overload : public LambdaN... {
 public:
  explicit Overload(LambdaN&&... lambda_n)
      : LambdaN(std::forward<LambdaN>(lambda_n))... {}

  using LambdaN::operator()...;
};
template <typename... LambdaN>
Overload(LambdaN...) -> Overload<LambdaN...>;

// variant_size / variant_size_v
// =============================

template <class VariantType>
struct variant_size {};

template <class... TypeN>
struct variant_size<verible::Variant<TypeN...>> {
  static constexpr uint8_t value = sizeof...(TypeN);
};

template <class VariantType>
inline constexpr uint8_t variant_size_v = variant_size<VariantType>::value;

// variant_alternative / variant_alternative_t
// ===========================================

template <uint8_t Index, class VariantType>
struct variant_alternative {};

template <uint8_t Index, class... TypeN>
struct variant_alternative<Index, Variant<TypeN...>> {
  using type = variant_internal::type_at_index_t<Index, TypeN...>;
};

template <uint8_t Index, class VariantType>
using variant_alternative_t =
    typename variant_alternative<Index, VariantType>::type;

// Visit()
// =======

namespace variant_internal {

template <uint8_t Index = 0, class Visitor, class VariantType>
inline decltype(auto) CallVisitorForActiveIndex(Visitor&& vis,
                                                VariantType&& var) {
  static constexpr uint8_t max_index =
      variant_size_v<verible::remove_cvref_t<VariantType>> - 1;

  if constexpr (Index < max_index) {
    if (var.index() == Index) {
      return vis(GetWithoutChecks<Index>(std::forward<VariantType>(var)));
    }
    return CallVisitorForActiveIndex<Index + 1>(std::forward<Visitor>(vis),
                                                std::forward<VariantType>(var));
  } else {
    static_assert(Index == max_index);
    return vis(GetWithoutChecks<Index>(std::forward<VariantType>(var)));
  }
}

template <class Visitor, class VariantType>
inline decltype(auto) Visit(Visitor&& vis, VariantType&& var) {
  return CallVisitorForActiveIndex(std::forward<Visitor>(vis),
                                   std::forward<VariantType>(var));
}

}  // namespace variant_internal

template <class Visitor, class... TypeN>
decltype(auto) Visit(Visitor&& vis, Variant<TypeN...>& var) {
  return variant_internal::Visit(std::forward<Visitor>(vis), var);
}

template <class Visitor, class... TypeN>
decltype(auto) Visit(Visitor&& vis, Variant<TypeN...>&& var) {
  return variant_internal::Visit(std::forward<Visitor>(vis), std::move(var));
}

template <class Visitor, class... TypeN>
decltype(auto) Visit(Visitor&& vis, const Variant<TypeN...>& var) {
  return variant_internal::Visit(std::forward<Visitor>(vis), var);
}

template <class Visitor, class... TypeN>
decltype(auto) Visit(Visitor&& vis, const Variant<TypeN...>&& var) {
  return variant_internal::Visit(std::forward<Visitor>(vis), std::move(var));
}

// Tags for in-place Variant construction
// ======================================

template <class T>
struct in_place_type_t {
  explicit in_place_type_t() = default;
};

template <class T>
inline constexpr in_place_type_t<T> in_place_type{};

// std's version uses size_t instead of uint8_t.
// TODO(mglb): try taking size_t index in constructors and cast it.
template <uint8_t I>
struct in_place_index_t {
  explicit in_place_index_t() = default;
};

template <uint8_t I>
inline constexpr in_place_index_t<I> in_place_index{};

// Variant
// =======

template <typename... TypeN>
class Variant {
  using IndexType = uint8_t;

  static_assert(sizeof...(TypeN) > 0);
  static_assert(sizeof...(TypeN) <= std::numeric_limits<IndexType>::max());
  static_assert((!std::is_reference_v<TypeN> && ...));
  static_assert((!std::is_const_v<TypeN> && ...));
  static_assert((!std::is_volatile_v<TypeN> && ...));
  static_assert((!std::is_array_v<TypeN> && ...));

  // Helpers for querying type list
  // ------------------------------

  template <typename T>
  static inline constexpr IndexType type_index_v =
      variant_internal::type_index_v<T, TypeN...>;

  template <IndexType I>
  using type_at_index_t =
      typename variant_internal::type_at_index_t<I, TypeN...>;

  template <class T>
  static inline constexpr bool type_list_contains_v =
      (std::is_same_v<remove_cvref_t<T>, TypeN> || ...);

  template <class T>
  static inline constexpr bool type_list_contains_type_or_its_subclass_v =
      ((std::is_same_v<remove_cvref_t<T>, TypeN> ||
        std::is_base_of_v<remove_cvref_t<T>, TypeN>) ||
       ...);

 public:
  // Constructors & destructor
  // -------------------------

  // Enable default constructor only if supported by the first type.
  template <class Type0 = type_at_index_t<0>,
            std::enable_if_t<std::is_default_constructible_v<Type0>>* = nullptr>
  constexpr Variant() noexcept {
    ::new (static_cast<void*>(storage_)) Type0;
    index_ = 0;
  }

  // Delete default constructor if not supported by the first type.
  template <
      class Type0 = type_at_index_t<0>,
      std::enable_if_t<!std::is_default_constructible_v<Type0>>* = nullptr>
  Variant() = delete;

  // TODO(mglb): enable conditionally
  constexpr Variant(const Variant& other) { Construct(other); }

  // TODO(mglb): enable conditionally
  constexpr Variant(Variant&& other) noexcept { Construct(std::move(other)); }

  // TODO(mglb): enable conditionally
  template <class T, typename = std::enable_if_t<type_list_contains_v<T>>>
  constexpr explicit Variant(T&& obj) noexcept {
    Construct(std::forward<T>(obj));
  }

  template <class T, class... Args>
  constexpr explicit Variant(in_place_type_t<T>, Args&&... args) {
    static_assert(type_list_contains_v<T>);

    ::new (static_cast<void*>(storage_)) T(std::forward<Args>(args)...);
    index_ = type_index_v<T>;
  }

  // TODO(mglb): remove or test
  template <class T, class U, class... Args>
  constexpr explicit Variant(in_place_type_t<T>, std::initializer_list<U> il,
                             Args&&... args) {
    static_assert(type_list_contains_v<T>);

    ::new (static_cast<void*>(storage_)) T(il, std::forward<Args>(args)...);
    index_ = type_index_v<T>;
  }

  template <IndexType I, class... Args>
  constexpr explicit Variant(in_place_index_t<I>, Args&&... args) {
    static_assert(I < sizeof...(TypeN));
    using T = type_at_index_t<I>;

    ::new (static_cast<void*>(storage_)) T(std::forward<Args>(args)...);
    index_ = I;
  }

  // TODO(mglb): remove or test
  template <IndexType I, class U, class... Args>
  constexpr explicit Variant(in_place_index_t<I>, std::initializer_list<U> il,
                             Args&&... args) {
    static_assert(I < sizeof...(TypeN));
    using T = type_at_index_t<I>;

    ::new (static_cast<void*>(storage_)) T(il, std::forward<Args>(args)...);
    index_ = I;
  }

  // TODO(mglb): Do not provide constructor if all types are trivially
  // destructible. This will enable constexpr use of the Variant.
  ~Variant() { DestroyStoredObject(); }

  // Assignments
  // -----------

  // TODO(mglb): enable conditionally
  constexpr Variant& operator=(const Variant& rhs) {
    if (this == &rhs) {
      return *this;
    }
    if (index_ == rhs.index_) {
      Visit(
          [this](const auto& rhs_obj) {
            using Type = verible::remove_cvref_t<decltype(rhs_obj)>;
            auto& this_obj = this->GetWithoutChecks<Type>();
            this_obj = rhs_obj;
          },
          rhs);
    } else {
      DestroyStoredObject();
      Construct(rhs);
    }
    return *this;
  }

  // TODO(mglb): enable conditionally
  constexpr Variant& operator=(Variant&& rhs) noexcept {
    if (index_ == rhs.index_) {
      Visit(
          [this](auto&& rhs_obj) {
            using Type = verible::remove_cvref_t<decltype(rhs_obj)>;
            auto& this_obj = this->GetWithoutChecks<Type>();
            this_obj = std::move(rhs_obj);
          },
          std::move(rhs));
    } else {
      DestroyStoredObject();
      Construct(std::move(rhs));
    }
    return *this;
  }

  // TODO(mglb): enable conditionally
  template <class T,  //
            std::enable_if_t<type_list_contains_v<T>>* = nullptr>
  Variant& operator=(T&& rhs) noexcept {
    static_assert(type_list_contains_v<T>);
    using BaseT = verible::remove_cvref_t<T>;

    if (index_ == type_index_v<BaseT>) {
      auto& this_obj = GetWithoutChecks<BaseT>();
      this_obj = std::forward<T>(rhs);
    } else {
      DestroyStoredObject();
      Construct(std::forward<T>(rhs));
    }
    return *this;
  }

  // TODO(mglb): enable conditionally
  template <class T, class... ArgTypeN>
  T& emplace(ArgTypeN&&... arg_n) {
    static_assert(type_list_contains_v<T>);
    DestroyStoredObject();

    ::new (static_cast<void*>(storage_)) T(std::forward<ArgTypeN>(arg_n)...);
    index_ = type_index_v<T>;
    return GetWithoutChecks<T>();
  }

  // TODO(mglb): remove or implement and test
  /*
  template <class T, class U, class... ArgTypeN>
  T& emplace( std::initializer_list<U> il, ArgTypeN&&... args );
  */

  // TODO(mglb): enable conditionally
  template <IndexType I, class... ArgTypeN>
  auto& emplace(ArgTypeN&&... arg_n) {
    static_assert(I < sizeof...(TypeN));
    using T = type_at_index_t<I>;
    DestroyStoredObject();

    ::new (static_cast<void*>(storage_)) T(std::forward<ArgTypeN>(arg_n)...);
    index_ = I;
    return GetWithoutChecks<I>();
  }

  // TODO(mglb): remove or implement and test
  /*
  template <IndexType I, class U, class... ArgTypeN>
  auto& emplace( std::initializer_list<U> il, ArgTypeN&&... args );
  */

  void swap(Variant& rhs) noexcept {
    if (index_ == rhs.index()) {
      Visit(
          [this](auto& rhs_obj) {
            using Type = std::remove_reference_t<decltype(rhs_obj)>;
            auto& this_obj = GetWithoutChecks<Type>();
            std::swap(this_obj, rhs_obj);
          },
          rhs);
    } else {
      Variant tmp = std::move(rhs);
      rhs = std::move(*this);
      *this = std::move(tmp);
    }
  }

  // Accessors
  // ---------

  // Returns type index of currently stored object.
  // The index is equal to position of a type in Variant's type list (starting
  // from 0).
  IndexType index() const { return index_; }

  // Utils
  // -----

  // Casts a pointer to an object stored in a Variant to a pointer to the
  // Variant itself. Constness of returned Variant matches constness of the
  // object.
  // `T` can be a base class of the class actually stored in the Variant, as
  // long as the class hierarchy does not use multiple inheritance or
  // polymorphism.
  // `stored_object` must not be nullptr.
  // The caller must assure that the object is actually stored in the variant
  // before calling this function.
  template <typename T>
  static match_const_t<Variant, T>* GetFromStoredObject(T* stored_object) {
    static_assert(type_list_contains_type_or_its_subclass_v<T>);
    // Required to get guarantee that the Variant's first member (i.e. storage_)
    // address is the same as Variant address.
    static_assert(std::is_standard_layout_v<Variant>);

    using ByteType = match_const_t<std::byte, T>;
    using VariantType = match_const_t<Variant, T>;

    auto* object_storage = reinterpret_cast<ByteType*>(stored_object);
    auto* variant =
        std::launder(reinterpret_cast<VariantType*>(object_storage));

    if constexpr (type_list_contains_v<T>) {
      CHECK_EQ(variant->index_, type_index_v<std::remove_const_t<T>>);
    }

    return variant;
  }

 private:
  // Internal helpers
  // ----------------

  // Common initialization code called from actual constructors and assignments.

  constexpr void Construct(const Variant& other) {
    Visit(
        [this](const auto& other_obj) {
          using OtherType = verible::remove_cvref_t<decltype(other_obj)>;
          ::new (static_cast<void*>(storage_)) OtherType(other_obj);
        },
        other);
    index_ = other.index_;
  }

  constexpr void Construct(Variant&& other) {
    Visit(
        [this](auto&& other_obj) {
          using OtherType = verible::remove_cvref_t<decltype(other_obj)>;
          ::new (static_cast<void*>(storage_)) OtherType(std::move(other_obj));
        },
        std::move(other));
    index_ = other.index_;
  }

  template <class T>
  constexpr void Construct(T&& obj) noexcept {
    static_assert(type_list_contains_v<T>);
    using BaseT = verible::remove_cvref_t<T>;

    ::new (static_cast<void*>(storage_)) BaseT(std::forward<T>(obj));
    index_ = type_index_v<BaseT>;
  }

  // Returns reference of type `T` to the stored object.
  // Does not check whether `T` is the type of currently stored object.
  // The caller must be sure that this is the case.
  template <class T>
  T& GetWithoutChecks() {
    static_assert(type_list_contains_v<T>);
    return *std::launder(reinterpret_cast<T*>(storage_));
  }

  // `T& GetWithoutChecks()` but const.
  template <class T>
  const T& GetWithoutChecks() const {
    static_assert(type_list_contains_v<T>);
    return *std::launder(reinterpret_cast<const T*>(storage_));
  }

  // `T& GetWithoutChecks()` with Variant's `I`th type as `T`.
  template <IndexType I>
  type_at_index_t<I>& GetWithoutChecks() {
    static_assert(I < sizeof...(TypeN));
    return GetWithoutChecks<type_at_index_t<I>>();
  }

  // `const T& GetWithoutChecks() const` with Variant's `I`th type as `T`.
  template <IndexType I>
  const type_at_index_t<I>& GetWithoutChecks() const {
    static_assert(I < sizeof...(TypeN));
    return GetWithoutChecks<type_at_index_t<I>>();
  }

  // Friend declarations for standalone wrappers of GetWithoutChecks() methods.

  template <class U, class... TN>
  friend constexpr U& variant_internal::GetWithoutChecks(Variant<TN...>&);

  template <class U, class... TN>
  friend constexpr U&& variant_internal::GetWithoutChecks(Variant<TN...>&&);

  template <class U, class... TN>
  friend constexpr const U& variant_internal::GetWithoutChecks(
      const Variant<TN...>&);

  template <class U, class... TN>
  friend constexpr const U&& variant_internal::GetWithoutChecks(
      const Variant<TN...>&&);

  template <uint8_t I, class... TN>
  friend constexpr auto& variant_internal::GetWithoutChecks(Variant<TN...>&);

  template <uint8_t I, class... TN>
  friend constexpr auto&& variant_internal::GetWithoutChecks(Variant<TN...>&&);

  template <uint8_t I, class... TN>
  friend constexpr const auto& variant_internal::GetWithoutChecks(
      const Variant<TN...>&);

  template <uint8_t I, class... TN>
  friend constexpr const auto&& variant_internal::GetWithoutChecks(
      const Variant<TN...>&&);

  //

  void DestroyStoredObject() {
    Visit([](auto& obj) { std::destroy_at(&obj); }, *this);
  }

  // Data members
  // ------------

  // Storage for currently held object.
  // IMPORTANT: this must be first member of the Variant.
  alignas(TypeN...) std::byte storage_[std::max({sizeof(TypeN)...})];
  // Index of type currently stored in the Variant.
  IndexType index_;
};

// holds_alternative()
// -------------------

template <class T, class... TypeN>
constexpr bool holds_alternative(const Variant<TypeN...>& v) noexcept {
  return (v.index() == variant_internal::type_index_v<T, TypeN...>);
}

// get()
// =====

template <class T, class... TypeN>
constexpr T& get(Variant<TypeN...>& v) {
  static_assert((std::is_same_v<T, TypeN> || ...));
  assert(holds_alternative<T>(v));  // CHECK() is not constexpr
  return variant_internal::GetWithoutChecks<T>(v);
}

template <class T, class... TypeN>
constexpr T&& get(Variant<TypeN...>&& v) {
  static_assert((std::is_same_v<T, TypeN> || ...));
  assert(holds_alternative<T>(v));  // CHECK() is not constexpr
  return variant_internal::GetWithoutChecks<T>(std::move(v));
}

template <class T, class... TypeN>
constexpr const T& get(const Variant<TypeN...>& v) {
  static_assert((std::is_same_v<T, TypeN> || ...));
  assert(holds_alternative<T>(v));  // CHECK() is not constexpr
  return variant_internal::GetWithoutChecks<T>(v);
}

template <class T, class... TypeN>
constexpr const T&& get(const Variant<TypeN...>&& v) {
  static_assert((std::is_same_v<T, TypeN> || ...));
  assert(holds_alternative<T>(v));  // CHECK() is not constexpr
  return variant_internal::GetWithoutChecks<T>(std::move(v));
}

template <uint8_t I, class... TypeN>
constexpr auto& get(Variant<TypeN...>& v) {
  static_assert(I < sizeof...(TypeN));
  assert(v.index() == I);  // CHECK() is not constexpr
  return variant_internal::GetWithoutChecks<I>(v);
}

template <uint8_t I, class... TypeN>
constexpr auto&& get(Variant<TypeN...>&& v) {
  static_assert(I < sizeof...(TypeN));
  assert(v.index() == I);  // CHECK() is not constexpr
  return variant_internal::GetWithoutChecks<I>(std::move(v));
}

template <uint8_t I, class... TypeN>
constexpr const auto& get(const Variant<TypeN...>& v) {
  static_assert(I < sizeof...(TypeN));
  assert(v.index() == I);  // CHECK() is not constexpr
  return variant_internal::GetWithoutChecks<I>(v);
}

template <uint8_t I, class... TypeN>
constexpr const auto&& get(const Variant<TypeN...>&& v) {
  static_assert(I < sizeof...(TypeN));
  assert(v.index() == I);  // CHECK() is not constexpr
  return variant_internal::GetWithoutChecks<I>(std::move(v));
}

// get_if()
// ========

template <class T, class... TypeN>
constexpr T* get_if(Variant<TypeN...>* pv) noexcept {
  static_assert((std::is_same_v<T, TypeN> || ...));
  if (!holds_alternative<T>(*pv)) {
    return nullptr;
  }
  return &variant_internal::GetWithoutChecks<T>(*pv);
}

template <class T, class... TypeN>
constexpr const T* get_if(const Variant<TypeN...>* pv) noexcept {
  static_assert((std::is_same_v<T, TypeN> || ...));
  if (!holds_alternative<T>(*pv)) {
    return nullptr;
  }
  return &variant_internal::GetWithoutChecks<T>(*pv);
}

template <uint8_t I, class... TypeN>
constexpr auto* get_if(Variant<TypeN...>* pv) noexcept {
  static_assert(I < sizeof...(TypeN));
  if (pv->index() != I) {
    return nullptr;
  }
  return &variant_internal::GetWithoutChecks<I>(*pv);
}

template <uint8_t I, class... TypeN>
constexpr const auto* get_if(const Variant<TypeN...>* pv) noexcept {
  static_assert(I < sizeof...(TypeN));
  if (pv->index() != I) {
    return nullptr;
  }
  return &variant_internal::GetWithoutChecks<I>(*pv);
}

}  // namespace verible

#endif  // VERIBLE_COMMON_UTIL_VARIANT_H_
