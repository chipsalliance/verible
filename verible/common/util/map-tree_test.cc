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

#include "verible/common/util/map-tree.h"

#include <cstddef>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

#include "gtest/gtest.h"
#include "verible/common/util/spacer.h"

namespace verible {
namespace {

using MapTreeTestType = MapTree<int, std::string>;
using KV = MapTreeTestType::key_value_type;

TEST(MapTreeTest, EmptyConstruction) {
  const MapTreeTestType m;
  EXPECT_TRUE(m.is_leaf());
  EXPECT_TRUE(m.Children().empty());
  EXPECT_EQ(m.Parent(), nullptr);
  EXPECT_EQ(m.begin(), m.end());
  EXPECT_EQ(m.NumAncestors(), 0);
  EXPECT_FALSE(m.HasAncestor(nullptr));
  EXPECT_FALSE(m.HasAncestor(&m));
  EXPECT_EQ(m.Root(), &m);
  EXPECT_EQ(m.KeyValuePair(), nullptr);
  EXPECT_EQ(m.Key(), nullptr);
  EXPECT_EQ(m.Find(0), m.end());   // there are no keys
  EXPECT_TRUE(m.Value().empty());  // default constructed std::string
}

TEST(MapTreeTest, InitializedValueConstruction) {
  const MapTreeTestType m("boofar");  // given initial value
  EXPECT_TRUE(m.is_leaf());
  EXPECT_TRUE(m.Children().empty());
  EXPECT_EQ(m.Parent(), nullptr);
  EXPECT_EQ(m.begin(), m.end());
  EXPECT_EQ(m.NumAncestors(), 0);
  EXPECT_FALSE(m.HasAncestor(nullptr));
  EXPECT_FALSE(m.HasAncestor(&m));
  EXPECT_EQ(m.Root(), &m);
  EXPECT_EQ(m.KeyValuePair(), nullptr);
  EXPECT_EQ(m.Key(), nullptr);
  EXPECT_EQ(m.Find(0), m.end());  // there are no keys
  EXPECT_EQ(m.Value(), "boofar");
}

TEST(MapTreeTest, InitializeOneChild) {
  const MapTreeTestType m("foo",  //
                          KV{3, MapTreeTestType("bar")});
  EXPECT_FALSE(m.is_leaf());
  EXPECT_EQ(m.Children().size(), 1);
  EXPECT_EQ(m.Parent(), nullptr);
  EXPECT_NE(m.begin(), m.end());
  EXPECT_EQ(m.NumAncestors(), 0);
  EXPECT_EQ(m.Root(), &m);
  EXPECT_EQ(m.KeyValuePair(), nullptr);
  EXPECT_EQ(m.Key(), nullptr);
  EXPECT_EQ(m.Find(0), m.end());
  EXPECT_EQ(m.Value(), "foo");

  const auto found = m.Find(3);
  ASSERT_NE(found, m.end());
  EXPECT_EQ(found->first, 3);
  EXPECT_EQ(found->second.Value(), "bar");
  EXPECT_EQ(found->second.KeyValuePair(), &*found);
  EXPECT_EQ(found->second.Key(), &found->first);
  EXPECT_EQ(*found->second.Key(), 3);
  EXPECT_EQ(found->second.Parent(), &m);
  EXPECT_EQ(found->second.Root(), &m);
  EXPECT_TRUE(found->second.is_leaf());
  EXPECT_EQ(found->second.NumAncestors(), 1);
  EXPECT_FALSE(m.HasAncestor(&found->second));
  EXPECT_TRUE(found->second.HasAncestor(&m));
}

TEST(MapTreeTest, EmplaceOneChild) {
  // Same structure as InitializeOneChild, but emplacing after construction.
  MapTreeTestType m("foo");
  const auto p = m.TryEmplace(3, "bar");
  ASSERT_TRUE(p.second);
  EXPECT_EQ(p.first, m.begin());
  EXPECT_EQ(p.first->second.Value(), "bar");

  EXPECT_FALSE(m.is_leaf());
  EXPECT_EQ(m.Children().size(), 1);
  EXPECT_EQ(m.Parent(), nullptr);
  EXPECT_NE(m.begin(), m.end());
  EXPECT_EQ(m.NumAncestors(), 0);
  EXPECT_EQ(m.Root(), &m);
  EXPECT_EQ(m.KeyValuePair(), nullptr);
  EXPECT_EQ(m.Key(), nullptr);
  EXPECT_EQ(m.Find(0), m.end());
  EXPECT_EQ(m.Value(), "foo");

  const auto found = m.Find(3);
  ASSERT_NE(found, m.end());
  EXPECT_EQ(found->first, 3);
  EXPECT_EQ(found->second.Value(), "bar");
  EXPECT_EQ(found->second.KeyValuePair(), &*found);
  EXPECT_EQ(found->second.Key(), &found->first);
  EXPECT_EQ(*found->second.Key(), 3);
  EXPECT_EQ(found->second.Parent(), &m);
  EXPECT_EQ(found->second.Root(), &m);
  EXPECT_TRUE(found->second.is_leaf());
  EXPECT_EQ(found->second.NumAncestors(), 1);
  EXPECT_FALSE(m.HasAncestor(&found->second));
  EXPECT_TRUE(found->second.HasAncestor(&m));
}

struct NonCopyable {
  std::string_view text;

  explicit NonCopyable(std::string_view text) : text(text) {}

  // move-only, no copy
  NonCopyable(const NonCopyable &) = delete;
  NonCopyable(NonCopyable &&) = default;
  NonCopyable &operator=(const NonCopyable &) = delete;
  NonCopyable &operator=(NonCopyable &&) = default;
};

TEST(MapTreeTest, EmplaceOneNonCopyable) {
  // Same structure as EmplaceOneChild, but on a non-copyable type.
  using NoCopyMapTreeTestType = MapTree<int, NonCopyable>;
  NoCopyMapTreeTestType m(NonCopyable("foo"));
  const auto p = m.TryEmplace(3, NonCopyable("bar"));
  ASSERT_TRUE(p.second);
  EXPECT_EQ(p.first, m.begin());
  EXPECT_EQ(p.first->second.Value().text, "bar");

  EXPECT_FALSE(m.is_leaf());
  EXPECT_EQ(m.Children().size(), 1);
  EXPECT_EQ(m.Parent(), nullptr);
  EXPECT_NE(m.begin(), m.end());
  EXPECT_EQ(m.NumAncestors(), 0);
  EXPECT_EQ(m.Root(), &m);
  EXPECT_EQ(m.KeyValuePair(), nullptr);
  EXPECT_EQ(m.Key(), nullptr);
  EXPECT_EQ(m.Find(0), m.end());
  EXPECT_EQ(m.Value().text, "foo");

  const auto found = m.Find(3);
  ASSERT_NE(found, m.end());
  EXPECT_EQ(found->first, 3);
  EXPECT_EQ(found->second.Value().text, "bar");
  EXPECT_EQ(found->second.KeyValuePair(), &*found);
  EXPECT_EQ(found->second.Key(), &found->first);
  EXPECT_EQ(*found->second.Key(), 3);
  EXPECT_EQ(found->second.Parent(), &m);
  EXPECT_EQ(found->second.Root(), &m);
  EXPECT_TRUE(found->second.is_leaf());
  EXPECT_EQ(found->second.NumAncestors(), 1);
  EXPECT_FALSE(m.HasAncestor(&found->second));
  EXPECT_TRUE(found->second.HasAncestor(&m));
}

TEST(MapTreeTest, EmplaceDuplicateKeyFails) {
  MapTreeTestType m("foo", KV{2, MapTreeTestType("bar")});
  const auto p = m.TryEmplace(2, "zzr");
  EXPECT_FALSE(p.second);
  EXPECT_EQ(p.first, m.begin());
  EXPECT_EQ(p.first->second.Value(), "bar");  // first entry retained
  EXPECT_EQ(m.Children().size(), 1);
}

TEST(MapTreeTest, EmplaceSecondKey) {
  MapTreeTestType m("foo", KV{9, MapTreeTestType("bar")});
  const auto first_iter = m.begin();
  EXPECT_EQ(first_iter->first, 9);
  const auto p = m.TryEmplace(7, "zzr");
  EXPECT_TRUE(p.second);  // successful insertion
  EXPECT_EQ(m.Children().size(), 2);
  EXPECT_EQ(p.first->first, 7);
  EXPECT_EQ(p.first->second.Value(), "zzr");
  EXPECT_EQ(m.Find(9), first_iter);  // iterator stability on insert
}

TEST(MapTreeTest, InitializeMultipleChildrenWithDuplicateKey) {
  const MapTreeTestType m("foo",  //
                          KV{4, MapTreeTestType("bbb")},
                          KV{4, MapTreeTestType("cccc")}  // will be dropped
  );
  EXPECT_FALSE(m.is_leaf());
  EXPECT_EQ(m.Children().size(), 1);  // not 2
  EXPECT_EQ(m.Parent(), nullptr);
  EXPECT_NE(m.begin(), m.end());
  EXPECT_EQ(m.NumAncestors(), 0);
  EXPECT_EQ(m.Root(), &m);
  EXPECT_EQ(m.KeyValuePair(), nullptr);
  EXPECT_EQ(m.Key(), nullptr);
  EXPECT_EQ(m.Find(0), m.end());
  EXPECT_EQ(m.Value(), "foo");
  const auto found = m.Find(4);
  ASSERT_NE(found, m.end());
  EXPECT_EQ(found->second.Value(), "bbb");
}

TEST(MapTreeTest, InitializeMultipleChildrenWithDistinctKeys) {
  const MapTreeTestType m("foo",  //
                          KV{4, MapTreeTestType("bbb")},
                          KV{1, MapTreeTestType("dd")});
  EXPECT_FALSE(m.is_leaf());
  EXPECT_EQ(m.Children().size(), 2);
  EXPECT_EQ(m.Parent(), nullptr);
  EXPECT_NE(m.begin(), m.end());
  EXPECT_EQ(m.NumAncestors(), 0);
  EXPECT_EQ(m.Root(), &m);
  EXPECT_EQ(m.KeyValuePair(), nullptr);
  EXPECT_EQ(m.Key(), nullptr);
  EXPECT_EQ(m.Find(0), m.end());
  EXPECT_EQ(m.Value(), "foo");

  {
    const auto found = m.Find(4);
    ASSERT_NE(found, m.end());
    EXPECT_EQ(found->second.Value(), "bbb");
  }
  {
    const auto found = m.Find(1);
    ASSERT_NE(found, m.end());
    EXPECT_EQ(found->second.Value(), "dd");
  }
  EXPECT_EQ(m.Find(2), m.end());

  for (const auto &p : m) {
    EXPECT_EQ(p.second.KeyValuePair(), &p);
    EXPECT_EQ(p.second.Key(), &p.first);
    EXPECT_EQ(p.second.Parent(), &m);
    EXPECT_EQ(p.second.Root(), &m);
    EXPECT_EQ(p.second.NumAncestors(), 1);
    EXPECT_TRUE(p.second.HasAncestor(&m));
    EXPECT_TRUE(p.second.is_leaf());
  }

  {
    const auto found1 = m.Find(1);
    const auto found2 = m.Find(4);
    EXPECT_FALSE(found1->second.HasAncestor(&found2->second));
    EXPECT_FALSE(found2->second.HasAncestor(&found1->second));
  }
}

TEST(MapTreeTest, InitializeTwoGenerationsDeepCopy) {
  const MapTreeTestType m("foo",  //
                          KV{4,   // child
                             MapTreeTestType("bbb",
                                             KV{1,  // grandchild
                                                MapTreeTestType("dd")})});
  const auto check = [](const MapTreeTestType &root) {
    const auto child = root.Find(4);
    const auto grandchild = child->second.Find(1);
    EXPECT_EQ(child->first, 4);
    EXPECT_EQ(child->second.Value(), "bbb");
    EXPECT_EQ(child->second.Parent(), &root);
    EXPECT_FALSE(child->second.is_leaf());
    EXPECT_EQ(child->second.Children().size(), 1);
    EXPECT_EQ(child->second.NumAncestors(), 1);
    EXPECT_TRUE(child->second.HasAncestor(&root));
    EXPECT_EQ(grandchild->first, 1);
    EXPECT_EQ(grandchild->second.Value(), "dd");
    EXPECT_EQ(grandchild->second.Parent(), &child->second);
    EXPECT_TRUE(grandchild->second.is_leaf());
    EXPECT_EQ(grandchild->second.NumAncestors(), 2);
    EXPECT_EQ(grandchild->second.Root(), &root);
    EXPECT_TRUE(grandchild->second.HasAncestor(&child->second));
    EXPECT_TRUE(grandchild->second.HasAncestor(&root));
  };

  check(m);

  {
    // specificially testing deep copy.
    // clang-format off
    const MapTreeTestType mcopy(m);  // NOLINT(performance-unnecessary-copy-initialization)
    // clang-format on
    check(mcopy);
    check(m);
  }
}

TEST(MapTreeTest, InitializeTwoGenerationsMove) {
  MapTreeTestType m("foo",  //
                    KV{4,   // child
                       MapTreeTestType("bbb",
                                       KV{1,  // grandchild
                                          MapTreeTestType("dd")})});

  MapTreeTestType m_moved(std::move(m));
  {
    const auto child = m_moved.Find(4);
    const auto grandchild = child->second.Find(1);
    EXPECT_EQ(child->first, 4);
    EXPECT_EQ(child->second.Value(), "bbb");
    EXPECT_EQ(child->second.Parent(), &m_moved);
    EXPECT_FALSE(child->second.is_leaf());
    EXPECT_EQ(child->second.Children().size(), 1);
    EXPECT_EQ(child->second.NumAncestors(), 1);
    EXPECT_TRUE(child->second.HasAncestor(&m_moved));
    EXPECT_EQ(grandchild->first, 1);
    EXPECT_EQ(grandchild->second.Value(), "dd");
    EXPECT_EQ(grandchild->second.Parent(), &child->second);
    EXPECT_TRUE(grandchild->second.is_leaf());
    EXPECT_EQ(grandchild->second.NumAncestors(), 2);
    EXPECT_EQ(grandchild->second.Root(), &m_moved);
    EXPECT_TRUE(grandchild->second.HasAncestor(&child->second));
    EXPECT_TRUE(grandchild->second.HasAncestor(&m_moved));
  }
}

TEST(MapTreeTest, Swap) {
  MapTreeTestType m1("foo",  //
                     KV{4,   // child
                        MapTreeTestType("bbb",
                                        KV{1,  // grandchild
                                           MapTreeTestType("dd")})});

  MapTreeTestType m2("foo",  //
                     KV{2,   // child
                        MapTreeTestType("aaaa")});

  const auto check1 = [](const MapTreeTestType &root) {
    const auto child = root.Find(4);
    const auto grandchild = child->second.Find(1);
    EXPECT_EQ(child->first, 4);
    EXPECT_EQ(child->second.Value(), "bbb");
    EXPECT_EQ(child->second.Parent(), &root);
    EXPECT_FALSE(child->second.is_leaf());
    EXPECT_EQ(child->second.Children().size(), 1);
    EXPECT_EQ(child->second.NumAncestors(), 1);
    EXPECT_EQ(grandchild->first, 1);
    EXPECT_EQ(grandchild->second.Value(), "dd");
    EXPECT_EQ(grandchild->second.Parent(), &child->second);
    EXPECT_TRUE(grandchild->second.is_leaf());
    EXPECT_EQ(grandchild->second.NumAncestors(), 2);
    EXPECT_EQ(grandchild->second.Root(), &root);
  };

  const auto check2 = [](const MapTreeTestType &root) {
    const auto child = root.Find(2);
    EXPECT_EQ(child->first, 2);
    EXPECT_EQ(child->second.Value(), "aaaa");
    EXPECT_EQ(child->second.Parent(), &root);
    EXPECT_TRUE(child->second.is_leaf());
    EXPECT_TRUE(child->second.Children().empty());
  };

  check1(m1);
  check2(m2);

  m1.swap(m2);
  check1(m2);
  check2(m1);
}

TEST(MapTreeTest, TraversePrint) {
  const MapTreeTestType m(
      "groot",  //
      KV{5, MapTreeTestType("pp", KV{4, MapTreeTestType("ss")},
                            KV{1, MapTreeTestType("tt")})},
      KV{3, MapTreeTestType("qq", KV{2, MapTreeTestType("ww")},
                            KV{6, MapTreeTestType("vv")})});
  // printing has the benefit of verifying traversal order

  // pre-order traversals
  {  // print node values (string_views)
    std::ostringstream stream;
    m.ApplyPreOrder([&stream](const MapTreeTestType::node_value_type &v) {
      stream << v << " ";
    });
    EXPECT_EQ(stream.str(), "groot qq ww vv pp tt ss ");
  }
  {  // print keys (ints)
    std::ostringstream stream;
    m.ApplyPreOrder([&stream](const MapTreeTestType &node) {
      const auto *key = node.Key();
      stream << (key == nullptr ? 0 : *key) << " ";
    });
    EXPECT_EQ(stream.str(), "0 3 2 6 5 1 4 ");
  }

  // post-order traversals
  {  // print node values (string_views)
    std::ostringstream stream;
    m.ApplyPostOrder([&stream](const MapTreeTestType::node_value_type &v) {
      stream << v << " ";
    });
    EXPECT_EQ(stream.str(), "ww vv qq tt ss pp groot ");
  }
  {  // print keys (ints)
    std::ostringstream stream;
    m.ApplyPostOrder([&stream](const MapTreeTestType &node) {
      const auto *key = node.Key();
      stream << (key == nullptr ? 0 : *key) << " ";
    });
    EXPECT_EQ(stream.str(), "2 6 3 1 4 5 0 ");
  }
}

TEST(MapTreeTest, TraverseMutate) {
  const MapTreeTestType m(
      "groot",  //
      KV{5, MapTreeTestType("pp", KV{4, MapTreeTestType("ss")},
                            KV{1, MapTreeTestType("tt")})},
      KV{3, MapTreeTestType("qq", KV{2, MapTreeTestType("ww")},
                            KV{6, MapTreeTestType("vv")})});
  // pre-order traversals
  {                             // mutate node values (string_views)
    MapTreeTestType m_copy(m);  // deep copy, mutate this copy
    std::ostringstream stream;
    m_copy.ApplyPreOrder([&stream](MapTreeTestType::node_value_type &v) {
      v = v.substr(1);     // mutate: truncate
      stream << v << " ";  // print to verify order
    });
    EXPECT_EQ(stream.str(), "root q w v p t s ");
  }
  {                             // mutate nodes
    MapTreeTestType m_copy(m);  // deep copy, mutate this copy
    std::ostringstream stream;
    m_copy.ApplyPreOrder([&stream](MapTreeTestType &v) {
      v.Value() = v.Value().substr(1);  // mutate: truncate
      stream << v.Value() << " ";       // print to verify order
    });
    EXPECT_EQ(stream.str(), "root q w v p t s ");
  }

  // post-order traversals
  {                             // mutate node values (string_views)
    MapTreeTestType m_copy(m);  // deep copy, mutate this copy
    std::ostringstream stream;
    m_copy.ApplyPostOrder([&stream](MapTreeTestType::node_value_type &v) {
      v = v.substr(1);     // mutate: truncate
      stream << v << " ";  // print to verify order
    });
    EXPECT_EQ(stream.str(), "w v q t s p root ");
  }
  {                             // mutate nodes
    MapTreeTestType m_copy(m);  // deep copy, mutate this copy
    std::ostringstream stream;
    m_copy.ApplyPostOrder([&stream](MapTreeTestType &v) {
      v.Value() = v.Value().substr(1);  // mutate: truncate
      stream << v.Value() << " ";       // print to verify order
    });
    EXPECT_EQ(stream.str(), "w v q t s p root ");
  }
}

TEST(MapTreeTest, PrintTreeRootOnly) {
  const MapTreeTestType m("groot");
  std::ostringstream stream;
  m.PrintTree(stream);
  EXPECT_EQ(stream.str(), "{ (groot) }");
}

TEST(MapTreeTest, PrintTreeOneChild) {
  const MapTreeTestType m("groot", KV{5, MapTreeTestType("gleaf")});
  std::ostringstream stream;
  m.PrintTree(stream);
  EXPECT_EQ(stream.str(),  //
            "{ (groot)\n"
            "  5: { (gleaf) }\n"
            "}");
}

TEST(MapTreeTest, PrintTreeTwoGenerations) {
  const MapTreeTestType m(
      "groot",  //
      KV{5, MapTreeTestType("pp", KV{4, MapTreeTestType("ss")},
                            KV{1, MapTreeTestType("tt")})},
      KV{3, MapTreeTestType("qq", KV{2, MapTreeTestType("ww")},
                            KV{6, MapTreeTestType("vv")})});
  std::ostringstream stream;
  m.PrintTree(stream);
  EXPECT_EQ(stream.str(),  //
            "{ (groot)\n"
            "  3: { (qq)\n"
            "    2: { (ww) }\n"
            "    6: { (vv) }\n"
            "  }\n"
            "  5: { (pp)\n"
            "    1: { (tt) }\n"
            "    4: { (ss) }\n"
            "  }\n"
            "}");
}

TEST(MapTreeTest, PrintTreeTwoGenerationsUsingIndent) {
  const MapTreeTestType m(
      "groot",  //
      KV{5, MapTreeTestType("pp", KV{4, MapTreeTestType("ss")},
                            KV{1, MapTreeTestType("tt")})},
      KV{3, MapTreeTestType("qq", KV{2, MapTreeTestType("ww")},
                            KV{6, MapTreeTestType("vv")})});
  std::ostringstream stream;
  m.PrintTree(stream,
              [](std::ostream &s, const std::string &text,
                 size_t indent) -> std::ostream & {
                const Spacer wrap(indent + 4);
                for (const auto c : text) {
                  s << '\n' << wrap << c;
                }
                return s << '\n' << Spacer(indent);
              });
  EXPECT_EQ(stream.str(),  //
            R"({ (
    g
    r
    o
    o
    t
)
  3: { (
      q
      q
  )
    2: { (
        w
        w
    ) }
    6: { (
        v
        v
    ) }
  }
  5: { (
      p
      p
  )
    1: { (
        t
        t
    ) }
    4: { (
        s
        s
    ) }
  }
})");
}

}  // namespace
}  // namespace verible
