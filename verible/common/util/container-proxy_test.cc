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

#include "verible/common/util/container-proxy.h"

#include <initializer_list>
#include <list>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "verible/common/strings/display-utils.h"
#include "verible/common/util/type-traits.h"

namespace verible {
namespace {

using ::testing::ElementsAre;

enum class ContainerProxyEvent {
  kUnknown,
  kInserted,
  kBeingRemoved,
  kBeingReplaced,
  kWereReplaced,
};

template <class Container>
struct TestTrace {
  using value_type = typename Container::value_type;
  using const_iterator = typename Container::const_iterator;

  ContainerProxyEvent triggered_method = ContainerProxyEvent::kUnknown;
  // Snapshot of the container when the method has been called.
  std::vector<value_type> container_snapshot;

  int first_index = -1;
  int last_index = -1;
  std::vector<value_type> values = {};

  // Constructors for creating reference object in tests

  template <class SnapshotRange, class ValuesRange>
  TestTrace(ContainerProxyEvent method, const SnapshotRange &snapshot,
            int first_index, int last_index, const ValuesRange &values)
      : triggered_method(method),
        container_snapshot(std::begin(snapshot), std::end(snapshot)),
        first_index(first_index),
        last_index(last_index),
        values(std::begin(values), std::end(values)) {}

  template <class SnapshotRange>
  TestTrace(ContainerProxyEvent method, const SnapshotRange &snapshot,
            int first_index, int last_index,
            std::initializer_list<value_type> values)
      : triggered_method(method),
        container_snapshot(std::begin(snapshot), std::end(snapshot)),
        first_index(first_index),
        last_index(last_index),
        values(values) {}

  TestTrace(ContainerProxyEvent method,
            std::initializer_list<value_type> snapshot, int first_index,
            int last_index, std::initializer_list<value_type> values)
      : triggered_method(method),
        container_snapshot(snapshot),
        first_index(first_index),
        last_index(last_index),
        values(values) {}

  TestTrace(ContainerProxyEvent method,
            std::initializer_list<value_type> snapshot)
      : triggered_method(method), container_snapshot(snapshot) {}

  // Constructors for creating trace objects in a container implementation

  template <class Range>
  TestTrace(ContainerProxyEvent method, const Range &container)
      : triggered_method(method),
        container_snapshot(container.begin(), container.end()) {}

  template <class Range>
  TestTrace(ContainerProxyEvent method, const Range &container,
            const_iterator first, const_iterator last)
      : triggered_method(method),
        container_snapshot(container.begin(), container.end()),
        first_index(std::distance(container.begin(), first)),
        last_index(std::distance(container.begin(), last)),
        values(first, last) {}

  // Operators

  bool operator==(const TestTrace &other) const {
    return triggered_method == other.triggered_method &&
           std::equal(container_snapshot.begin(), container_snapshot.end(),
                      other.container_snapshot.begin()) &&
           first_index == other.first_index && last_index == other.last_index &&
           last_index == other.last_index &&
           std::equal(values.begin(), values.end(), other.values.begin());
  }

  friend std::ostream &operator<<(std::ostream &s, TestTrace obj) {
    switch (obj.triggered_method) {
      case ContainerProxyEvent::kUnknown:
        s << "UNKNOWN";
        break;
      case ContainerProxyEvent::kInserted:
        s << "inserted";
        break;
      case ContainerProxyEvent::kBeingRemoved:
        s << "being_removed";
        break;
      case ContainerProxyEvent::kBeingReplaced:
        s << "being_replaced";
        break;
      case ContainerProxyEvent::kWereReplaced:
        s << "were_replaced";
        break;
    }
    s << "(";
    if (obj.first_index >= 0) {
      s << "elements = [" << obj.first_index << ", " << obj.last_index << ")";
      s << " {" << SequenceFormatter(obj.values) << "}; ";
    }
    s << "snapshot = {" << SequenceFormatter(obj.container_snapshot) << "})";
    return s;
  }
};

template <class Container>
class ContainerProxy
    : private ContainerProxyBase<ContainerProxy<Container>, Container> {
  using ThisType = ContainerProxy<Container>;
  using Base = ContainerProxyBase<ContainerProxy<Container>, Container>;
  friend Base;

 public:
  explicit ContainerProxy(Container &container) : container_(container) {}

  using typename Base::container_type;

  using typename Base::value_type;

  using typename Base::const_reference;
  using typename Base::reference;

  using typename Base::const_iterator;
  using typename Base::iterator;

  using typename Base::difference_type;
  using typename Base::size_type;

  using typename Base::const_reverse_iterator;
  using typename Base::reverse_iterator;

  using Base::begin;
  using Base::cbegin;
  using Base::cend;
  using Base::end;

  using Base::crbegin;
  using Base::crend;
  using Base::rbegin;
  using Base::rend;

  using Base::back;
  using Base::front;
  using Base::operator[];
  using Base::at;

  using Base::empty;
  using Base::max_size;
  using Base::size;

  using Base::emplace_back;
  using Base::push_back;

  using Base::emplace_front;
  using Base::push_front;

  using Base::emplace;
  using Base::insert;

  using Base::clear;
  using Base::erase;
  using Base::pop_back;
  using Base::pop_front;

  using Base::assign;
  using Base::operator=;
  using Base::swap;

  using Base::capacity;
  using Base::reserve;
  using Base::resize;

  // Tracing data for test purposes.
  std::vector<TestTrace<ThisType>> trace_data;

 private:
  Container &container_;

  Container &underlying_container() { return container_; }
  const Container &underlying_container() const { return container_; }

  void ElementsInserted(const_iterator first, const_iterator last) {
    trace_data.push_back(TestTrace<ThisType>(ContainerProxyEvent::kInserted,
                                             container_, first, last));
  }

  void ElementsBeingRemoved(const_iterator first, const_iterator last) {
    trace_data.push_back(TestTrace<ThisType>(ContainerProxyEvent::kBeingRemoved,
                                             container_, first, last));
  }

  void ElementsBeingReplaced() {
    trace_data.push_back(
        TestTrace<ThisType>(ContainerProxyEvent::kBeingReplaced, container_));
  }

  void ElementsWereReplaced() {
    trace_data.push_back(
        TestTrace<ThisType>(ContainerProxyEvent::kWereReplaced, container_));
  }
};

template <class Proxy>
class ContainerProxyTest : public ::testing::Test {
 public:
  ContainerProxyTest() = default;

  using Container = typename Proxy::container_type;
  Container container = {"zero", "one", "two"};
  Proxy proxy = Proxy(container);
};

using BidirectionalContainerProxyTestTypes = ::testing::Types<
    // vector
    ContainerProxy<std::vector<std::string>>,
    const ContainerProxy<std::vector<std::string>>,
    // list
    ContainerProxy<std::list<std::string>>,
    const ContainerProxy<std::list<std::string>>
    // TODO: deque
    // TODO: const array
    >;
template <class T>
class BidirectionalContainerProxyTest : public ContainerProxyTest<T> {};
TYPED_TEST_SUITE(BidirectionalContainerProxyTest,
                 BidirectionalContainerProxyTestTypes);

using MutableBidirectionalContainerProxyTestTypes = ::testing::Types<
    // vector
    ContainerProxy<std::vector<std::string>>,
    // list
    ContainerProxy<std::list<std::string>>
    // TODO: deque
    >;
template <class T>
class MutableBidirectionalContainerProxyTest : public ContainerProxyTest<T> {};
TYPED_TEST_SUITE(MutableBidirectionalContainerProxyTest,
                 MutableBidirectionalContainerProxyTestTypes);

using MutablePrependableContainerProxyTestTypes = ::testing::Types<
    // list
    ContainerProxy<std::list<std::string>>
    // TODO: deque
    >;
template <class T>
class MutablePrependableContainerProxyTest : public ContainerProxyTest<T> {};
TYPED_TEST_SUITE(MutablePrependableContainerProxyTest,
                 MutablePrependableContainerProxyTestTypes);

using RandomAccessContainerProxyTestTypes = ::testing::Types<
    // vector
    ContainerProxy<std::vector<std::string>>,
    const ContainerProxy<std::vector<std::string>>>;
template <class T>
class RandomAccessContainerProxyTest : public ContainerProxyTest<T> {};
TYPED_TEST_SUITE(RandomAccessContainerProxyTest,
                 RandomAccessContainerProxyTestTypes);

using MutableRandomAccessContainerProxyTestTypes = ::testing::Types<
    // vector
    ContainerProxy<std::vector<std::string>>>;
template <class T>
class MutableRandomAccessContainerProxyTest : public ContainerProxyTest<T> {};
TYPED_TEST_SUITE(MutableRandomAccessContainerProxyTest,
                 MutableRandomAccessContainerProxyTestTypes);

// Access

TYPED_TEST(BidirectionalContainerProxyTest, Access) {
  EXPECT_EQ(this->proxy.front(), this->container.front());
  EXPECT_EQ(this->proxy.back(), this->container.back());
  EXPECT_EQ(&this->proxy.front(), &this->container.front());
  EXPECT_EQ(&this->proxy.back(), &this->container.back());
  EXPECT_EQ(*this->proxy.begin(), this->container.front());
}

TYPED_TEST(RandomAccessContainerProxyTest, Access) {
  EXPECT_EQ(this->proxy[0], this->container[0]);
  EXPECT_EQ(this->proxy[1], this->container[1]);
  EXPECT_EQ(this->proxy[2], this->container[2]);

  EXPECT_EQ(this->proxy.at(0), this->container[0]);
  EXPECT_EQ(this->proxy.at(1), this->container[1]);
  EXPECT_EQ(this->proxy.at(2), this->container[2]);

  EXPECT_EQ(&this->proxy[0], &this->container[0]);
  EXPECT_EQ(&this->proxy[1], &this->container[1]);
  EXPECT_EQ(&this->proxy[2], &this->container[2]);

  EXPECT_EQ(&this->proxy.at(0), &this->container[0]);
  EXPECT_EQ(&this->proxy.at(1), &this->container[1]);
  EXPECT_EQ(&this->proxy.at(2), &this->container[2]);
}

// Iteration

TYPED_TEST(BidirectionalContainerProxyTest, Iteration) {
  const int initial_size = this->container.size();

  {
    auto iter = this->proxy.begin();
    for (int i = 0; i < initial_size; ++i) {
      EXPECT_EQ(*iter, *std::next(this->container.begin(), i)) << "i = " << i;
      EXPECT_EQ(&*iter, &*std::next(this->container.begin(), i)) << "i = " << i;
      ++iter;
    }
    EXPECT_EQ(iter, this->proxy.end());
  }
  {
    auto iter = this->proxy.cbegin();
    for (int i = 0; i < initial_size; ++i) {
      EXPECT_EQ(*iter, *std::next(this->container.begin(), i)) << "i = " << i;
      EXPECT_EQ(&*iter, &*std::next(this->container.begin(), i)) << "i = " << i;
      ++iter;
    }
    EXPECT_EQ(iter, this->proxy.cend());
  }
  {
    int i = 0;
    for (auto &elem : this->proxy) {
      EXPECT_EQ(elem, *std::next(this->container.begin(), i)) << "i = " << i;
      EXPECT_EQ(&elem, &*std::next(this->container.begin(), i)) << "i = " << i;
      ++i;
    }
  }
}

TYPED_TEST(BidirectionalContainerProxyTest, ReverseIteration) {
  const int initial_size = this->container.size();

  {
    auto iter = this->proxy.rbegin();
    for (int i = initial_size - 1; i >= 0; --i) {
      EXPECT_EQ(*iter, *std::next(this->container.begin(), i)) << "i = " << i;
      EXPECT_EQ(&*iter, &*std::next(this->container.begin(), i)) << "i = " << i;
      ++iter;
    }
    EXPECT_EQ(iter, this->proxy.rend());
  }
  {
    auto iter = this->proxy.crbegin();
    for (int i = initial_size - 1; i >= 0; --i) {
      EXPECT_EQ(*iter, *std::next(this->container.begin(), i)) << "i = " << i;
      EXPECT_EQ(&*iter, &*std::next(this->container.begin(), i)) << "i = " << i;
      ++iter;
    }
    EXPECT_EQ(iter, this->proxy.crend());
  }
}

// Modifiers (inserting)

TYPED_TEST(MutableBidirectionalContainerProxyTest, ModifiersPushEmplaceBack) {
  using Trace = TestTrace<TypeParam>;
  const int initial_size = this->proxy.size();

  this->proxy.trace_data.clear();

  this->proxy.push_back("three");
  EXPECT_EQ(this->proxy.size(), initial_size + 1);
  EXPECT_EQ(this->proxy.back(), "three");
  EXPECT_THAT(
      this->proxy.trace_data,
      ElementsAre(Trace(ContainerProxyEvent::kInserted, this->container,  //
                        initial_size, initial_size + 1, {"three"})));

  this->proxy.trace_data.clear();

  std::string four = std::string("four");
  // Make sure the string buffer is dynamically allocated.
  four.reserve(1000);
  const auto *four_data = four.data();
  this->proxy.push_back(std::move(four));
  EXPECT_EQ(this->proxy.size(), initial_size + 2);
  EXPECT_EQ(this->proxy.back(), "four");
  // Verify that the string has been moved and not copied.
  // TODO(mglb): make sure this is reliable.
  EXPECT_EQ(this->proxy.back().data(), four_data);
  EXPECT_THAT(
      this->proxy.trace_data,
      ElementsAre(Trace(ContainerProxyEvent::kInserted, this->container,  //
                        initial_size + 1, initial_size + 2, {"four"})));

  this->proxy.trace_data.clear();

  this->proxy.emplace_back(5, '5');
  EXPECT_EQ(this->proxy.size(), initial_size + 3);
  EXPECT_EQ(this->proxy.back(), "55555");
  EXPECT_THAT(
      this->proxy.trace_data,
      ElementsAre(Trace(ContainerProxyEvent::kInserted, this->container,  //
                        initial_size + 2, initial_size + 3, {"55555"})));
}

TYPED_TEST(MutablePrependableContainerProxyTest, ModifiersPushEmplaceFront) {
  using Trace = TestTrace<TypeParam>;
  const int initial_size = this->proxy.size();

  this->proxy.trace_data.clear();

  this->proxy.push_front("minus_one");
  EXPECT_EQ(this->proxy.size(), initial_size + 1);
  EXPECT_EQ(this->proxy.front(), "minus_one");
  EXPECT_THAT(
      this->proxy.trace_data,
      ElementsAre(Trace(ContainerProxyEvent::kInserted, this->container,  //
                        0, 1, {"minus_one"})));

  this->proxy.trace_data.clear();

  std::string minus_two = std::string("minus_two");
  // Make sure the string buffer is dynamically allocated.
  minus_two.reserve(1000);
  const auto *minus_two_data = minus_two.data();
  this->proxy.push_front(std::move(minus_two));
  EXPECT_EQ(this->proxy.size(), initial_size + 2);
  EXPECT_EQ(this->proxy.front(), "minus_two");
  // Verify that the string has been moved and not copied.
  // TODO(mglb): make sure this is reliable.
  EXPECT_EQ(this->proxy.front().data(), minus_two_data);
  EXPECT_THAT(
      this->proxy.trace_data,
      ElementsAre(Trace(ContainerProxyEvent::kInserted, this->container,  //
                        0, 1, {"minus_two"})));

  this->proxy.trace_data.clear();

  this->proxy.emplace_front(3, '-');
  EXPECT_EQ(this->proxy.size(), initial_size + 3);
  EXPECT_EQ(this->proxy.front(), "---");
  EXPECT_THAT(
      this->proxy.trace_data,
      ElementsAre(Trace(ContainerProxyEvent::kInserted, this->container,  //
                        0, 1, {"---"})));
}

TYPED_TEST(MutableBidirectionalContainerProxyTest, ModifiersInsertEmplace) {
  using Trace = TestTrace<TypeParam>;

  const int initial_size = this->proxy.size();

  this->proxy.trace_data.clear();

  this->proxy.emplace(std::next(this->proxy.begin()), 3, 'a');
  EXPECT_EQ(this->proxy.size(), initial_size + 1);
  EXPECT_EQ(*std::next(this->proxy.begin()), "aaa");
  EXPECT_THAT(
      this->proxy.trace_data,
      ElementsAre(Trace(ContainerProxyEvent::kInserted, this->container,  //
                        1, 2, {"aaa"})));

  this->proxy.trace_data.clear();

  this->proxy.insert(this->proxy.begin(), "foo");
  EXPECT_EQ(this->proxy.size(), initial_size + 2);
  EXPECT_EQ(this->proxy.front(), "foo");
  EXPECT_THAT(
      this->proxy.trace_data,
      ElementsAre(Trace(ContainerProxyEvent::kInserted, this->container,  //
                        0, 1, {"foo"})));

  this->proxy.trace_data.clear();

  this->proxy.insert(std::next(this->proxy.begin(), 3), {"bbb", "ccc", "ddd"});
  EXPECT_EQ(this->proxy.size(), initial_size + 5);
  EXPECT_EQ(*std::next(this->proxy.begin(), 3), "bbb");
  EXPECT_EQ(*std::next(this->proxy.begin(), 4), "ccc");
  EXPECT_EQ(*std::next(this->proxy.begin(), 5), "ddd");
  EXPECT_THAT(
      this->proxy.trace_data,
      ElementsAre(Trace(ContainerProxyEvent::kInserted, this->container,  //
                        3, 6, {"bbb", "ccc", "ddd"})));

  this->proxy.trace_data.clear();

  static const std::string source[] = {"eee", "fff", "ggg"};
  this->proxy.insert(std::next(this->proxy.begin(), 6), std::begin(source),
                     std::end(source));
  EXPECT_EQ(this->proxy.size(), initial_size + 8);
  EXPECT_EQ(*std::next(this->proxy.begin(), 6), "eee");
  EXPECT_EQ(*std::next(this->proxy.begin(), 7), "fff");
  EXPECT_EQ(*std::next(this->proxy.begin(), 8), "ggg");
  EXPECT_THAT(
      this->proxy.trace_data,
      ElementsAre(Trace(ContainerProxyEvent::kInserted, this->container,  //
                        6, 9, {"eee", "fff", "ggg"})));

  this->proxy.trace_data.clear();

  std::string s = std::string("bar");
  // Make sure the string buffer is dynamically allocated.
  s.reserve(1000);
  const auto *s_data = s.data();
  this->proxy.insert(std::next(this->proxy.begin(), 1), std::move(s));
  EXPECT_EQ(this->proxy.size(), initial_size + 9);
  EXPECT_EQ(*std::next(this->proxy.begin(), 1), "bar");
  // Verify that the string has been moved and not copied.
  // TODO(mglb): make sure this is reliable.
  EXPECT_EQ(std::next(this->proxy.begin(), 1)->data(), s_data);
  EXPECT_THAT(
      this->proxy.trace_data,
      ElementsAre(Trace(ContainerProxyEvent::kInserted, this->container,  //
                        1, 2, {"bar"})));

  this->proxy.trace_data.clear();

  this->proxy.insert(std::next(this->proxy.begin(), 2), 3, "baz");
  EXPECT_EQ(this->proxy.size(), initial_size + 12);
  EXPECT_EQ(*std::next(this->proxy.begin(), 2), "baz");
  EXPECT_EQ(*std::next(this->proxy.begin(), 3), "baz");
  EXPECT_EQ(*std::next(this->proxy.begin(), 4), "baz");
  EXPECT_THAT(
      this->proxy.trace_data,
      ElementsAre(Trace(ContainerProxyEvent::kInserted, this->container,  //
                        2, 5, {"baz", "baz", "baz"})));

  // Check whole container, just to be sure.

  EXPECT_THAT(this->container, ElementsAre("foo", "bar", "baz", "baz", "baz",
                                           "zero", "aaa", "bbb", "ccc", "ddd",
                                           "eee", "fff", "ggg", "one", "two"));
}

// Modifiers (removing)

TYPED_TEST(MutableBidirectionalContainerProxyTest, ModifiersErase) {
  using Trace = TestTrace<TypeParam>;
  using iterator = typename TypeParam::iterator;
  using const_iterator = typename TypeParam::const_iterator;

  // Add a few more elements
  this->container.push_back("three");
  this->container.push_back("four");
  this->container.push_back("five");
  this->container.push_back("six");
  this->container.push_back("seven");
  this->container.push_back("eight");

  {
    this->proxy.trace_data.clear();

    iterator pos = std::next(this->proxy.begin(), 2);  // "two"
    this->proxy.erase(pos);
    EXPECT_THAT(this->container, ElementsAre("zero", "one", "three", "four",
                                             "five", "six", "seven", "eight"));
    EXPECT_THAT(this->proxy.trace_data,
                ElementsAre(Trace(ContainerProxyEvent::kBeingRemoved,
                                  {"zero", "one", "two", "three", "four",
                                   "five", "six", "seven", "eight"},  //
                                  2, 3, {"two"})));
  }
  {
    this->proxy.trace_data.clear();

    const_iterator pos = std::next(this->proxy.begin(), 2);  // "three"
    this->proxy.erase(pos);
    EXPECT_THAT(this->container, ElementsAre("zero", "one", "four", "five",
                                             "six", "seven", "eight"));
    EXPECT_THAT(this->proxy.trace_data,
                ElementsAre(Trace(ContainerProxyEvent::kBeingRemoved,
                                  {"zero", "one", "three", "four", "five",
                                   "six", "seven", "eight"},  //
                                  2, 3, {"three"})));
  }
  {
    this->proxy.trace_data.clear();

    iterator first = std::next(this->proxy.begin(), 2);  // "four"
    iterator last = std::next(this->proxy.begin(), 5);   // "seven"
    this->proxy.erase(first, last);
    EXPECT_THAT(this->container, ElementsAre("zero", "one", "seven", "eight"));
    EXPECT_THAT(this->proxy.trace_data,
                ElementsAre(Trace(ContainerProxyEvent::kBeingRemoved,
                                  {"zero", "one", "four", "five", "six",
                                   "seven", "eight"},  //
                                  2, 5, {"four", "five", "six"})));
  }
  {
    this->proxy.trace_data.clear();

    const_iterator first = std::next(this->proxy.begin(), 1);  // "one"
    const_iterator last = std::next(this->proxy.begin(), 3);   // "eight"
    this->proxy.erase(first, last);
    EXPECT_THAT(this->container, ElementsAre("zero", "eight"));
    EXPECT_THAT(this->proxy.trace_data,
                ElementsAre(Trace(ContainerProxyEvent::kBeingRemoved,
                                  {"zero", "one", "seven", "eight"},  //
                                  1, 3, {"one", "seven"})));
  }
}

TYPED_TEST(MutablePrependableContainerProxyTest, ModifiersPopFront) {
  using Trace = TestTrace<TypeParam>;

  this->proxy.trace_data.clear();

  this->proxy.pop_front();
  EXPECT_THAT(this->container, ElementsAre("one", "two"));
  EXPECT_THAT(this->proxy.trace_data,
              ElementsAre(Trace(ContainerProxyEvent::kBeingRemoved,
                                {"zero", "one", "two"},  //
                                0, 1, {"zero"})));
}

TYPED_TEST(MutableRandomAccessContainerProxyTest, ModifiersPopBack) {
  using Trace = TestTrace<TypeParam>;

  this->proxy.trace_data.clear();

  this->proxy.pop_back();
  EXPECT_THAT(this->container, ElementsAre("zero", "one"));
  EXPECT_THAT(this->proxy.trace_data,
              ElementsAre(Trace(ContainerProxyEvent::kBeingRemoved,
                                {"zero", "one", "two"},  //
                                2, 3, {"two"})));
}

TYPED_TEST(MutableRandomAccessContainerProxyTest, ModifiersClear) {
  using Trace = TestTrace<TypeParam>;

  this->proxy.trace_data.clear();

  this->proxy.clear();
  EXPECT_TRUE(this->container.empty());
  EXPECT_THAT(this->proxy.trace_data,
              ElementsAre(Trace(ContainerProxyEvent::kBeingRemoved,
                                {"zero", "one", "two"},  //
                                0, 3, {"zero", "one", "two"})));
}

// Assignment

TYPED_TEST(MutableBidirectionalContainerProxyTest, Assign) {
  using Trace = TestTrace<TypeParam>;

  static const std::vector<std::string> other_container = {"foo", "bar", "baz"};

  this->proxy.trace_data.clear();

  this->proxy.assign(other_container.begin(), other_container.end());
  EXPECT_THAT(this->container, ElementsAre("foo", "bar", "baz"));
  EXPECT_THAT(
      this->proxy.trace_data,
      ElementsAre(
          Trace(ContainerProxyEvent::kBeingReplaced, {"zero", "one", "two"}),
          Trace(ContainerProxyEvent::kWereReplaced, {"foo", "bar", "baz"})));

  this->proxy.trace_data.clear();

  this->proxy.assign({"x", "y"});
  EXPECT_THAT(this->container, ElementsAre("x", "y"));
  EXPECT_THAT(
      this->proxy.trace_data,
      ElementsAre(
          Trace(ContainerProxyEvent::kBeingReplaced, {"foo", "bar", "baz"}),
          Trace(ContainerProxyEvent::kWereReplaced, {"x", "y"})));

  this->proxy.trace_data.clear();

  this->proxy.assign(4, "FOUR");
  EXPECT_THAT(this->container, ElementsAre("FOUR", "FOUR", "FOUR", "FOUR"));
  EXPECT_THAT(
      this->proxy.trace_data,
      ElementsAre(Trace(ContainerProxyEvent::kBeingReplaced, {"x", "y"}),
                  Trace(ContainerProxyEvent::kWereReplaced,
                        {"FOUR", "FOUR", "FOUR", "FOUR"})));
}

TYPED_TEST(MutableBidirectionalContainerProxyTest, AssignmentOperator) {
  using Trace = TestTrace<TypeParam>;
  using Container = typename TypeParam::container_type;

  Container other_container = {"foo", "bar", "baz"};

  this->proxy.trace_data.clear();

  auto &proxy_ref_0 = this->proxy = other_container;
  static_assert(std::is_same_v<decltype(proxy_ref_0), TypeParam &>);
  EXPECT_THAT(this->container, ElementsAre("foo", "bar", "baz"));
  EXPECT_THAT(
      this->proxy.trace_data,
      ElementsAre(
          Trace(ContainerProxyEvent::kBeingReplaced, {"zero", "one", "two"}),
          Trace(ContainerProxyEvent::kWereReplaced, {"foo", "bar", "baz"})));

  this->proxy.trace_data.clear();

  auto &proxy_ref_1 = this->proxy = {"x", "y"};
  static_assert(std::is_same_v<decltype(proxy_ref_1), TypeParam &>);
  EXPECT_THAT(this->container, ElementsAre("x", "y"));
  EXPECT_THAT(
      this->proxy.trace_data,
      ElementsAre(
          Trace(ContainerProxyEvent::kBeingReplaced, {"foo", "bar", "baz"}),
          Trace(ContainerProxyEvent::kWereReplaced, {"x", "y"})));

  this->proxy.trace_data.clear();

  auto *const foo_ptr = &other_container.front();
  auto &proxy_ref_2 = this->proxy = std::move(other_container);
  static_assert(std::is_same_v<decltype(proxy_ref_2), TypeParam &>);
  EXPECT_THAT(this->container, ElementsAre("foo", "bar", "baz"));
  // Verify that the container has been moved and not copied.
  EXPECT_EQ(&this->container.front(), foo_ptr);
  EXPECT_THAT(
      this->proxy.trace_data,
      ElementsAre(
          Trace(ContainerProxyEvent::kBeingReplaced, {"x", "y"}),
          Trace(ContainerProxyEvent::kWereReplaced, {"foo", "bar", "baz"})));
}

TYPED_TEST(MutableBidirectionalContainerProxyTest, Swap) {
  using Proxy = TypeParam;
  using Container = typename Proxy::container_type;
  using Trace = TestTrace<TypeParam>;

  Container other_container = {"foo", "bar"};
  Proxy other_proxy = Proxy(other_container);

  this->proxy.trace_data.clear();
  other_proxy.trace_data.clear();

  this->proxy.swap(other_proxy);
  EXPECT_THAT(this->container, ElementsAre("foo", "bar"));
  EXPECT_THAT(other_container, ElementsAre("zero", "one", "two"));
  EXPECT_THAT(
      this->proxy.trace_data,
      ElementsAre(
          Trace(ContainerProxyEvent::kBeingReplaced, {"zero", "one", "two"}),
          Trace(ContainerProxyEvent::kWereReplaced, {"foo", "bar"})));
  EXPECT_THAT(
      other_proxy.trace_data,
      ElementsAre(
          Trace(ContainerProxyEvent::kBeingReplaced, {"foo", "bar"}),
          Trace(ContainerProxyEvent::kWereReplaced, {"zero", "one", "two"})));

  this->proxy.trace_data.clear();
  other_proxy.trace_data.clear();

  swap(this->proxy, other_proxy);
  EXPECT_THAT(this->container, ElementsAre("zero", "one", "two"));
  EXPECT_THAT(other_container, ElementsAre("foo", "bar"));
  EXPECT_THAT(
      this->proxy.trace_data,
      ElementsAre(
          Trace(ContainerProxyEvent::kBeingReplaced, {"foo", "bar"}),
          Trace(ContainerProxyEvent::kWereReplaced, {"zero", "one", "two"})));
  EXPECT_THAT(
      other_proxy.trace_data,
      ElementsAre(
          Trace(ContainerProxyEvent::kBeingReplaced, {"zero", "one", "two"}),
          Trace(ContainerProxyEvent::kWereReplaced, {"foo", "bar"})));

  Container container_without_proxy = {"qux", "quux", "quz", "quuz"};

  this->proxy.trace_data.clear();

  this->proxy.swap(container_without_proxy);
  EXPECT_THAT(this->container, ElementsAre("qux", "quux", "quz", "quuz"));
  EXPECT_THAT(container_without_proxy, ElementsAre("zero", "one", "two"));
  EXPECT_THAT(this->proxy.trace_data,
              ElementsAre(Trace(ContainerProxyEvent::kBeingReplaced,
                                {"zero", "one", "two"}),
                          Trace(ContainerProxyEvent::kWereReplaced,
                                {"qux", "quux", "quz", "quuz"})));
}

// Capacity

TYPED_TEST(BidirectionalContainerProxyTest, Capacity) {
  EXPECT_EQ(this->proxy.size(), this->container.size());
  EXPECT_EQ(this->proxy.empty(), this->container.empty());
  EXPECT_EQ(this->proxy.max_size(), this->container.max_size());
}

TYPED_TEST(RandomAccessContainerProxyTest, Capacity) {
  EXPECT_EQ(this->proxy.capacity(), this->container.capacity());
}

TYPED_TEST(MutableBidirectionalContainerProxyTest, Capacity) {
  using Trace = TestTrace<TypeParam>;

  const int initial_size = this->proxy.size();
  const int resized_up_size = initial_size + 3;
  const int resized_down_size = 2;
  const int resized_down_again_size = 1;

  this->proxy.trace_data.clear();

  this->proxy.resize(resized_up_size);
  EXPECT_EQ(this->proxy.size(), resized_up_size);
  EXPECT_THAT(this->container, ElementsAre("zero", "one", "two", "", "", ""));
  EXPECT_THAT(this->proxy.trace_data,
              ElementsAre(Trace(ContainerProxyEvent::kInserted,
                                {"zero", "one", "two", "", "", ""},  //
                                initial_size, resized_up_size,       //
                                {"", "", ""})));

  this->proxy.trace_data.clear();

  this->proxy.resize(resized_down_size);
  EXPECT_EQ(this->proxy.size(), resized_down_size);
  EXPECT_THAT(this->container, ElementsAre("zero", "one"));
  EXPECT_THAT(this->proxy.trace_data,
              ElementsAre(Trace(ContainerProxyEvent::kBeingRemoved,
                                {"zero", "one", "two", "", "", ""},  //
                                resized_down_size, resized_up_size,  //
                                {"two", "", "", ""})));

  this->proxy.trace_data.clear();

  this->proxy.resize(resized_up_size, "1");
  EXPECT_EQ(this->proxy.size(), resized_up_size);
  EXPECT_THAT(this->container, ElementsAre("zero", "one", "1", "1", "1", "1"));
  EXPECT_THAT(this->proxy.trace_data,
              ElementsAre(Trace(ContainerProxyEvent::kInserted,
                                {"zero", "one", "1", "1", "1", "1"},  //
                                resized_down_size, resized_up_size,   //
                                {"1", "1", "1", "1"})));

  this->proxy.trace_data.clear();

  this->proxy.resize(resized_down_again_size, "whatever, will not be used");
  EXPECT_EQ(this->proxy.size(), resized_down_again_size);
  EXPECT_THAT(this->container, ElementsAre("zero"));
  EXPECT_THAT(this->proxy.trace_data,
              ElementsAre(Trace(ContainerProxyEvent::kBeingRemoved,
                                {"zero", "one", "1", "1", "1", "1"},       //
                                resized_down_again_size, resized_up_size,  //
                                {"one", "1", "1", "1", "1"})));
}

TYPED_TEST(MutableRandomAccessContainerProxyTest, Capacity) {
  const int initial_capacity = this->proxy.capacity();
  EXPECT_EQ(this->proxy.capacity(), this->container.capacity());

  const int new_capacity = initial_capacity + 42;
  this->proxy.reserve(new_capacity);
  EXPECT_EQ(this->proxy.capacity(), new_capacity);
  EXPECT_EQ(this->container.capacity(), new_capacity);

  const int lower_capacity = 1;
  this->proxy.reserve(lower_capacity);
  EXPECT_EQ(this->proxy.capacity(), new_capacity);
  EXPECT_EQ(this->container.capacity(), new_capacity);
}

}  // namespace
}  // namespace verible
