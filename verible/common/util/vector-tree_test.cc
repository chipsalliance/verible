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

#include "verible/common/util/vector-tree.h"

#include <cstddef>
#include <ostream>
#include <sstream>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "verible/common/util/logging.h"
#include "verible/common/util/tree-operations.h"
#include "verible/common/util/vector-tree-test-util.h"

namespace verible {
namespace {

using ::testing::ElementsAre;
using verible::testing::MakePath;
using verible::testing::NamedInterval;
using verible::testing::VectorTreeTestType;

template <class Tree>
void ExpectPath(const Tree &tree, std::string_view expect) {
  std::ostringstream stream;
  stream << NodePath(tree);
  EXPECT_EQ(stream.str(), expect);
}

// Test that basic Tree construction works on a singleton node.
TEST(VectorTreeTest, RootOnly) {
  const VectorTreeTestType tree(verible::testing::MakeRootOnlyExampleTree());
  EXPECT_TRUE(is_leaf(tree));
  EXPECT_EQ(tree.Parent(), nullptr);
  EXPECT_EQ(NumAncestors(tree), 0);
  EXPECT_EQ(BirthRank(tree), 0);  // no parent
  EXPECT_TRUE(IsFirstChild(tree));
  EXPECT_TRUE(IsLastChild(tree));
  EXPECT_EQ(&Root(tree), &tree);

  const auto &value = tree.Value();
  EXPECT_EQ(value.left, 0);
  EXPECT_EQ(value.right, 2);
  EXPECT_EQ(value.name, "root");

  ExpectPath(tree, "{}");
}

TEST(VectorTreeTest, RootOnlyDescendants) {
  VectorTreeTestType tree(verible::testing::MakeRootOnlyExampleTree());
  EXPECT_EQ(&LeftmostDescendant(tree), &tree);
  EXPECT_EQ(&RightmostDescendant(tree), &tree);
  {  // Test const method variants.
    const auto &ctree(tree);
    EXPECT_EQ(&LeftmostDescendant(ctree), &ctree);
    EXPECT_EQ(&RightmostDescendant(ctree), &ctree);
  }
}

TEST(VectorTreeTest, RootOnlyHasAncestor) {
  VectorTreeTestType tree(verible::testing::MakeRootOnlyExampleTree());
  EXPECT_FALSE(HasAncestor(tree, nullptr));
  EXPECT_FALSE(HasAncestor(tree, &tree));

  // Separate tree
  VectorTreeTestType tree2(verible::testing::MakeRootOnlyExampleTree());
  EXPECT_FALSE(HasAncestor(tree2, &tree));
  EXPECT_FALSE(HasAncestor(tree, &tree2));
}

TEST(VectorTreeTest, RootOnlyLeafIteration) {
  VectorTreeTestType tree(verible::testing::MakeRootOnlyExampleTree());
  EXPECT_EQ(NextLeaf(tree), nullptr);
  EXPECT_EQ(PreviousLeaf(tree), nullptr);
  {  // const method variants
    const auto &ctree(tree);
    EXPECT_EQ(NextLeaf(ctree), nullptr);
    EXPECT_EQ(PreviousLeaf(ctree), nullptr);
  }
}

TEST(VectorTreeTest, RootOnlySiblingIteration) {
  VectorTreeTestType tree(verible::testing::MakeRootOnlyExampleTree());
  EXPECT_EQ(verible::NextSibling(tree), nullptr);
  EXPECT_EQ(verible::PreviousSibling(tree), nullptr);
  {  // const method variants
    const auto &ctree(tree);
    EXPECT_EQ(verible::NextSibling(ctree), nullptr);
    EXPECT_EQ(verible::PreviousSibling(ctree), nullptr);
  }
}

TEST(VectorTreeTest, CopyAssignEmpty) {
  using tree_type = VectorTree<int>;
  const tree_type tree(1);  // Root only tree.
  const tree_type expected(1);
  tree_type tree2(5);
  tree2 = tree;
  {
    const auto result_pair = DeepEqual(tree2, expected);
    EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
    EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
  }
  {  // verify original copy
    const auto result_pair = DeepEqual(tree, expected);
    EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
    EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
  }
}

TEST(VectorTreeTest, CopyAssignDeep) {
  using tree_type = VectorTree<int>;
  const tree_type tree(1,
                       tree_type(2, tree_type(3, tree_type(4, tree_type(5)))));
  const tree_type expected(
      1, tree_type(2, tree_type(3, tree_type(4, tree_type(5)))));
  tree_type tree2(6);
  tree2 = tree;
  {
    const auto result_pair = DeepEqual(tree2, expected);
    EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
    EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
  }
  {  // verify original copy
    const auto result_pair = DeepEqual(tree, expected);
    EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
    EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
  }
}

TEST(VectorTreeTest, CopyInitializeEmpty) {
  using tree_type = VectorTree<int>;
  const tree_type tree(1);  // Root only tree.
  const tree_type expected(1);
  // Specifically testing copy here.
  // clang-format off
  tree_type tree2 = tree;  // NOLINT(performance-unnecessary-copy-initialization)
  // clang-format on
  {
    const auto result_pair = DeepEqual(tree2, expected);
    EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
    EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
  }
  {  // verify original copy
    const auto result_pair = DeepEqual(tree, expected);
    EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
    EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
  }
}

TEST(VectorTreeTest, CopyInitializeDeep) {
  using tree_type = VectorTree<int>;
  const tree_type tree(1,
                       tree_type(2, tree_type(3, tree_type(4, tree_type(5)))));
  const tree_type expected(
      1, tree_type(2, tree_type(3, tree_type(4, tree_type(5)))));
  // Specifically testing copy here.
  // clang-format off
  tree_type tree2 = tree;  // NOLINT(performance-unnecessary-copy-initialization)
  // clang-format on
  {
    const auto result_pair = DeepEqual(tree2, expected);
    EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
    EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
  }
  {  // verify original copy
    const auto result_pair = DeepEqual(tree, expected);
    EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
    EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
  }
}

TEST(VectorTreeTest, MoveInitializeEmpty) {
  using tree_type = VectorTree<int>;
  tree_type tree(1);  // Root only tree.
  const tree_type expected(1);
  tree_type tree2 = std::move(tree);
  const auto result_pair = DeepEqual(tree2, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(VectorTreeTest, MoveInitializeDeep) {
  using tree_type = VectorTree<int>;
  tree_type tree(1, tree_type(2, tree_type(3, tree_type(4, tree_type(5)))));
  const tree_type expected(
      1, tree_type(2, tree_type(3, tree_type(4, tree_type(5)))));
  tree_type tree2 = std::move(tree);
  const auto result_pair = DeepEqual(tree2, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(VectorTreeTest, MoveAssignEmpty) {
  using tree_type = VectorTree<int>;
  tree_type tree(1);  // Root only tree.
  const tree_type expected(1);
  tree_type tree2(2);
  VLOG(4) << "about to move.";
  tree2 = std::move(tree);
  VLOG(4) << "done moving.";
  const auto result_pair = DeepEqual(tree2, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(VectorTreeTest, MoveAssignDeep) {
  using tree_type = VectorTree<int>;
  tree_type tree(1, tree_type(2, tree_type(3, tree_type(4, tree_type(5)))));
  tree_type tree2(7, tree_type(8, tree_type(9)));
  tree2 = std::move(tree);
  const tree_type expected(
      1, tree_type(2, tree_type(3, tree_type(4, tree_type(5)))));
  const auto result_pair = DeepEqual(tree2, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(VectorTreeTest, SwapUnrelatedRoots) {
  using tree_type = VectorTree<int>;
  tree_type tree(1, tree_type(2, tree_type(3, tree_type(4, tree_type(5)))));
  tree_type tree2(7, tree_type(8, tree_type(9)));
  const tree_type t1_expected(tree2);  // deep-copy
  const tree_type t2_expected(tree);   // deep-copy
  swap(tree, tree2);                   // verible::swap, using ADL
  {
    const auto result_pair = DeepEqual(tree, t1_expected);
    EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
    EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
  }
  {
    const auto result_pair = DeepEqual(tree2, t2_expected);
    EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
    EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
  }
}

TEST(VectorTreeTest, SwapUnrelatedSubtrees) {
  using tree_type = VectorTree<int>;
  tree_type tree(1, tree_type(2, tree_type(3, tree_type(4, tree_type(5)))));
  tree_type tree2(7, tree_type(8, tree_type(9, tree_type(10))));
  swap(tree.Children()[0], tree2.Children()[0]);
  {
    const tree_type expected(1, tree_type(8, tree_type(9, tree_type(10))));
    const auto result_pair = DeepEqual(tree, expected);
    EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
    EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
  }
  {
    const tree_type expected(
        7, tree_type(2, tree_type(3, tree_type(4, tree_type(5)))));
    const auto result_pair = DeepEqual(tree2, expected);
    EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
    EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
  }
}

TEST(VectorTreeTest, SwapSiblings) {
  using tree_type = VectorTree<int>;
  tree_type tree(1,  //
                 tree_type(0),
                 tree_type(2, tree_type(3, tree_type(4, tree_type(5)))),
                 tree_type(7, tree_type(8, tree_type(9))), tree_type(11));
  swap(tree.Children()[2], tree.Children()[1]);

  const tree_type expected(
      1,  //
      tree_type(0), tree_type(7, tree_type(8, tree_type(9))),
      tree_type(2, tree_type(3, tree_type(4, tree_type(5)))), tree_type(11));
  const auto result_pair = DeepEqual(tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(VectorTreeTest, SwapDistantCousins) {
  using tree_type = VectorTree<int>;
  tree_type tree(1,  //
                 tree_type(0),
                 tree_type(2, tree_type(3, tree_type(4, tree_type(5)))),
                 tree_type(7, tree_type(8, tree_type(9))), tree_type(11));
  swap(tree.Children()[2], tree.Children()[1].Children()[0]);

  const tree_type expected(
      1,  //
      tree_type(0), tree_type(2, tree_type(7, tree_type(8, tree_type(9)))),
      tree_type(3, tree_type(4, tree_type(5))), tree_type(11));
  const auto result_pair = DeepEqual(tree, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(VectorTreeTest, StructureEqualRootToRoot) {
  const VectorTreeTestType ltree(verible::testing::MakeRootOnlyExampleTree());
  const VectorTreeTestType rtree(verible::testing::MakeRootOnlyExampleTree());
  const auto result_pair = StructureEqual(ltree, rtree);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(VectorTreeTest, StructureEqualRootToRootIgnoringValue) {
  VectorTreeTestType ltree(verible::testing::MakeRootOnlyExampleTree());
  VectorTreeTestType rtree(verible::testing::MakeRootOnlyExampleTree());
  ltree.Value().left = 11;
  rtree.Value().left = 34;
  const auto result_pair = StructureEqual(ltree, rtree);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(VectorTreeTest, DeepEqualRootToRoot) {
  const VectorTreeTestType ltree(verible::testing::MakeRootOnlyExampleTree());
  const VectorTreeTestType rtree(verible::testing::MakeRootOnlyExampleTree());
  const auto result_pair = DeepEqual(ltree, rtree);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(VectorTreeTest, DeepEqualRootToRootValueDifferent) {
  VectorTreeTestType ltree(verible::testing::MakeRootOnlyExampleTree());
  VectorTreeTestType rtree(verible::testing::MakeRootOnlyExampleTree());
  ltree.Value().left = 11;
  rtree.Value().left = 34;
  const auto result_pair = DeepEqual(ltree, rtree);
  EXPECT_EQ(result_pair.left, &ltree);
  EXPECT_EQ(result_pair.right, &rtree);
}

struct NameOnly {
  std::string_view name;

  explicit NameOnly(const NamedInterval &v) : name(v.name) {}
};

std::ostream &operator<<(std::ostream &stream, const NameOnly &n) {
  return stream << '(' << n.name << ")\n";
}

static NameOnly NameOnlyConverter(const VectorTreeTestType &node) {
  return NameOnly(node.Value());
}

// Heterogeneous comparison.
bool operator==(const NamedInterval &left, const NameOnly &right) {
  return left.name == right.name;
}

TEST(VectorTreeTest, RootOnlyTreeTransformConstruction) {
  const VectorTreeTestType tree(verible::testing::MakeRootOnlyExampleTree());
  const auto other_tree =
      Transform<VectorTree<NameOnly>>(tree, NameOnlyConverter);
  EXPECT_TRUE(is_leaf(other_tree));
  EXPECT_EQ(other_tree.Parent(), nullptr);
  EXPECT_EQ(NumAncestors(other_tree), 0);
  EXPECT_EQ(BirthRank(other_tree), 0);  // no parent

  const auto &value = other_tree.Value();
  EXPECT_EQ(value.name, "root");
}

TEST(VectorTreeTest, RootOnlyTreeTransformComparisonMatches) {
  const VectorTreeTestType tree(verible::testing::MakeRootOnlyExampleTree());
  const auto other_tree =
      Transform<VectorTree<NameOnly>>(tree, NameOnlyConverter);
  {
    const auto result_pair = StructureEqual(tree, other_tree);
    EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
    EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
  }
  {
    const auto result_pair = StructureEqual(other_tree, tree);  // swapped
    EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
    EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
  }
  {
    // Uses hetergeneous value comparison.
    const auto result_pair = DeepEqual(tree, other_tree);
    EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
    EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
  }
}

TEST(VectorTreeTest, RootOnlyTreeTransformComparisonDiffer) {
  const VectorTreeTestType tree(verible::testing::MakeRootOnlyExampleTree());
  auto other_tree = Transform<VectorTree<NameOnly>>(tree, NameOnlyConverter);
  other_tree.Value().name = "groot";
  {
    const auto result_pair = StructureEqual(tree, other_tree);
    EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
    EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
  }
  {
    // Uses hetergeneous value comparison.
    const auto result_pair = DeepEqual(tree, other_tree);
    EXPECT_EQ(result_pair.left, &tree);
    EXPECT_EQ(result_pair.right, &other_tree);
  }
}

TEST(VectorTreeTest, NewChild) {
  VectorTreeTestType tree(verible::testing::MakeRootOnlyExampleTree());
  {
    tree.Children().emplace_back(NamedInterval(1, 2, "child"));
    auto *child = &tree.Children().back();
    EXPECT_EQ(child->Parent(), &tree);
    EXPECT_EQ(&Root(*child), &tree);
    EXPECT_TRUE(verible::is_leaf(*child));

    const auto &value(child->Value());
    EXPECT_EQ(value.left, 1);
    EXPECT_EQ(value.right, 2);
    EXPECT_EQ(value.name, "child");

    ExpectPath(*child, "{0}");
  }
  {
    tree.Children().emplace_back(NamedInterval(2, 3, "lil-bro"));
    auto *child = &tree.Children().back();
    EXPECT_EQ(child->Parent(), &tree);
    EXPECT_EQ(&Root(*child), &tree);
    EXPECT_TRUE(verible::is_leaf(*child));

    const auto &value(child->Value());
    EXPECT_EQ(value.left, 2);
    EXPECT_EQ(value.right, 3);
    EXPECT_EQ(value.name, "lil-bro");
    // Note: first child may have moved due to realloc

    ExpectPath(*child, "{1}");
  }
}

TEST(VectorTreeTest, NewSibling) {
  VectorTreeTestType tree(verible::testing::MakeRootOnlyExampleTree());
  {
    tree.Children().emplace_back(NamedInterval(1, 2, "child"));
    auto *first_child = &tree.Children().back();
    ExpectPath(*first_child, "{0}");

    auto &siblings = first_child->Parent()->Children();
    siblings.emplace_back(NamedInterval(2, 3, "lil-bro"));
    auto *second_child = &siblings.back();
    // Recall that emplace_back() may invalidate reference to first_child.
    EXPECT_EQ(second_child->Parent(), &tree);
    EXPECT_EQ(&Root(*second_child), &tree);
    EXPECT_TRUE(verible::is_leaf(*second_child));

    const auto &value(second_child->Value());
    EXPECT_EQ(value.left, 2);
    EXPECT_EQ(value.right, 3);
    EXPECT_EQ(value.name, "lil-bro");

    ExpectPath(*second_child, "{1}");
  }
}

TEST(VectorTreeTest, OneChildPolicy) {
  const auto tree = verible::testing::MakeOneChildPolicyExampleTree();
  EXPECT_EQ(tree.Parent(), nullptr);
  EXPECT_FALSE(is_leaf(tree));

  const auto &value = tree.Value();
  EXPECT_EQ(value.left, 0);
  EXPECT_EQ(value.right, 3);
  EXPECT_EQ(value.name, "root");

  {
    const auto &child = tree.Children().front();
    EXPECT_EQ(child.Parent(), &tree);
    EXPECT_EQ(&Root(child), &tree);
    EXPECT_FALSE(is_leaf(child));
    EXPECT_EQ(NumAncestors(child), 1);
    EXPECT_EQ(BirthRank(child), 0);
    EXPECT_TRUE(IsFirstChild(child));
    EXPECT_TRUE(IsLastChild(child));

    const auto &cvalue = child.Value();
    EXPECT_EQ(cvalue.left, 0);
    EXPECT_EQ(cvalue.right, 3);
    EXPECT_EQ(cvalue.name, "gen1");
    ExpectPath(child, "{0}");

    EXPECT_EQ(verible::NextSibling(child), nullptr);
    EXPECT_EQ(verible::PreviousSibling(child), nullptr);

    // The invoking node need not be a leaf.
    EXPECT_EQ(NextLeaf(child), nullptr);
    EXPECT_EQ(PreviousLeaf(child), nullptr);

    {
      const auto &grandchild = child.Children().front();
      EXPECT_EQ(grandchild.Parent(), &child);
      EXPECT_EQ(&Root(grandchild), &tree);
      EXPECT_TRUE(is_leaf(grandchild));
      EXPECT_EQ(NumAncestors(grandchild), 2);
      EXPECT_EQ(BirthRank(grandchild), 0);
      EXPECT_TRUE(IsFirstChild(grandchild));
      EXPECT_TRUE(IsLastChild(grandchild));

      const auto &gcvalue = grandchild.Value();
      EXPECT_EQ(gcvalue.left, 0);
      EXPECT_EQ(gcvalue.right, 3);
      EXPECT_EQ(gcvalue.name, "gen2");
      ExpectPath(grandchild, "{0,0}");

      // As the ancestry chain is linear, Leftmost == Rightmost.
      EXPECT_EQ(&LeftmostDescendant(child), &grandchild);
      EXPECT_EQ(&RightmostDescendant(child), &grandchild);
      EXPECT_EQ(&LeftmostDescendant(tree), &grandchild);
      EXPECT_EQ(&RightmostDescendant(tree), &grandchild);

      EXPECT_EQ(verible::NextSibling(grandchild), nullptr);
      EXPECT_EQ(verible::PreviousSibling(grandchild), nullptr);

      // There is still only a single leaf in a one-child tree,
      // thus next and previous do not exist.
      EXPECT_EQ(NextLeaf(grandchild), nullptr);
      EXPECT_EQ(PreviousLeaf(grandchild), nullptr);
    }
  }
}

TEST(VectorTreeTest, OneChildPolicyHasAncestor) {
  const auto tree = verible::testing::MakeOneChildPolicyExampleTree();

  {
    const auto &child = tree.Children().front();

    EXPECT_FALSE(HasAncestor(tree, &child));
    EXPECT_TRUE(HasAncestor(child, &tree));

    {
      const auto &grandchild = child.Children().front();

      EXPECT_FALSE(HasAncestor(child, &grandchild));
      EXPECT_TRUE(HasAncestor(grandchild, &child));

      EXPECT_FALSE(HasAncestor(tree, &grandchild));
      EXPECT_TRUE(HasAncestor(grandchild, &tree));
    }
  }
}

TEST(VectorTreeTest, StructureEqualOneChild) {
  const VectorTreeTestType ltree(
      verible::testing::MakeOneChildPolicyExampleTree());
  const VectorTreeTestType rtree(
      verible::testing::MakeOneChildPolicyExampleTree());
  const auto result_pair = StructureEqual(ltree, rtree);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(VectorTreeTest, StructureEqualOneChildIgnoreValues) {
  VectorTreeTestType ltree(verible::testing::MakeOneChildPolicyExampleTree());
  VectorTreeTestType rtree(verible::testing::MakeOneChildPolicyExampleTree());
  ltree.Children()[0].Value().right = 32;
  rtree.Children()[0].Value().right = 77;
  const auto result_pair = StructureEqual(ltree, rtree);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(VectorTreeTest, DeepEqualOneChild) {
  const VectorTreeTestType ltree(
      verible::testing::MakeOneChildPolicyExampleTree());
  const VectorTreeTestType rtree(
      verible::testing::MakeOneChildPolicyExampleTree());
  const auto result_pair = DeepEqual(ltree, rtree);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(VectorTreeTest, DeepEqualOneChildDifferentChildValues) {
  VectorTreeTestType ltree(verible::testing::MakeOneChildPolicyExampleTree());
  VectorTreeTestType rtree(verible::testing::MakeOneChildPolicyExampleTree());
  auto &lchild = ltree.Children()[0];
  auto &rchild = rtree.Children()[0];
  lchild.Value().right = 32;
  rchild.Value().right = 77;
  const auto result_pair = DeepEqual(ltree, rtree);
  EXPECT_EQ(result_pair.left, &lchild);
  EXPECT_EQ(result_pair.right, &rchild);
}

TEST(VectorTreeTest, DeepEqualOneChildDifferentGrandchildValues) {
  VectorTreeTestType ltree(verible::testing::MakeOneChildPolicyExampleTree());
  VectorTreeTestType rtree(verible::testing::MakeOneChildPolicyExampleTree());
  auto &lchild = ltree.Children()[0].Children()[0];  // only grandchild
  auto &rchild = rtree.Children()[0].Children()[0];  // only grandchild
  lchild.Value().right = 32;
  rchild.Value().right = 77;
  const auto result_pair = DeepEqual(ltree, rtree);
  EXPECT_EQ(result_pair.left, &lchild);
  EXPECT_EQ(result_pair.right, &rchild);
}

TEST(VectorTreeTest, DeepEqualOneChildGrandchildValuesHeterogeneous) {
  VectorTreeTestType ltree(verible::testing::MakeOneChildPolicyExampleTree());
  auto rtree = Transform<VectorTree<NameOnly>>(ltree, NameOnlyConverter);
  {  // Match
    const auto result_pair = DeepEqual(ltree, rtree);
    EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
    EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
  }
  {                                          // Mismatch
    const std::vector<size_t> path({0, 0});  // only grandchild
    auto &lchild = DescendPath(ltree, path.begin(), path.end());
    auto &rchild = DescendPath(rtree, path.begin(), path.end());
    lchild.Value().name = "alex";
    rchild.Value().name = "james";
    const auto result_pair = DeepEqual(ltree, rtree);
    EXPECT_EQ(result_pair.left, &lchild);
    EXPECT_EQ(result_pair.right, &rchild);
  }
}

template <typename T>
void VerifyFamilyTree(const VectorTree<T> &tree) {
  EXPECT_EQ(tree.Parent(), nullptr);
  EXPECT_EQ(&Root(tree), &tree);
  EXPECT_FALSE(is_leaf(tree));
  EXPECT_EQ(NumAncestors(tree), 0);
  EXPECT_EQ(BirthRank(tree), 0);

  const auto tree_path = MakePath(tree);
  EXPECT_TRUE(tree_path.empty());
  EXPECT_EQ(&DescendPath(tree, tree_path.begin(), tree_path.end()), &tree);

  for (int i = 0; i < 2; ++i) {
    const auto &child = tree.Children()[i];
    EXPECT_EQ(child.Parent(), &tree);
    EXPECT_EQ(&Root(child), &tree);
    EXPECT_FALSE(is_leaf(child));
    EXPECT_EQ(NumAncestors(child), 1);
    EXPECT_EQ(BirthRank(child), i);
    EXPECT_EQ(IsFirstChild(child), i == 0);
    EXPECT_EQ(IsLastChild(child), i == 1);

    const auto child_path = MakePath(child);
    EXPECT_THAT(child_path, ElementsAre(i));
    EXPECT_EQ(&DescendPath(tree, child_path.begin(), child_path.end()), &child);

    for (int j = 0; j < 2; ++j) {
      const auto &grandchild = child.Children()[j];
      EXPECT_EQ(grandchild.Parent(), &child);
      EXPECT_EQ(&Root(grandchild), &tree);
      EXPECT_TRUE(is_leaf(grandchild));
      EXPECT_EQ(NumAncestors(grandchild), 2);
      EXPECT_EQ(BirthRank(grandchild), j);
      EXPECT_EQ(IsFirstChild(grandchild), j == 0);
      EXPECT_EQ(IsLastChild(grandchild), j == 1);

      const auto grandchild_path = MakePath(grandchild);
      EXPECT_THAT(grandchild_path, ElementsAre(i, j));
      const auto begin = grandchild_path.begin(), end = grandchild_path.end();
      EXPECT_EQ(&DescendPath(tree, begin, end), &grandchild);
      EXPECT_EQ(&DescendPath(child, begin + 1, end), &grandchild);
      ExpectPath(grandchild, absl::StrCat("{", i, ",", j, "}"));
    }
  }
}

// Tests internal consistency of BirthRank and Path properties.
TEST(VectorTreeTest, FamilyTreeMembers) {
  const auto tree = verible::testing::MakeExampleFamilyTree();
  VerifyFamilyTree(tree);
}

// Tests internal consistency and properties of copied tree.
TEST(VectorTreeTest, FamilyTreeCopiedMembers) {
  const auto orig_tree = verible::testing::MakeExampleFamilyTree();
  // Specifically testing copy here.
  // clang-format off
  const auto tree(orig_tree);  // NOLINT(performance-unnecessary-copy-initialization)
  // clang-format on
  VerifyFamilyTree(orig_tree);
  VerifyFamilyTree(tree);

  const auto result_pair = DeepEqual(orig_tree, tree);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

// Tests internal consistency and properties of moved tree.
TEST(VectorTreeTest, FamilyTreeMovedMembers) {
  auto orig_tree = verible::testing::MakeExampleFamilyTree();
  const auto tree(std::move(orig_tree));  // moved
  VerifyFamilyTree(tree);
}

TEST(VectorTreeTest, FamilyTreeLeftRightmostDescendants) {
  auto tree = verible::testing::MakeExampleFamilyTree();
  const auto left_path = {0, 0};
  const auto right_path = {1, 1};
  {  // Test mutable method variants.
    EXPECT_EQ(&LeftmostDescendant(tree),
              &DescendPath(tree, left_path.begin(), left_path.end()));
    EXPECT_EQ(&RightmostDescendant(tree),
              &DescendPath(tree, right_path.begin(), right_path.end()));
  }
  {  // Test const method variants.
    const auto &ctree(tree);
    EXPECT_EQ(&LeftmostDescendant(ctree),
              &DescendPath(ctree, left_path.begin(), left_path.end()));
    EXPECT_EQ(&RightmostDescendant(ctree),
              &DescendPath(ctree, right_path.begin(), right_path.end()));
  }
}

TEST(VectorTreeTest, FamilyTreeHasAncestor) {
  const auto tree = verible::testing::MakeExampleFamilyTree();
  const auto &child_0 = tree.Children()[0];
  const auto &child_1 = tree.Children()[1];
  const auto &grandchild_00 = child_0.Children()[0];
  const auto &grandchild_01 = child_0.Children()[1];
  const auto &grandchild_10 = child_1.Children()[0];
  const auto &grandchild_11 = child_1.Children()[1];

  EXPECT_FALSE(HasAncestor(tree, &child_0));
  EXPECT_FALSE(HasAncestor(tree, &child_1));
  EXPECT_FALSE(HasAncestor(tree, &grandchild_00));
  EXPECT_FALSE(HasAncestor(tree, &grandchild_01));
  EXPECT_FALSE(HasAncestor(tree, &grandchild_10));
  EXPECT_FALSE(HasAncestor(tree, &grandchild_11));

  EXPECT_TRUE(HasAncestor(child_0, &tree));
  EXPECT_TRUE(HasAncestor(child_1, &tree));
  EXPECT_TRUE(HasAncestor(grandchild_00, &tree));
  EXPECT_TRUE(HasAncestor(grandchild_01, &tree));
  EXPECT_TRUE(HasAncestor(grandchild_10, &tree));
  EXPECT_TRUE(HasAncestor(grandchild_11, &tree));

  EXPECT_FALSE(HasAncestor(child_0, &child_1));
  EXPECT_FALSE(HasAncestor(child_1, &child_0));

  EXPECT_FALSE(HasAncestor(child_0, &grandchild_00));
  EXPECT_FALSE(HasAncestor(child_0, &grandchild_01));
  EXPECT_FALSE(HasAncestor(child_0, &grandchild_10));
  EXPECT_FALSE(HasAncestor(child_0, &grandchild_11));
  EXPECT_FALSE(HasAncestor(child_1, &grandchild_00));
  EXPECT_FALSE(HasAncestor(child_1, &grandchild_01));
  EXPECT_FALSE(HasAncestor(child_1, &grandchild_10));
  EXPECT_FALSE(HasAncestor(child_1, &grandchild_11));

  EXPECT_TRUE(HasAncestor(grandchild_00, &child_0));
  EXPECT_FALSE(HasAncestor(grandchild_00, &child_1));
  EXPECT_TRUE(HasAncestor(grandchild_01, &child_0));
  EXPECT_FALSE(HasAncestor(grandchild_01, &child_1));
  EXPECT_FALSE(HasAncestor(grandchild_10, &child_0));
  EXPECT_TRUE(HasAncestor(grandchild_10, &child_1));
  EXPECT_FALSE(HasAncestor(grandchild_11, &child_0));
  EXPECT_TRUE(HasAncestor(grandchild_11, &child_1));
}

TEST(VectorTreeTest, FamilyTreeNextPreviousSiblings) {
  auto tree = verible::testing::MakeExampleFamilyTree();
  auto &child_0 = tree.Children()[0];
  auto &child_1 = tree.Children()[1];
  auto &grandchild_00 = child_0.Children()[0];
  auto &grandchild_01 = child_0.Children()[1];
  auto &grandchild_10 = child_1.Children()[0];
  auto &grandchild_11 = child_1.Children()[1];

  // Verify child generation.
  EXPECT_EQ(verible::NextSibling(child_0), &child_1);
  EXPECT_EQ(verible::NextSibling(child_1), nullptr);
  EXPECT_EQ(verible::PreviousSibling(child_0), nullptr);
  EXPECT_EQ(verible::PreviousSibling(child_1), &child_0);

  // Verify grandchild generation.
  EXPECT_EQ(verible::NextSibling(grandchild_00), &grandchild_01);
  EXPECT_EQ(verible::NextSibling(grandchild_01), nullptr);
  EXPECT_EQ(verible::NextSibling(grandchild_10), &grandchild_11);
  EXPECT_EQ(verible::NextSibling(grandchild_11), nullptr);
  EXPECT_EQ(verible::PreviousSibling(grandchild_00), nullptr);
  EXPECT_EQ(verible::PreviousSibling(grandchild_01), &grandchild_00);
  EXPECT_EQ(verible::PreviousSibling(grandchild_10), nullptr);
  EXPECT_EQ(verible::PreviousSibling(grandchild_11), &grandchild_10);

  {  // same but testing const method variants.
    const auto &cchild_0(child_0);
    const auto &cchild_1(child_1);
    const auto &cgrandchild_00(grandchild_00);
    const auto &cgrandchild_01(grandchild_01);
    const auto &cgrandchild_10(grandchild_10);
    const auto &cgrandchild_11(grandchild_11);

    // Verify child generation.
    EXPECT_EQ(verible::NextSibling(cchild_0), &cchild_1);
    EXPECT_EQ(verible::NextSibling(cchild_1), nullptr);
    EXPECT_EQ(verible::PreviousSibling(cchild_0), nullptr);
    EXPECT_EQ(verible::PreviousSibling(cchild_1), &cchild_0);

    // Verify grandchild generation.
    EXPECT_EQ(verible::NextSibling(cgrandchild_00), &cgrandchild_01);
    EXPECT_EQ(verible::NextSibling(cgrandchild_01), nullptr);
    EXPECT_EQ(verible::NextSibling(cgrandchild_10), &cgrandchild_11);
    EXPECT_EQ(verible::NextSibling(cgrandchild_11), nullptr);
    EXPECT_EQ(verible::PreviousSibling(cgrandchild_00), nullptr);
    EXPECT_EQ(verible::PreviousSibling(cgrandchild_01), &cgrandchild_00);
    EXPECT_EQ(verible::PreviousSibling(cgrandchild_10), nullptr);
    EXPECT_EQ(verible::PreviousSibling(cgrandchild_11), &cgrandchild_10);
  }
}

TEST(VectorTreeTest, FamilyTreeNextPreviousLeafChain) {
  auto tree = verible::testing::MakeExampleFamilyTree();
  auto &grandchild_00 = tree.Children()[0].Children()[0];
  auto &grandchild_01 = tree.Children()[0].Children()[1];
  auto &grandchild_10 = tree.Children()[1].Children()[0];
  auto &grandchild_11 = tree.Children()[1].Children()[1];

  // Verify forward links.
  EXPECT_EQ(NextLeaf(grandchild_00), &grandchild_01);
  EXPECT_EQ(NextLeaf(grandchild_01), &grandchild_10);
  EXPECT_EQ(NextLeaf(grandchild_10), &grandchild_11);
  EXPECT_EQ(NextLeaf(grandchild_11), nullptr);

  // Verify reverse links.
  EXPECT_EQ(PreviousLeaf(grandchild_00), nullptr);
  EXPECT_EQ(PreviousLeaf(grandchild_01), &grandchild_00);
  EXPECT_EQ(PreviousLeaf(grandchild_10), &grandchild_01);
  EXPECT_EQ(PreviousLeaf(grandchild_11), &grandchild_10);

  {  // same but testing const method variants.
    const auto &cgrandchild_00(grandchild_00);
    const auto &cgrandchild_01(grandchild_01);
    const auto &cgrandchild_10(grandchild_10);
    const auto &cgrandchild_11(grandchild_11);

    // Verify forward links.
    EXPECT_EQ(NextLeaf(cgrandchild_00), &cgrandchild_01);
    EXPECT_EQ(NextLeaf(cgrandchild_01), &cgrandchild_10);
    EXPECT_EQ(NextLeaf(cgrandchild_10), &cgrandchild_11);
    EXPECT_EQ(NextLeaf(cgrandchild_11), nullptr);

    // Verify reverse links.
    EXPECT_EQ(PreviousLeaf(cgrandchild_00), nullptr);
    EXPECT_EQ(PreviousLeaf(cgrandchild_01), &cgrandchild_00);
    EXPECT_EQ(PreviousLeaf(cgrandchild_10), &cgrandchild_01);
    EXPECT_EQ(PreviousLeaf(cgrandchild_11), &cgrandchild_10);
  }
}

TEST(VectorTreeTest, FamilyTreeMembersTransformed) {
  const auto orig_tree = verible::testing::MakeExampleFamilyTree();
  const auto tree =
      Transform<VectorTree<NameOnly>>(orig_tree, NameOnlyConverter);
  VerifyFamilyTree(orig_tree);
  VerifyFamilyTree(tree);

  {
    const auto result_pair = StructureEqual(orig_tree, tree);
    EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
    EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
  }
  {  // Converse comparison.
    const auto result_pair = StructureEqual(tree, orig_tree);
    EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
    EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
  }

  {
    // Uses hetergeneous value comparison.
    const auto result_pair = DeepEqual(orig_tree, tree);
    EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
    EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
  }

  // Mutate one grandchild at a time.
  for (size_t i = 0; i < 2; ++i) {
    for (size_t j = 0; j < 2; ++j) {
      auto ltree(orig_tree);  // copy
      auto rtree(tree);       // copy
      const std::vector<size_t> path({i, j});
      auto &lchild = DescendPath(ltree, path.begin(), path.end());
      auto &rchild = DescendPath(rtree, path.begin(), path.end());

      lchild.Value().name = "foo";
      rchild.Value().name = "bar";
      const auto result_pair = DeepEqual(ltree, rtree);
      EXPECT_EQ(result_pair.left, &lchild);
      EXPECT_EQ(result_pair.right, &rchild);
    }
  }
}

TEST(VectorTreeTest, FamilyTreeMembersDifferentStructureExtraGreatGrand) {
  // Mutate grandchildren structure.
  for (size_t i = 0; i < 2; ++i) {
    for (size_t j = 0; j < 2; ++j) {
      auto ltree = verible::testing::MakeExampleFamilyTree();
      auto rtree = verible::testing::MakeExampleFamilyTree();
      const std::vector<size_t> path({i, j});
      auto &lchild = DescendPath(ltree, path.begin(), path.end());
      auto &rchild = DescendPath(rtree, path.begin(), path.end());

      rchild.Children().emplace_back(NamedInterval(8, 9, "black-sheep"));
      const auto result_pair = StructureEqual(ltree, rtree);
      EXPECT_EQ(result_pair.left, &lchild);
      EXPECT_EQ(result_pair.right, &rchild);
    }
  }
}

TEST(VectorTreeTest, FamilyTreeMembersDifferentStructureExtraGrand) {
  // Mutate grandchildren structure.
  for (size_t i = 0; i < 2; ++i) {
    for (size_t j = 0; j < 2; ++j) {
      auto ltree = verible::testing::MakeExampleFamilyTree();
      auto rtree = verible::testing::MakeExampleFamilyTree();
      const std::vector<size_t> path({i, j});
      const auto &lparent = DescendPath(ltree, path.begin(), path.end() - 1);
      const auto &rparent = DescendPath(rtree, path.begin(), path.end() - 1);
      auto &rchild = DescendPath(rtree, path.begin(), path.end());

      rchild.Parent()->Children().emplace_back(
          NamedInterval(8, 9, "black-sheep"));
      // Note: `rchild` may have been moved and invalidated due to realloc.
      const auto result_pair = StructureEqual(ltree, rtree);
      EXPECT_EQ(result_pair.left, &lparent);
      EXPECT_EQ(result_pair.right, &rparent);
    }
  }
}

TEST(VectorTreeTest, FamilyTreeMembersDifferentStructureMissingGrand) {
  // Remove one set of grandchildren.
  for (size_t i = 0; i < 2; ++i) {
    auto ltree = verible::testing::MakeExampleFamilyTree();
    auto rtree = verible::testing::MakeExampleFamilyTree();
    const std::vector<size_t> path({i});
    auto &lchild = DescendPath(ltree, path.begin(), path.end());
    auto &rchild = DescendPath(rtree, path.begin(), path.end());

    lchild.Children().clear();
    const auto result_pair = StructureEqual(ltree, rtree);
    EXPECT_EQ(result_pair.left, &lchild);
    EXPECT_EQ(result_pair.right, &rchild);
  }
}

bool EqualNamedIntervalIgnoreName(const NamedInterval &l,
                                  const NamedInterval &r) {
  return l.left == r.left && l.right == r.right;  // ignore .name
}

TEST(VectorTreeTest, FamilyTreeMembersDeepEqualCustomComparator) {
  // Mutate grandchildren structure.
  for (size_t i = 0; i < 2; ++i) {
    for (size_t j = 0; j < 2; ++j) {
      auto ltree = verible::testing::MakeExampleFamilyTree();
      auto rtree = verible::testing::MakeExampleFamilyTree();
      const std::vector<size_t> path({i, j});
      auto &lchild = DescendPath(ltree, path.begin(), path.end());
      auto &rchild = DescendPath(rtree, path.begin(), path.end());
      lchild.Value().name = "larry";
      rchild.Value().name = "sergey";

      {
        const auto result_pair = DeepEqual(ltree, rtree);
        EXPECT_EQ(result_pair.left, &lchild);
        EXPECT_EQ(result_pair.right, &rchild);
      }
      {
        const auto result_pair =
            DeepEqual(ltree, rtree, EqualNamedIntervalIgnoreName);
        EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
        EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
      }
    }
  }
}

TEST(VectorTreeTest, NearestCommonAncestorNoneMutable) {
  using tree_type = VectorTree<int>;
  tree_type tree1{0}, tree2{0};
  EXPECT_EQ(NearestCommonAncestor(tree1, tree2), nullptr);
  EXPECT_EQ(NearestCommonAncestor(tree2, tree1), nullptr);
}

TEST(VectorTreeTest, NearestCommonAncestorNoneConst) {
  using tree_type = VectorTree<int>;
  const tree_type tree1{0}, tree2{0};
  EXPECT_EQ(NearestCommonAncestor(tree1, tree2), nullptr);
  EXPECT_EQ(NearestCommonAncestor(tree2, tree1), nullptr);
}

TEST(VectorTreeTest, NearestCommonAncestorSameMutable) {
  using tree_type = VectorTree<int>;
  tree_type tree{0};
  EXPECT_EQ(NearestCommonAncestor(tree, tree), &tree);
  EXPECT_EQ(NearestCommonAncestor(tree, tree), &tree);
}

TEST(VectorTreeTest, NearestCommonAncestorSameConst) {
  using tree_type = VectorTree<int>;
  const tree_type tree{0};
  EXPECT_EQ(NearestCommonAncestor(tree, tree), &tree);
  EXPECT_EQ(NearestCommonAncestor(tree, tree), &tree);
}

TEST(VectorTreeTest, NearestCommonAncestorOneIsRootConst) {
  using tree_type = VectorTree<int>;
  const tree_type tree(1,            //
                       tree_type(2,  //
                                 tree_type(4), tree_type(5)),
                       tree_type(3,  //
                                 tree_type(6), tree_type(7)));

  for (int i = 0; i < 2; ++i) {
    {
      const auto path = {i};
      auto &child = DescendPath(tree, path.begin(), path.end());
      EXPECT_EQ(NearestCommonAncestor(tree, child), &tree);
      EXPECT_EQ(NearestCommonAncestor(child, tree), &tree);
    }
    for (int j = 0; j < 2; ++j) {
      const auto path = {i, j};
      auto &grandchild = DescendPath(tree, path.begin(), path.end());
      EXPECT_EQ(NearestCommonAncestor(tree, grandchild), &tree);
      EXPECT_EQ(NearestCommonAncestor(grandchild, tree), &tree);
    }
  }
}

TEST(VectorTreeTest, NearestCommonAncestorNeitherIsRootConst) {
  using tree_type = VectorTree<int>;
  const tree_type tree(1,            //
                       tree_type(2,  //
                                 tree_type(4), tree_type(5)),
                       tree_type(3,  //
                                 tree_type(6), tree_type(7)));
  auto &left = tree.Children()[0];
  auto &right = tree.Children()[1];
  EXPECT_EQ(NearestCommonAncestor(left, right), &tree);
  EXPECT_EQ(NearestCommonAncestor(right, left), &tree);

  for (int i = 0; i < 2; ++i) {
    {
      const auto left_path = {0, i};
      auto &left_grandchild =
          DescendPath(tree, left_path.begin(), left_path.end());
      EXPECT_EQ(NearestCommonAncestor(left, left_grandchild), &left);
      EXPECT_EQ(NearestCommonAncestor(left_grandchild, left), &left);
      EXPECT_EQ(NearestCommonAncestor(right, left_grandchild), &tree);
      EXPECT_EQ(NearestCommonAncestor(left_grandchild, right), &tree);
    }
    {
      const auto right_path = {1, i};
      auto &right_grandchild =
          DescendPath(tree, right_path.begin(), right_path.end());
      EXPECT_EQ(NearestCommonAncestor(right, right_grandchild), &right);
      EXPECT_EQ(NearestCommonAncestor(right_grandchild, right), &right);
      EXPECT_EQ(NearestCommonAncestor(left, right_grandchild), &tree);
      EXPECT_EQ(NearestCommonAncestor(right_grandchild, left), &tree);
    }
  }
}

TEST(VectorTreeTest, ApplyPreOrderPrint) {
  const auto tree = verible::testing::MakeExampleFamilyTree();

  std::ostringstream stream;
  ApplyPreOrder(tree, [&stream](const NamedInterval &interval) {
    IntervalPrinter(&stream, interval);
  });
  EXPECT_EQ(stream.str(), absl::StrJoin(
                              {
                                  "(0, 4, grandparent)",
                                  "(0, 2, parent1)",
                                  "(0, 1, child1)",
                                  "(1, 2, child2)",
                                  "(2, 4, parent2)",
                                  "(2, 3, child3)",
                                  "(3, 4, child4)",
                              },
                              "\n") +
                              "\n");
}

TEST(VectorTreeTest, ApplyPreOrderPrintTransformed) {
  const auto orig_tree = verible::testing::MakeExampleFamilyTree();
  const auto tree =
      Transform<VectorTree<NameOnly>>(orig_tree, NameOnlyConverter);

  std::ostringstream stream;
  ApplyPreOrder(tree, [&stream](const NameOnly &n) { stream << n; });
  EXPECT_EQ(stream.str(), absl::StrJoin(
                              {
                                  "(grandparent)",
                                  "(parent1)",
                                  "(child1)",
                                  "(child2)",
                                  "(parent2)",
                                  "(child3)",
                                  "(child4)",
                              },
                              "\n") +
                              "\n");
}

TEST(VectorTreeTest, ApplyPostOrderPrint) {
  const auto tree = verible::testing::MakeExampleFamilyTree();

  std::ostringstream stream;
  ApplyPostOrder(tree, [&stream](const NamedInterval &interval) {
    IntervalPrinter(&stream, interval);
  });
  EXPECT_EQ(stream.str(), absl::StrJoin(
                              {
                                  "(0, 1, child1)",
                                  "(1, 2, child2)",
                                  "(0, 2, parent1)",
                                  "(2, 3, child3)",
                                  "(3, 4, child4)",
                                  "(2, 4, parent2)",
                                  "(0, 4, grandparent)",
                              },
                              "\n") +
                              "\n");
}

TEST(VectorTreeTest, ApplyPreOrderVerify) {
  const auto tree = verible::testing::MakeExampleFamilyTree();
  // Verify invariant at every node.
  ApplyPreOrder(tree, verible::testing::VerifyInterval);
}

TEST(VectorTreeTest, ApplyPostOrderVerify) {
  const auto tree = verible::testing::MakeExampleFamilyTree();
  // Verify invariant at every node.
  ApplyPostOrder(tree, verible::testing::VerifyInterval);
}

TEST(VectorTreeTest, ApplyPreOrderTransformValue) {
  auto tree = verible::testing::MakeExampleFamilyTree();

  // Transform intervals.
  std::vector<std::string_view> visit_order;
  const int shift = 2;
  ApplyPreOrder(tree, [=, &visit_order](NamedInterval &interval) {
    visit_order.push_back(interval.name);
    interval.left += shift;
    interval.right += shift;
  });
  EXPECT_THAT(visit_order,
              ElementsAre("grandparent", "parent1", "child1", "child2",
                          "parent2", "child3", "child4"));

  // Print output for verification.
  std::ostringstream stream;
  ApplyPreOrder(tree, [&stream](const NamedInterval &interval) {
    IntervalPrinter(&stream, interval);
  });
  EXPECT_EQ(stream.str(), absl::StrJoin(
                              {
                                  "(2, 6, grandparent)",
                                  "(2, 4, parent1)",
                                  "(2, 3, child1)",
                                  "(3, 4, child2)",
                                  "(4, 6, parent2)",
                                  "(4, 5, child3)",
                                  "(5, 6, child4)",
                              },
                              "\n") +
                              "\n");
}

TEST(VectorTreeTest, ApplyPreOrderTransformNode) {
  auto tree = verible::testing::MakeExampleFamilyTree();

  // Transform intervals.
  std::vector<std::string_view> visit_order;
  const int shift = 2;
  ApplyPreOrder(tree, [=, &visit_order](VectorTreeTestType &node) {
    auto &interval = node.Value();
    visit_order.push_back(interval.name);
    interval.left += shift;
    interval.right += shift;
  });
  EXPECT_THAT(visit_order,
              ElementsAre("grandparent", "parent1", "child1", "child2",
                          "parent2", "child3", "child4"));

  // Print output for verification.
  std::ostringstream stream;
  ApplyPreOrder(tree, [&stream](const NamedInterval &interval) {
    IntervalPrinter(&stream, interval);
  });
  EXPECT_EQ(stream.str(), absl::StrJoin(
                              {
                                  "(2, 6, grandparent)",
                                  "(2, 4, parent1)",
                                  "(2, 3, child1)",
                                  "(3, 4, child2)",
                                  "(4, 6, parent2)",
                                  "(4, 5, child3)",
                                  "(5, 6, child4)",
                              },
                              "\n") +
                              "\n");
}

TEST(VectorTreeTest, ApplyPostOrderTransformValue) {
  auto tree = verible::testing::MakeExampleFamilyTree();

  // Transform intervals.
  std::vector<std::string_view> visit_order;
  const int shift = 1;
  ApplyPostOrder(tree, [=, &visit_order](NamedInterval &interval) {
    visit_order.push_back(interval.name);
    interval.left += shift;
    interval.right += shift;
  });
  EXPECT_THAT(visit_order, ElementsAre("child1", "child2", "parent1", "child3",
                                       "child4", "parent2", "grandparent"));

  // Print output for verification.
  std::ostringstream stream;
  ApplyPostOrder(tree, [&stream](const NamedInterval &interval) {
    IntervalPrinter(&stream, interval);
  });
  EXPECT_EQ(stream.str(), absl::StrJoin(
                              {
                                  "(1, 2, child1)",
                                  "(2, 3, child2)",
                                  "(1, 3, parent1)",
                                  "(3, 4, child3)",
                                  "(4, 5, child4)",
                                  "(3, 5, parent2)",
                                  "(1, 5, grandparent)",
                              },
                              "\n") +
                              "\n");
}

TEST(VectorTreeTest, ApplyPostOrderTransformNode) {
  auto tree = verible::testing::MakeExampleFamilyTree();

  // Transform intervals.
  std::vector<std::string_view> visit_order;
  const int shift = 1;
  ApplyPostOrder(tree, [=, &visit_order](VectorTreeTestType &node) {
    auto &interval = node.Value();
    visit_order.push_back(interval.name);
    interval.left += shift;
    interval.right += shift;
  });
  EXPECT_THAT(visit_order, ElementsAre("child1", "child2", "parent1", "child3",
                                       "child4", "parent2", "grandparent"));

  // Print output for verification.
  std::ostringstream stream;
  ApplyPostOrder(tree, [&stream](const NamedInterval &interval) {
    IntervalPrinter(&stream, interval);
  });
  EXPECT_EQ(stream.str(), absl::StrJoin(
                              {
                                  "(1, 2, child1)",
                                  "(2, 3, child2)",
                                  "(1, 3, parent1)",
                                  "(3, 4, child3)",
                                  "(4, 5, child4)",
                                  "(3, 5, parent2)",
                                  "(1, 5, grandparent)",
                              },
                              "\n") +
                              "\n");
}

TEST(VectorTreeTest, HoistOnlyChildRootOnly) {
  VectorTreeTestType tree(verible::testing::MakeRootOnlyExampleTree());
  // No children, no change.
  EXPECT_FALSE(HoistOnlyChild(tree));

  EXPECT_TRUE(is_leaf(tree));
  EXPECT_EQ(tree.Parent(), nullptr);
  EXPECT_EQ(NumAncestors(tree), 0);
  EXPECT_EQ(BirthRank(tree), 0);  // no parent
  EXPECT_EQ(&Root(tree), &tree);

  const auto &value = tree.Value();
  EXPECT_EQ(value.left, 0);
  EXPECT_EQ(value.right, 2);
  EXPECT_EQ(value.name, "root");

  ExpectPath(tree, "{}");
}

TEST(VectorTreeTest, HoistOnlyChildOneChildTreeGreatestAncestor) {
  VectorTreeTestType tree(verible::testing::MakeOneChildPolicyExampleTree());
  // effectively remove the "root" generation
  EXPECT_TRUE(HoistOnlyChild(tree));
  {
    const auto &child = tree;
    EXPECT_EQ(child.Parent(), nullptr);
    EXPECT_EQ(&Root(child), &tree);
    EXPECT_FALSE(is_leaf(child));
    EXPECT_EQ(NumAncestors(child), 0);
    EXPECT_EQ(BirthRank(child), 0);

    const auto &cvalue = child.Value();
    EXPECT_EQ(cvalue.left, 0);
    EXPECT_EQ(cvalue.right, 3);
    EXPECT_EQ(cvalue.name, "gen1");
    ExpectPath(child, "{}");

    EXPECT_EQ(verible::NextSibling(child), nullptr);
    EXPECT_EQ(verible::PreviousSibling(child), nullptr);

    // The invoking node need not be a leaf.
    EXPECT_EQ(NextLeaf(child), nullptr);
    EXPECT_EQ(PreviousLeaf(child), nullptr);

    {
      const auto &grandchild = child.Children().front();
      EXPECT_EQ(grandchild.Parent(), &child);
      EXPECT_EQ(&Root(grandchild), &tree);
      EXPECT_TRUE(is_leaf(grandchild));
      EXPECT_EQ(NumAncestors(grandchild), 1);
      EXPECT_EQ(BirthRank(grandchild), 0);

      const auto &gcvalue = grandchild.Value();
      EXPECT_EQ(gcvalue.left, 0);
      EXPECT_EQ(gcvalue.right, 3);
      EXPECT_EQ(gcvalue.name, "gen2");
      ExpectPath(grandchild, "{0}");

      // As the ancestry chain is linear, Leftmost == Rightmost.
      EXPECT_EQ(&LeftmostDescendant(child), &grandchild);
      EXPECT_EQ(&RightmostDescendant(child), &grandchild);
      EXPECT_EQ(&LeftmostDescendant(tree), &grandchild);
      EXPECT_EQ(&RightmostDescendant(tree), &grandchild);

      EXPECT_EQ(verible::NextSibling(grandchild), nullptr);
      EXPECT_EQ(verible::PreviousSibling(grandchild), nullptr);

      // There is still only a single leaf in a one-child tree,
      // thus next and previous do not exist.
      EXPECT_EQ(NextLeaf(grandchild), nullptr);
      EXPECT_EQ(PreviousLeaf(grandchild), nullptr);
    }
  }
}

TEST(VectorTreeTest, HoistOnlyChildOneChildTreeMiddleAncestor) {
  VectorTreeTestType tree(verible::testing::MakeOneChildPolicyExampleTree());
  EXPECT_TRUE(HoistOnlyChild(tree.Children().front()));
  {
    const auto &value = tree.Value();
    EXPECT_EQ(value.left, 0);
    EXPECT_EQ(value.right, 3);
    EXPECT_EQ(value.name, "root");

    EXPECT_EQ(verible::NextSibling(tree), nullptr);
    EXPECT_EQ(verible::PreviousSibling(tree), nullptr);

    // The invoking node need not be a leaf.
    EXPECT_EQ(NextLeaf(tree), nullptr);
    EXPECT_EQ(PreviousLeaf(tree), nullptr);

    // The "gen1" node is removed, and now the grandchild is directly linked.
    {
      const auto &grandchild = tree.Children().front();
      EXPECT_EQ(grandchild.Parent(), &tree);
      EXPECT_EQ(&Root(grandchild), &tree);
      EXPECT_TRUE(is_leaf(grandchild));
      EXPECT_EQ(NumAncestors(grandchild), 1);
      EXPECT_EQ(BirthRank(grandchild), 0);

      const auto &gcvalue = grandchild.Value();
      EXPECT_EQ(gcvalue.left, 0);
      EXPECT_EQ(gcvalue.right, 3);
      EXPECT_EQ(gcvalue.name, "gen2");
      ExpectPath(grandchild, "{0}");

      // As the ancestry chain is linear, Leftmost == Rightmost.
      EXPECT_EQ(&LeftmostDescendant(tree), &grandchild);
      EXPECT_EQ(&RightmostDescendant(tree), &grandchild);

      EXPECT_EQ(verible::NextSibling(grandchild), nullptr);
      EXPECT_EQ(verible::PreviousSibling(grandchild), nullptr);

      // There is still only a single leaf in a one-child tree,
      // thus next and previous do not exist.
      EXPECT_EQ(NextLeaf(grandchild), nullptr);
      EXPECT_EQ(PreviousLeaf(grandchild), nullptr);
    }
  }
}

TEST(VectorTreeTest, HoistOnlyChildFamilyTree) {
  VectorTreeTestType tree(verible::testing::MakeExampleFamilyTree());
  // No change because each generation has more than one child.
  EXPECT_FALSE(HoistOnlyChild(tree));
}

// Copy-extract tree values from a node's direct children only.
// TODO(fangism): Adapt this into a public method of VectorTree.
template <typename T>
static std::vector<typename T::value_type> NodeValues(const T &node) {
  std::vector<typename T::value_type> result;
  result.reserve(node.Children().size());
  for (const auto &child : node.Children()) {
    result.emplace_back(child.Value());
  }
  return result;
}

TEST(VectorTreeTest, AdoptSubtreesFromEmptyToEmpty) {
  using tree_type = VectorTree<int>;
  tree_type tree1(1), tree2(2);  // no subtrees
  EXPECT_TRUE(is_leaf(tree1));
  EXPECT_TRUE(is_leaf(tree2));

  AdoptSubtreesFrom(tree1, &tree2);
  EXPECT_TRUE(is_leaf(tree1));
  EXPECT_TRUE(is_leaf(tree2));
}

TEST(VectorTreeTest, AdoptSubtreesFromEmptyToNonempty) {
  using tree_type = VectorTree<int>;
  tree_type tree1(1, tree_type(4)), tree2(2);
  EXPECT_THAT(NodeValues(tree1), ElementsAre(4));
  EXPECT_THAT(NodeValues(tree2), ElementsAre());

  AdoptSubtreesFrom(tree1, &tree2);
  EXPECT_THAT(NodeValues(tree1), ElementsAre(4));
  EXPECT_THAT(NodeValues(tree2), ElementsAre());
}

TEST(VectorTreeTest, AdoptSubtreesFromNonemptyToEmpty) {
  using tree_type = VectorTree<int>;
  tree_type tree1(1), tree2(2, tree_type(5));
  EXPECT_THAT(NodeValues(tree1), ElementsAre());
  EXPECT_THAT(NodeValues(tree2), ElementsAre(5));

  AdoptSubtreesFrom(tree1, &tree2);
  EXPECT_THAT(NodeValues(tree1), ElementsAre(5));
  EXPECT_THAT(NodeValues(tree2), ElementsAre());
}

TEST(VectorTreeTest, AdoptSubtreesFromNonemptyToNonempty) {
  using tree_type = VectorTree<int>;
  tree_type tree1(1, tree_type(3), tree_type(6)),
      tree2(2, tree_type(5), tree_type(8));
  EXPECT_THAT(NodeValues(tree1), ElementsAre(3, 6));
  EXPECT_THAT(NodeValues(tree2), ElementsAre(5, 8));

  AdoptSubtreesFrom(tree1, &tree2);
  EXPECT_THAT(NodeValues(tree1), ElementsAre(3, 6, 5, 8));
  EXPECT_THAT(NodeValues(tree2), ElementsAre());
}

TEST(VectorTreeTest, MergeConsecutiveSiblingsTooFewElements) {
  using tree_type = VectorTree<int>;
  tree_type tree(1, tree_type(2));
  auto adder = [](int *left, const int &right) { *left += right; };
  EXPECT_THAT(NodeValues(tree), ElementsAre(2));
  EXPECT_DEATH(MergeConsecutiveSiblings(tree, 0, adder), "");
}

TEST(VectorTreeTest, MergeConsecutiveSiblingsOutOfBounds) {
  using tree_type = VectorTree<int>;
  tree_type tree(1, tree_type(2), tree_type(3));
  auto adder = [](int *left, const int &right) { *left += right; };
  EXPECT_THAT(NodeValues(tree), ElementsAre(2, 3));
  EXPECT_DEATH(MergeConsecutiveSiblings(tree, 1, adder), "");
}

TEST(VectorTreeTest, MergeConsecutiveSiblingsAddLeaves) {
  using tree_type = VectorTree<int>;
  tree_type tree(1, tree_type(2), tree_type(3), tree_type(4), tree_type(5));
  auto adder = [](int *left, const int &right) { *left += right; };
  EXPECT_THAT(NodeValues(tree), ElementsAre(2, 3, 4, 5));

  VLOG(1) << __FUNCTION__ << ": before first merge";

  MergeConsecutiveSiblings(tree, 1, adder);  // combine middle two subtrees
  EXPECT_THAT(NodeValues(tree), ElementsAre(2, 7, 5));
  VLOG(1) << __FUNCTION__ << ": after first merge";

  MergeConsecutiveSiblings(tree, 1, adder);  // combine last two subtrees
  EXPECT_THAT(NodeValues(tree), ElementsAre(2, 12));
  VLOG(1) << __FUNCTION__ << ": after second merge";

  MergeConsecutiveSiblings(tree, 0, adder);  // combine only two subtrees
  EXPECT_THAT(NodeValues(tree), ElementsAre(14));
  VLOG(1) << __FUNCTION__ << ": after third merge";
}

TEST(VectorTreeTest, MergeConsecutiveSiblingsConcatenateSubtreesOnce) {
  using tree_type = VectorTree<int>;
  tree_type tree(1,            //
                 tree_type(2,  //
                           tree_type(6), tree_type(7)),
                 tree_type(3,  //
                           tree_type(8), tree_type(9)),
                 tree_type(4,  //
                           tree_type(10), tree_type(11)),
                 tree_type(5,  //
                           tree_type(12), tree_type(13)));
  auto subtractor = [](int *left, const int &right) { *left -= right; };
  EXPECT_THAT(NodeValues(tree), ElementsAre(2, 3, 4, 5));

  MergeConsecutiveSiblings(tree, 1, subtractor);  // combine middle two subtrees
  EXPECT_THAT(NodeValues(tree), ElementsAre(2, /* 3 - 4 */ -1, 5));
  EXPECT_THAT(NodeValues(tree.Children()[1]), ElementsAre(8, 9, 10, 11));
}

TEST(VectorTreeTest, MergeConsecutiveSiblingsConcatenateSubtrees) {
  using tree_type = VectorTree<int>;
  tree_type tree(1,            //
                 tree_type(2,  //
                           tree_type(6), tree_type(7)),
                 tree_type(3,  //
                           tree_type(8), tree_type(9)),
                 tree_type(4,  //
                           tree_type(10), tree_type(11)),
                 tree_type(5,  //
                           tree_type(12), tree_type(13)));
  auto subtractor = [](int *left, const int &right) { *left -= right; };
  EXPECT_THAT(NodeValues(tree), ElementsAre(2, 3, 4, 5));

  MergeConsecutiveSiblings(tree, 0, subtractor);  // combine first two subtrees
  EXPECT_THAT(NodeValues(tree), ElementsAre(/* 2 - 3 */ -1, 4, 5));
  EXPECT_THAT(NodeValues(tree.Children()[0]), ElementsAre(6, 7, 8, 9));

  MergeConsecutiveSiblings(tree, 1, subtractor);  // combine last two subtrees
  EXPECT_THAT(NodeValues(tree), ElementsAre(-1, /* 4 - 5 */ -1));
  EXPECT_THAT(NodeValues(tree.Children()[1]), ElementsAre(10, 11, 12, 13));

  MergeConsecutiveSiblings(tree, 0, subtractor);  // combine only two subtrees
  EXPECT_THAT(NodeValues(tree), ElementsAre(/* -1 - -1 */ 0));
  EXPECT_THAT(NodeValues(tree.Children()[0]),
              ElementsAre(6, 7, 8, 9, 10, 11, 12, 13));
}

TEST(VectorTreeTest, RemoveSelfFromParentRoot) {
  using tree_type = VectorTree<int>;
  tree_type tree(1);
  EXPECT_DEATH(RemoveSelfFromParent(tree), "");
}

TEST(VectorTreeTest, RemoveSelfFromParentFirstChild) {
  using tree_type = VectorTree<int>;
  tree_type tree(1,             //
                 tree_type(2),  // no grandchildren
                 tree_type(3,   //
                           tree_type(8), tree_type(9)),
                 tree_type(4),  // no grandchildren
                 tree_type(5,   //
                           tree_type(12), tree_type(13)));
  EXPECT_THAT(NodeValues(tree), ElementsAre(2, 3, 4, 5));

  const tree_type expect_tree(1,            //
                                            // tree_type(2),  // deleted
                              tree_type(3,  //
                                        tree_type(8), tree_type(9)),
                              tree_type(4),  // no grandchildren
                              tree_type(5,   //
                                        tree_type(12), tree_type(13)));
  RemoveSelfFromParent(tree.Children().front());
  const auto result_pair = DeepEqual(tree, expect_tree);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(VectorTreeTest, RemoveSelfFromParentMiddleChildWithGrandchildren) {
  using tree_type = VectorTree<int>;
  tree_type tree(1,             //
                 tree_type(2),  // no grandchildren
                 tree_type(3,   //
                           tree_type(8), tree_type(9)),
                 tree_type(4),  // no grandchildren
                 tree_type(5,   //
                           tree_type(12), tree_type(13)));
  EXPECT_THAT(NodeValues(tree), ElementsAre(2, 3, 4, 5));

  const tree_type expect_tree(
      1,             //
      tree_type(2),  //
      // tree_type(3, tree_type(8), tree_type(9)),  // deleted
      tree_type(4),  // no grandchildren
      tree_type(5,   //
                tree_type(12), tree_type(13)));
  RemoveSelfFromParent(tree.Children()[1]);
  const auto result_pair = DeepEqual(tree, expect_tree);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(VectorTreeTest, RemoveSelfFromParentMiddleChildWithoutGrandchildren) {
  using tree_type = VectorTree<int>;
  tree_type tree(1,             //
                 tree_type(2),  // no grandchildren
                 tree_type(3,   //
                           tree_type(8), tree_type(9)),
                 tree_type(4),  // no grandchildren
                 tree_type(5,   //
                           tree_type(12), tree_type(13)));
  EXPECT_THAT(NodeValues(tree), ElementsAre(2, 3, 4, 5));

  const tree_type expect_tree(1,             //
                              tree_type(2),  //
                              tree_type(3,   //
                                        tree_type(8), tree_type(9)),
                              // tree_type(4),  // deleted
                              tree_type(5,  //
                                        tree_type(12), tree_type(13)));
  RemoveSelfFromParent(tree.Children()[2]);
  const auto result_pair = DeepEqual(tree, expect_tree);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(VectorTreeTest, RemoveSelfFromParentLastChild) {
  using tree_type = VectorTree<int>;
  tree_type tree(1,             //
                 tree_type(2),  // no grandchildren
                 tree_type(3,   //
                           tree_type(8), tree_type(9)),
                 tree_type(4),  // no grandchildren
                 tree_type(5,   //
                           tree_type(12), tree_type(13)));
  EXPECT_THAT(NodeValues(tree), ElementsAre(2, 3, 4, 5));

  const tree_type expect_tree(
      1,             //
      tree_type(2),  //
      tree_type(3,   //
                tree_type(8), tree_type(9)),
      tree_type(4)  // no grandchildren
                    // tree_type(5, tree_type(12), tree_type(13))  // deleted
  );
  RemoveSelfFromParent(tree.Children().back());
  const auto result_pair = DeepEqual(tree, expect_tree);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(VectorTreeTest, FlattenOnceNoChildren) {
  using tree_type = VectorTree<int>;
  tree_type tree(1);
  EXPECT_THAT(NodeValues(tree), ElementsAre());

  const tree_type expect_tree(1);
  FlattenOnce(tree);
  const auto result_pair = DeepEqual(tree, expect_tree);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(VectorTreeTest, FlattenOnceNoGrandchildren) {
  using tree_type = VectorTree<int>;
  tree_type tree(1,  // no grandchildren
                 tree_type(2), tree_type(3), tree_type(4), tree_type(5));
  EXPECT_THAT(NodeValues(tree), ElementsAre(2, 3, 4, 5));

  const tree_type expect_tree(1);
  FlattenOnce(tree);
  const auto result_pair = DeepEqual(tree, expect_tree);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(VectorTreeTest, FlattenOnceOneGrandchild) {
  using tree_type = VectorTree<int>;
  tree_type tree(1, tree_type(2, tree_type(3)));
  EXPECT_THAT(NodeValues(tree), ElementsAre(2));

  const tree_type expect_tree(1, tree_type(3));
  FlattenOnce(tree);
  const auto result_pair = DeepEqual(tree, expect_tree);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(VectorTreeTest, FlattenOnceMixed) {
  using tree_type = VectorTree<int>;
  tree_type tree(1,             //
                 tree_type(2),  // no grandchildren
                 tree_type(3,   //
                           tree_type(8), tree_type(9)),
                 tree_type(4),  // no grandchildren
                 tree_type(5,   //
                           tree_type(12), tree_type(13)));
  EXPECT_THAT(NodeValues(tree), ElementsAre(2, 3, 4, 5));

  const tree_type expect_tree(1,  //
                              tree_type(8), tree_type(9), tree_type(12),
                              tree_type(13));
  FlattenOnce(tree);
  const auto result_pair = DeepEqual(tree, expect_tree);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(VectorTreeTest, FlattenOnceAllNonempty) {
  using tree_type = VectorTree<int>;
  tree_type tree(1,            //
                 tree_type(2,  //
                           tree_type(6), tree_type(7)),
                 tree_type(3,  //
                           tree_type(8), tree_type(9)),
                 tree_type(4,  //
                           tree_type(10), tree_type(11)),
                 tree_type(5,  //
                           tree_type(12), tree_type(13)));
  EXPECT_THAT(NodeValues(tree), ElementsAre(2, 3, 4, 5));

  const tree_type expect_tree(1,  //
                              tree_type(6), tree_type(7), tree_type(8),
                              tree_type(9), tree_type(10), tree_type(11),
                              tree_type(12), tree_type(13));
  FlattenOnce(tree);
  const auto result_pair = DeepEqual(tree, expect_tree);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(VectorTreeTest, FlattenOnceGreatgrandchildren) {
  using tree_type = VectorTree<int>;
  tree_type tree(1,                      //
                 tree_type(2,            //
                           tree_type(6,  //
                                     tree_type(7))),
                 tree_type(3,            //
                           tree_type(8,  //
                                     tree_type(9))),
                 tree_type(4,             //
                           tree_type(10,  //
                                     tree_type(11))),
                 tree_type(5,             //
                           tree_type(12,  //
                                     tree_type(13))));
  EXPECT_THAT(NodeValues(tree), ElementsAre(2, 3, 4, 5));

  const tree_type expect_tree(1,            //
                              tree_type(6,  //
                                        tree_type(7)),
                              tree_type(8,  //
                                        tree_type(9)),
                              tree_type(10,  //
                                        tree_type(11)),
                              tree_type(12,  //
                                        tree_type(13)));
  FlattenOnce(tree);
  const auto result_pair = DeepEqual(tree, expect_tree);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(VectorTreeTest, FlattenOnlyChildrenWithChildrenNoChildren) {
  using tree_type = VectorTree<int>;
  tree_type tree(1);
  EXPECT_THAT(NodeValues(tree), ElementsAre());

  const tree_type expect_tree(1);
  std::vector<size_t> new_offsets;
  FlattenOnlyChildrenWithChildren(tree, &new_offsets);
  const auto result_pair = DeepEqual(tree, expect_tree);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
  EXPECT_TRUE(new_offsets.empty());
}

TEST(VectorTreeTest, FlattenOnlyChildrenWithChildrenNoChildrenNoOffsets) {
  using tree_type = VectorTree<int>;
  tree_type tree(1);
  EXPECT_THAT(NodeValues(tree), ElementsAre());

  const tree_type expect_tree(1);
  FlattenOnlyChildrenWithChildren(tree /* no offsets */);
  const auto result_pair = DeepEqual(tree, expect_tree);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(VectorTreeTest, FlattenOnlyChildrenWithChildrenNoGrandchildren) {
  using tree_type = VectorTree<int>;
  tree_type tree(1,  // no grandchildren
                 tree_type(2), tree_type(3), tree_type(4), tree_type(5));
  EXPECT_THAT(NodeValues(tree), ElementsAre(2, 3, 4, 5));

  const tree_type expect_tree(1,  // all children preserved
                              tree_type(2), tree_type(3), tree_type(4),
                              tree_type(5));
  std::vector<size_t> new_offsets;
  FlattenOnlyChildrenWithChildren(tree, &new_offsets);
  const auto result_pair = DeepEqual(tree, expect_tree);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
  EXPECT_THAT(new_offsets, ElementsAre(0, 1, 2, 3));
}

TEST(VectorTreeTest, FlattenOnlyChildrenWithChildrenOneGrandchild) {
  using tree_type = VectorTree<int>;
  tree_type tree(1, tree_type(2, tree_type(3)));
  EXPECT_THAT(NodeValues(tree), ElementsAre(2));

  const tree_type expect_tree(1, tree_type(3));
  std::vector<size_t> new_offsets;
  FlattenOnlyChildrenWithChildren(tree, &new_offsets);
  const auto result_pair = DeepEqual(tree, expect_tree);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
  EXPECT_THAT(new_offsets, ElementsAre(0));
}

TEST(VectorTreeTest, FlattenOnlyChildrenWithChildrenOneGrandchildNoOffsets) {
  using tree_type = VectorTree<int>;
  tree_type tree(1, tree_type(2, tree_type(3)));
  EXPECT_THAT(NodeValues(tree), ElementsAre(2));

  const tree_type expect_tree(1, tree_type(3));
  FlattenOnlyChildrenWithChildren(tree /* no offsets */);
  const auto result_pair = DeepEqual(tree, expect_tree);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(VectorTreeTest, FlattenOnlyChildrenWithChildrenTwoGrandchildren) {
  using tree_type = VectorTree<int>;
  tree_type tree(1, tree_type(2, tree_type(3), tree_type(7)));
  EXPECT_THAT(NodeValues(tree), ElementsAre(2));

  const tree_type expect_tree(1, tree_type(3), tree_type(7));
  std::vector<size_t> new_offsets;
  FlattenOnlyChildrenWithChildren(tree, &new_offsets);
  const auto result_pair = DeepEqual(tree, expect_tree);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
  EXPECT_THAT(new_offsets, ElementsAre(0));
}

TEST(VectorTreeTest, FlattenOnlyChildrenWithChildrenMixed) {
  using tree_type = VectorTree<int>;
  tree_type tree(1,             //
                 tree_type(2),  // no grandchildren
                 tree_type(3,   //
                           tree_type(8), tree_type(9)),
                 tree_type(4),  // no grandchildren
                 tree_type(5,   //
                           tree_type(12), tree_type(13)));
  EXPECT_THAT(NodeValues(tree), ElementsAre(2, 3, 4, 5));

  const tree_type expect_tree(1,                           //
                              tree_type(2),                //
                              tree_type(8), tree_type(9),  //
                              tree_type(4),                //
                              tree_type(12), tree_type(13));
  std::vector<size_t> new_offsets;
  FlattenOnlyChildrenWithChildren(tree, &new_offsets);
  const auto result_pair = DeepEqual(tree, expect_tree);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
  EXPECT_THAT(new_offsets, ElementsAre(0, 1, 3, 4));
}

TEST(VectorTreeTest, FlattenOnlyChildrenWithChildrenAllNonempty) {
  using tree_type = VectorTree<int>;
  tree_type tree(1,            //
                 tree_type(2,  //
                           tree_type(6), tree_type(7)),
                 tree_type(3,  //
                           tree_type(8), tree_type(9)),
                 tree_type(4,  //
                           tree_type(10), tree_type(11)),
                 tree_type(5,  //
                           tree_type(12), tree_type(13)));
  EXPECT_THAT(NodeValues(tree), ElementsAre(2, 3, 4, 5));

  const tree_type expect_tree(1,  //
                              tree_type(6), tree_type(7), tree_type(8),
                              tree_type(9), tree_type(10), tree_type(11),
                              tree_type(12), tree_type(13));
  std::vector<size_t> new_offsets;
  FlattenOnlyChildrenWithChildren(tree, &new_offsets);
  const auto result_pair = DeepEqual(tree, expect_tree);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
  EXPECT_THAT(new_offsets, ElementsAre(0, 2, 4, 6));
}

TEST(VectorTreeTest, FlattenOnlyChildrenWithChildrenGreatgrandchildren) {
  using tree_type = VectorTree<int>;
  tree_type tree(1,                      //
                 tree_type(2,            //
                           tree_type(6,  //
                                     tree_type(7))),
                 tree_type(3,            //
                           tree_type(8,  //
                                     tree_type(9))),
                 tree_type(4,             //
                           tree_type(10,  //
                                     tree_type(11))),
                 tree_type(5,             //
                           tree_type(12,  //
                                     tree_type(13))));
  EXPECT_THAT(NodeValues(tree), ElementsAre(2, 3, 4, 5));

  const tree_type expect_tree(1,            //
                              tree_type(6,  //
                                        tree_type(7)),
                              tree_type(8,  //
                                        tree_type(9)),
                              tree_type(10,  //
                                        tree_type(11)),
                              tree_type(12,  //
                                        tree_type(13)));
  std::vector<size_t> new_offsets;
  FlattenOnlyChildrenWithChildren(tree, &new_offsets);
  const auto result_pair = DeepEqual(tree, expect_tree);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
  EXPECT_THAT(new_offsets, ElementsAre(0, 1, 2, 3));
}

TEST(VectorTreeTest, FlattenOneChildEmpty) {
  using tree_type = VectorTree<int>;
  tree_type tree(4);  // no children
  EXPECT_DEATH(FlattenOneChild(tree, 0), "");
}

TEST(VectorTreeTest, FlattenOneChildOnlyChildNoGrandchildren) {
  using tree_type = VectorTree<int>;
  tree_type tree(4, tree_type(2));  // no grandchildren
  const tree_type expect_tree(4);
  FlattenOneChild(tree, 0);
  const auto result_pair = DeepEqual(tree, expect_tree);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(VectorTreeTest, FlattenOneChildOnlyChildOneGrandchild) {
  using tree_type = VectorTree<int>;
  tree_type tree(4, tree_type(2, tree_type(11)));  // with grandchild
  const tree_type expect_tree(4, tree_type(11));
  FlattenOneChild(tree, 0);
  const auto result_pair = DeepEqual(tree, expect_tree);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(VectorTreeTest, FlattenOneChildFirstChildInFamilyTree) {
  using tree_type = VectorTree<int>;
  tree_type tree(1,            //
                 tree_type(2,  //
                           tree_type(6), tree_type(7)),
                 tree_type(3,  //
                           tree_type(8), tree_type(9)),
                 tree_type(4,  //
                           tree_type(10), tree_type(11)),
                 tree_type(5,  //
                           tree_type(12), tree_type(13)));
  const tree_type expect_tree(1,             //
                              tree_type(6),  //
                              tree_type(7),  //
                              tree_type(3,   //
                                        tree_type(8), tree_type(9)),
                              tree_type(4,  //
                                        tree_type(10), tree_type(11)),
                              tree_type(5,  //
                                        tree_type(12), tree_type(13)));
  FlattenOneChild(tree, 0);
  const auto result_pair = DeepEqual(tree, expect_tree);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(VectorTreeTest, FlattenOneChildMiddleChildInFamilyTree) {
  using tree_type = VectorTree<int>;
  tree_type tree(1,            //
                 tree_type(2,  //
                           tree_type(6), tree_type(7)),
                 tree_type(3,  //
                           tree_type(8), tree_type(9)),
                 tree_type(4,  //
                           tree_type(10), tree_type(11)),
                 tree_type(5,  //
                           tree_type(12), tree_type(13)));
  const tree_type expect_tree(1,            //
                              tree_type(2,  //
                                        tree_type(6), tree_type(7)),
                              tree_type(8),  //
                              tree_type(9),  //
                              tree_type(4,   //
                                        tree_type(10), tree_type(11)),
                              tree_type(5,  //
                                        tree_type(12), tree_type(13)));
  FlattenOneChild(tree, 1);
  const auto result_pair = DeepEqual(tree, expect_tree);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(VectorTreeTest, FlattenOneChildLastChildInFamilyTree) {
  using tree_type = VectorTree<int>;
  tree_type tree(1,            //
                 tree_type(2,  //
                           tree_type(6), tree_type(7)),
                 tree_type(3,  //
                           tree_type(8), tree_type(9)),
                 tree_type(4,  //
                           tree_type(10), tree_type(11)),
                 tree_type(5,  //
                           tree_type(12), tree_type(13)));
  const tree_type expect_tree(1,            //
                              tree_type(2,  //
                                        tree_type(6), tree_type(7)),
                              tree_type(3,  //
                                        tree_type(8), tree_type(9)),
                              tree_type(4,  //
                                        tree_type(10), tree_type(11)),
                              tree_type(12),  //
                              tree_type(13));
  FlattenOneChild(tree, 3);
  const auto result_pair = DeepEqual(tree, expect_tree);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(VectorTreeTest, PrintTree) {
  const auto tree = verible::testing::MakeExampleFamilyTree();
  std::ostringstream stream;
  stream << tree;
  EXPECT_EQ(stream.str(), R"({ ((0, 4, grandparent))
  { ((0, 2, parent1))
    { ((0, 1, child1)) }
    { ((1, 2, child2)) }
  }
  { ((2, 4, parent2))
    { ((2, 3, child3)) }
    { ((3, 4, child4)) }
  }
})");
}

TEST(VectorTreeTest, PrintTreeCustom) {
  const auto tree = verible::testing::MakeExampleFamilyTree();
  std::ostringstream stream;
  PrintTree(tree, &stream,
            [](std::ostream &s,
               const decltype(tree)::value_type &value) -> std::ostream & {
              return s << value.name;  // print only the name field
            });
  EXPECT_EQ(stream.str(), R"({ (grandparent)
  { (parent1)
    { (child1) }
    { (child2) }
  }
  { (parent2)
    { (child3) }
    { (child4) }
  }
})");
}

TEST(VectorTreeTest, ChildrenManipulation) {
  VectorTreeTestType tree(verible::testing::MakeExampleFamilyTree());

  auto &children_gp = tree.Children();

  children_gp.push_back(VectorTreeTestType(NamedInterval(4, 6, "parent3")));

  EXPECT_EQ(tree.Children().size(), 3);
  EXPECT_EQ(tree.Children().back().Value(), NamedInterval(4, 6, "parent3"));
  EXPECT_EQ(tree.Children().back().Parent(), &tree);

  auto &p3 = tree.Children().back();
  auto &children_p3 = p3.Children();

  children_p3.push_back(VectorTreeTestType(NamedInterval(4, 5, "child5")));
  children_p3.emplace_back(NamedInterval(5, 6, "child6"));

  EXPECT_EQ(p3.Children().size(), 2);

  EXPECT_EQ(p3.Children().at(0).Value(), NamedInterval(4, 5, "child5"));
  EXPECT_EQ(p3.Children().at(0).Parent(), &p3);

  EXPECT_EQ(p3.Children().at(1).Value(), NamedInterval(5, 6, "child6"));
  EXPECT_EQ(p3.Children().at(1).Parent(), &p3);

  auto &p2 = tree.Children().at(1);

  const NamedInterval original_p2_children[] = {
      p2.Children().at(0).Value(),
      p2.Children().at(1).Value(),
  };

  children_p3.insert(children_p3.begin(), p2.Children().begin(),
                     p2.Children().end());

  EXPECT_EQ(p3.Children().size(), 4);

  EXPECT_EQ(p3.Children().at(0).Value(), original_p2_children[0]);
  EXPECT_EQ(p3.Children().at(0).Parent(), &p3);

  EXPECT_EQ(p3.Children().at(1).Value(), original_p2_children[1]);
  EXPECT_EQ(p3.Children().at(1).Parent(), &p3);

  EXPECT_EQ(p3.Children().at(2).Value(), NamedInterval(4, 5, "child5"));
  EXPECT_EQ(p3.Children().at(2).Parent(), &p3);

  EXPECT_EQ(p3.Children().at(3).Value(), NamedInterval(5, 6, "child6"));
  EXPECT_EQ(p3.Children().at(3).Parent(), &p3);

  EXPECT_EQ(p2.Children().size(), 2);

  EXPECT_EQ(p2.Children().at(0).Value(), original_p2_children[0]);
  EXPECT_EQ(p2.Children().at(0).Parent(), &p2);

  EXPECT_EQ(p2.Children().at(1).Value(), original_p2_children[1]);
  EXPECT_EQ(p2.Children().at(1).Parent(), &p2);

  p2.Children().clear();

  EXPECT_EQ(p2.Children().size(), 0);

  p2.Children().assign({
      VectorTreeTestType(NamedInterval(0, 1, "foo")),
      VectorTreeTestType(NamedInterval(1, 2, "bar")),
      VectorTreeTestType(NamedInterval(2, 3, "baz")),
  });

  EXPECT_EQ(p2.Children().size(), 3);

  EXPECT_EQ(p2.Children().at(0).Value(), NamedInterval(0, 1, "foo"));
  EXPECT_EQ(p2.Children().at(0).Parent(), &p2);

  EXPECT_EQ(p2.Children().at(1).Value(), NamedInterval(1, 2, "bar"));
  EXPECT_EQ(p2.Children().at(1).Parent(), &p2);

  EXPECT_EQ(p2.Children().at(2).Value(), NamedInterval(2, 3, "baz"));
  EXPECT_EQ(p2.Children().at(2).Parent(), &p2);

  tree.Children().clear();
  EXPECT_TRUE(tree.Children().empty());
}

}  // namespace
}  // namespace verible
