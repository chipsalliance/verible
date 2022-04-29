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

#include "common/util/variant.h"

#include <deque>
#include <string>
#include <utility>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace verible {
namespace {

using testing::ElementsAre;

//------------------------------------------------------------------------------
// Test types

using EventLog = std::deque<std::string>;

struct Foo {
  Foo(EventLog* log = nullptr, absl::string_view name = "",
      absl::string_view type = "Foo")
      : event_log(log), foo(absl::StrCat("(", type, ")", name, ".foo")) {
    if (event_log) event_log->push_back("Foo constructor");
  }

  Foo(const Foo& other) : event_log(other.event_log), foo(other.foo) {
    if (event_log) event_log->push_back("Foo copy constructor");
  }

  Foo(Foo&& other) : event_log(other.event_log), foo(std::move(other.foo)) {
    if (event_log) event_log->push_back("Foo move constructor");
  }

  ~Foo() {
    if (event_log) event_log->push_back("Foo destructor");
  }

  EventLog* event_log;
  std::string foo;
};

struct FooBar : public Foo {
  FooBar(EventLog* log = nullptr, absl::string_view name = "",
         absl::string_view type = "FooBar")
      : Foo(log, name, type), bar(absl::StrCat("(", type, ")", name, ".bar")) {
    if (event_log) event_log->push_back("FooBar constructor");
  }

  FooBar(const FooBar& other) : Foo(other), bar(other.bar) {
    if (event_log) event_log->push_back("FooBar copy constructor");
  }

  FooBar(FooBar&& other) : Foo(std::move(other)), bar(std::move(other.bar)) {
    if (event_log) event_log->push_back("FooBar move constructor");
  }

  ~FooBar() {
    if (event_log) event_log->push_back("FooBar destructor");
  }

  std::string bar;
};

struct FooBaz : public Foo {
  FooBaz(EventLog* log = nullptr, absl::string_view name = "",
         absl::string_view type = "FooBaz")
      : Foo(log, name, type),
        baz(absl::StrCat("(", type, ")", name, ".baz")),
        baz2(absl::StrCat("(", type, ")", name, ".baz2")) {
    if (event_log) event_log->push_back("FooBaz constructor");
  }

  FooBaz(const FooBaz& other) : Foo(other), baz(other.baz), baz2(other.baz2) {
    if (event_log) event_log->push_back("FooBaz copy constructor");
  }

  FooBaz(FooBaz&& other)
      : Foo(std::move(other)),
        baz(std::move(other.baz)),
        baz2(std::move(other.baz2)) {
    if (event_log) event_log->push_back("FooBaz move constructor");
  }

  ~FooBaz() {
    if (event_log) event_log->push_back("FooBaz destructor");
  }

  std::string baz;
  std::string baz2;
};

using FooVariant = Variant<FooBar, FooBaz>;
using ValueVariant = Variant<int, float, char, bool>;

//------------------------------------------------------------------------------

TEST(VariantTest, Construction) {
  {
    // Default construction
    FooVariant var;
  }
  {
    EventLog log;
    { FooVariant var = FooVariant(in_place_type<FooBar>, &log); }
    EXPECT_THAT(log, ElementsAre("Foo constructor", "FooBar constructor",
                                 "FooBar destructor", "Foo destructor"));
  }
  {
    EventLog log;
    { FooVariant var = FooVariant(in_place_index<0>, &log); }
    EXPECT_THAT(log, ElementsAre("Foo constructor", "FooBar constructor",
                                 "FooBar destructor", "Foo destructor"));
  }
  {
    EventLog log;
    { FooVariant var = FooVariant(in_place_type<FooBaz>, &log); }
    EXPECT_THAT(log, ElementsAre("Foo constructor", "FooBaz constructor",
                                 "FooBaz destructor", "Foo destructor"));
  }
  {
    EventLog log;
    { FooVariant var = FooVariant(in_place_index<1>, &log); }
    EXPECT_THAT(log, ElementsAre("Foo constructor", "FooBaz constructor",
                                 "FooBaz destructor", "Foo destructor"));
  }
}

TEST(VariantTest, NoDefaultConstructor) {
  struct NoDefault {
    NoDefault() = delete;
    NoDefault(int v) : value(v) {}
    int value;
  };

  // Success if compiles
  Variant<NoDefault> var(in_place_index<0>, 11);
}

// TODO: Variant / assign

// TODO: Variant / copy

// TODO: Variant / value-move

// TODO: Variant / move

//----------

// TODO: Variant / misc / strict aliasing

//----------

// TODO: Variant / cast value ptr → variant ptr (and back)

//----------

TEST(VariantTest, Index) {
  {
    ValueVariant var(in_place_type<int>);
    EXPECT_EQ(var.index(), 0);
  }
  {
    ValueVariant var(in_place_type<float>);
    EXPECT_EQ(var.index(), 1);
  }
  {
    ValueVariant var(in_place_type<char>);
    EXPECT_EQ(var.index(), 2);
  }
  {
    ValueVariant var(in_place_type<bool>);
    EXPECT_EQ(var.index(), 3);
  }
  {
    FooVariant var(in_place_type<FooBar>);
    EXPECT_EQ(var.index(), 0);
  }
  {
    FooVariant var(in_place_type<FooBaz>);
    EXPECT_EQ(var.index(), 1);
  }
}

// TODO: Variant / emplace

// TODO: Variant / swap

//----------

TEST(VisitTest, ReturnNothing) {
  static int status = -1;
  static const auto bar_or_baz = Overload{
      [](FooBar& v) { status = 0; },
      [](FooBaz& v) { status = 1; },
  };
  {
    FooVariant var(in_place_type<FooBar>);
    status = -1;
    static_assert(std::is_void_v<decltype(Visit(bar_or_baz, var))>);
    Visit(bar_or_baz, var);
    EXPECT_EQ(status, 0);
  }
  {
    FooVariant var(in_place_type<FooBaz>);
    status = -1;
    static_assert(std::is_void_v<decltype(Visit(bar_or_baz, var))>);
    Visit(bar_or_baz, var);
    EXPECT_EQ(status, 1);
  }
}

TEST(VisitTest, ReturnValue) {
  static const auto bar_or_baz = Overload{
      [](FooBar& v) { return absl::string_view(v.bar); },
      [](FooBaz& v) { return absl::string_view(v.baz); },
  };
  {
    FooVariant var(in_place_type<FooBar>);
    auto result = Visit(bar_or_baz, var);
    static_assert(std::is_same_v<absl::string_view, decltype(result)>);
    EXPECT_EQ(result, "(FooBar).bar");
  }
  {
    FooVariant var(in_place_type<FooBaz>);
    auto result = Visit(bar_or_baz, var);
    static_assert(std::is_same_v<absl::string_view, decltype(result)>);
    EXPECT_EQ(result, "(FooBaz).baz");
  }
}

TEST(VisitTest, ReturnReference) {
  ValueVariant var;

  const auto get_lvref = [](auto&) -> int& {
    static int v = 0;
    return v;
  };
  static_assert(std::is_same_v<int&, decltype(Visit(get_lvref, var))>);

  const auto get_rvref = [](auto&) -> int&& {
    static int v = 0;
    return std::move(v);
  };
  static_assert(std::is_same_v<int&&, decltype(Visit(get_rvref, var))>);

  const auto get_const_lvref = [](auto&) -> const int& {
    static const int v = 0;
    return v;
  };
  static_assert(
      std::is_same_v<const int&, decltype(Visit(get_const_lvref, var))>);

  const auto get_const_rvref = [](auto&) -> const int&& {
    static const int v = 0;
    return std::move(v);
  };
  static_assert(
      std::is_same_v<const int&&, decltype(Visit(get_const_rvref, var))>);
}

TEST(VisitTest, ConstAndReferenceForwarding) {
  static const auto detect_cref = Overload{
      [](auto& v) { return 0; },
      [](auto&& v) { return 1; },
      [](const auto& v) { return 2; },
      [](const auto&& v) { return 3; },
  };

  FooVariant var;
  EXPECT_EQ(Visit(detect_cref, var), 0);
  EXPECT_EQ(Visit(detect_cref, std::move(var)), 1);
  EXPECT_EQ(Visit(detect_cref, std::as_const(var)), 2);
  EXPECT_EQ(Visit(detect_cref, std::move(std::as_const(var))), 3);
}

TEST(VisitTest, StatelessVisitorLambda) {
  FooVariant var;
  auto result = Visit([](Foo&) { return 42; }, var);
  EXPECT_EQ(result, 42);
}

TEST(VisitTest, StatefulVisitorLambda) {
  FooVariant var;
  int number = 0;
  auto result = Visit(
      [&number](Foo&) {
        number = 0x42;
        return 42;
      },
      var);
  EXPECT_EQ(result, 42);
  EXPECT_EQ(number, 0x42);
}

TEST(VisitTest, StatelessVisitorObject) {
  struct StatelessVisitor {
    int operator()(const FooBar&) { return 100; }
    int operator()(const FooBaz&) { return 200; }
  };

  FooVariant var;
  auto result = Visit(StatelessVisitor{}, var);
  EXPECT_EQ(result, 100);
}

TEST(VisitTest, StatefulVisitorObject) {
  struct StatefulVisitor {
    void operator()(const FooBar&) { id = 100; }
    void operator()(const FooBaz&) { id = 200; }

    int id = -1;
  };

  FooVariant var;
  StatefulVisitor vis;
  Visit(vis, var);
  EXPECT_EQ(vis.id, 100);
}

TEST(HelpersTest, type_index_v) {
  using namespace variant_internal;

  static_assert(type_index_v<int, int> == 0);

  static_assert(type_index_v<int, int, float, char, bool> == 0);
  static_assert(type_index_v<int, float, int, char, bool> == 1);
  static_assert(type_index_v<int, float, char, int, bool> == 2);
  static_assert(type_index_v<int, float, char, bool, int> == 3);
}

//----------

// TODO: Variant / holds_alternative

// TODO: Variant / get

// TODO: Variant / get_if

//----------

}  // namespace
}  // namespace verible
