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

#include "common/util/vector_tree.h"

#include <cstddef>
#include <iterator>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "common/util/vector_tree_test_util.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace verible {
namespace {

using ::testing::ElementsAre;
using verible::testing::MakePath;
using verible::testing::NamedInterval;
using verible::testing::VectorTreeTestType;

template <class Tree>
void ExpectPath(const Tree& tree, absl::string_view expect) {
  std::ostringstream stream;
  stream << NodePath(tree);
  EXPECT_EQ(stream.str(), expect);
}

// Test that basic Tree construction works on a singleton node.
TEST(VectorTreeTest, RootOnly) {
  const VectorTreeTestType tree(verible::testing::MakeRootOnlyExampleTree());
  EXPECT_TRUE(tree.is_leaf());
  EXPECT_EQ(tree.Parent(), nullptr);
  EXPECT_EQ(tree.NumAncestors(), 0);
  EXPECT_EQ(tree.BirthRank(), 0);  // no parent
  EXPECT_TRUE(tree.IsFirstChild());
  EXPECT_TRUE(tree.IsLastChild());
  EXPECT_EQ(tree.Root(), &tree);

  const auto& value = tree.Value();
  EXPECT_EQ(value.left, 0);
  EXPECT_EQ(value.right, 2);
  EXPECT_EQ(value.name, "root");

  ExpectPath(tree, "{}");
}

TEST(VectorTreeTest, RootOnlyDescendants) {
  VectorTreeTestType tree(verible::testing::MakeRootOnlyExampleTree());
  EXPECT_EQ(tree.LeftmostDescendant(), &tree);
  EXPECT_EQ(tree.RightmostDescendant(), &tree);
  {  // Test const method variants.
    const auto& ctree(tree);
    EXPECT_EQ(ctree.LeftmostDescendant(), &ctree);
    EXPECT_EQ(ctree.RightmostDescendant(), &ctree);
  }
}

TEST(VectorTreeTest, RootOnlyHasAncestor) {
  VectorTreeTestType tree(verible::testing::MakeRootOnlyExampleTree());
  EXPECT_FALSE(tree.HasAncestor(nullptr));
  EXPECT_FALSE(tree.HasAncestor(&tree));

  // Separate tree
  VectorTreeTestType tree2(verible::testing::MakeRootOnlyExampleTree());
  EXPECT_FALSE(tree2.HasAncestor(&tree));
  EXPECT_FALSE(tree.HasAncestor(&tree2));
}

TEST(VectorTreeTest, RootOnlyLeafIteration) {
  VectorTreeTestType tree(verible::testing::MakeRootOnlyExampleTree());
  EXPECT_EQ(tree.NextLeaf(), nullptr);
  EXPECT_EQ(tree.PreviousLeaf(), nullptr);
  {  // const method variants
    const auto& ctree(tree);
    EXPECT_EQ(ctree.NextLeaf(), nullptr);
    EXPECT_EQ(ctree.PreviousLeaf(), nullptr);
  }
}

TEST(VectorTreeTest, RootOnlySiblingIteration) {
  VectorTreeTestType tree(verible::testing::MakeRootOnlyExampleTree());
  EXPECT_EQ(tree.NextSibling(), nullptr);
  EXPECT_EQ(tree.PreviousSibling(), nullptr);
  {  // const method variants
    const auto& ctree(tree);
    EXPECT_EQ(ctree.NextSibling(), nullptr);
    EXPECT_EQ(ctree.PreviousSibling(), nullptr);
  }
}

TEST(VectorTreeTest, CopyAssignEmpty) {
  typedef VectorTree<int> tree_type;
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
  typedef VectorTree<int> tree_type;
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
  typedef VectorTree<int> tree_type;
  const tree_type tree(1);  // Root only tree.
  const tree_type expected(1);
  tree_type tree2 = tree;
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
  typedef VectorTree<int> tree_type;
  const tree_type tree(1,
                       tree_type(2, tree_type(3, tree_type(4, tree_type(5)))));
  const tree_type expected(
      1, tree_type(2, tree_type(3, tree_type(4, tree_type(5)))));
  tree_type tree2 = tree;
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
  typedef VectorTree<int> tree_type;
  tree_type tree(1);  // Root only tree.
  const tree_type expected(1);
  tree_type tree2 = std::move(tree);
  const auto result_pair = DeepEqual(tree2, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(VectorTreeTest, MoveInitializeDeep) {
  typedef VectorTree<int> tree_type;
  tree_type tree(1, tree_type(2, tree_type(3, tree_type(4, tree_type(5)))));
  const tree_type expected(
      1, tree_type(2, tree_type(3, tree_type(4, tree_type(5)))));
  tree_type tree2 = std::move(tree);
  const auto result_pair = DeepEqual(tree2, expected);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(VectorTreeTest, MoveAssignEmpty) {
  typedef VectorTree<int> tree_type;
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
  typedef VectorTree<int> tree_type;
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
  typedef VectorTree<int> tree_type;
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
  typedef VectorTree<int> tree_type;
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
  typedef VectorTree<int> tree_type;
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
  typedef VectorTree<int> tree_type;
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
  absl::string_view name;

  explicit NameOnly(const NamedInterval& v) : name(v.name) {}
};

std::ostream& operator<<(std::ostream& stream, const NameOnly& n) {
  return stream << '(' << n.name << ")\n";
}

static NameOnly NameOnlyConverter(const VectorTreeTestType& node) {
  return NameOnly(node.Value());
}

// Heterogeneous comparison.
bool operator==(const NamedInterval& left, const NameOnly& right) {
  return left.name == right.name;
}

TEST(VectorTreeTest, RootOnlyTreeTransformConstruction) {
  const VectorTreeTestType tree(verible::testing::MakeRootOnlyExampleTree());
  const auto other_tree = tree.Transform<NameOnly>(NameOnlyConverter);
  EXPECT_TRUE(other_tree.is_leaf());
  EXPECT_EQ(other_tree.Parent(), nullptr);
  EXPECT_EQ(other_tree.NumAncestors(), 0);
  EXPECT_EQ(other_tree.BirthRank(), 0);  // no parent

  const auto& value = other_tree.Value();
  EXPECT_EQ(value.name, "root");
}

TEST(VectorTreeTest, RootOnlyTreeTransformComparisonMatches) {
  const VectorTreeTestType tree(verible::testing::MakeRootOnlyExampleTree());
  const auto other_tree = tree.Transform<NameOnly>(NameOnlyConverter);
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
  auto other_tree = tree.Transform<NameOnly>(NameOnlyConverter);
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
    auto* child = tree.NewChild(NamedInterval(1, 2, "child"));
    EXPECT_EQ(child->Parent(), &tree);
    EXPECT_EQ(child->Root(), &tree);
    EXPECT_TRUE(child->is_leaf());

    const auto& value(child->Value());
    EXPECT_EQ(value.left, 1);
    EXPECT_EQ(value.right, 2);
    EXPECT_EQ(value.name, "child");

    ExpectPath(*child, "{0}");
  }
  {
    auto* child = tree.NewChild(NamedInterval(2, 3, "lil-bro"));
    EXPECT_EQ(child->Parent(), &tree);
    EXPECT_EQ(child->Root(), &tree);
    EXPECT_TRUE(child->is_leaf());

    const auto& value(child->Value());
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
    auto* first_child = tree.NewChild(NamedInterval(1, 2, "child"));
    ExpectPath(*first_child, "{0}");

    auto* second_child =
        first_child->NewSibling(NamedInterval(2, 3, "lil-bro"));
    // Recall that NewSibling() may invalidate reference to first_child.
    EXPECT_EQ(second_child->Parent(), &tree);
    EXPECT_EQ(second_child->Root(), &tree);
    EXPECT_TRUE(second_child->is_leaf());

    const auto& value(second_child->Value());
    EXPECT_EQ(value.left, 2);
    EXPECT_EQ(value.right, 3);
    EXPECT_EQ(value.name, "lil-bro");

    ExpectPath(*second_child, "{1}");
  }
}

TEST(VectorTreeTest, OneChildPolicy) {
  const auto tree = verible::testing::MakeOneChildPolicyExampleTree();
  EXPECT_EQ(tree.Parent(), nullptr);
  EXPECT_FALSE(tree.is_leaf());

  const auto& value = tree.Value();
  EXPECT_EQ(value.left, 0);
  EXPECT_EQ(value.right, 3);
  EXPECT_EQ(value.name, "root");

  {
    const auto& child = tree.Children().front();
    EXPECT_EQ(child.Parent(), &tree);
    EXPECT_EQ(child.Root(), &tree);
    EXPECT_FALSE(child.is_leaf());
    EXPECT_EQ(child.NumAncestors(), 1);
    EXPECT_EQ(child.BirthRank(), 0);
    EXPECT_TRUE(child.IsFirstChild());
    EXPECT_TRUE(child.IsLastChild());

    const auto& cvalue = child.Value();
    EXPECT_EQ(cvalue.left, 0);
    EXPECT_EQ(cvalue.right, 3);
    EXPECT_EQ(cvalue.name, "gen1");
    ExpectPath(child, "{0}");

    EXPECT_EQ(child.NextSibling(), nullptr);
    EXPECT_EQ(child.PreviousSibling(), nullptr);

    // The invoking node need not be a leaf.
    EXPECT_EQ(child.NextLeaf(), nullptr);
    EXPECT_EQ(child.PreviousLeaf(), nullptr);

    {
      const auto& grandchild = child.Children().front();
      EXPECT_EQ(grandchild.Parent(), &child);
      EXPECT_EQ(grandchild.Root(), &tree);
      EXPECT_TRUE(grandchild.is_leaf());
      EXPECT_EQ(grandchild.NumAncestors(), 2);
      EXPECT_EQ(grandchild.BirthRank(), 0);
      EXPECT_TRUE(grandchild.IsFirstChild());
      EXPECT_TRUE(grandchild.IsLastChild());

      const auto& gcvalue = grandchild.Value();
      EXPECT_EQ(gcvalue.left, 0);
      EXPECT_EQ(gcvalue.right, 3);
      EXPECT_EQ(gcvalue.name, "gen2");
      ExpectPath(grandchild, "{0,0}");

      // As the ancestry chain is linear, Leftmost == Rightmost.
      EXPECT_EQ(child.LeftmostDescendant(), &grandchild);
      EXPECT_EQ(child.RightmostDescendant(), &grandchild);
      EXPECT_EQ(tree.LeftmostDescendant(), &grandchild);
      EXPECT_EQ(tree.RightmostDescendant(), &grandchild);

      EXPECT_EQ(grandchild.NextSibling(), nullptr);
      EXPECT_EQ(grandchild.PreviousSibling(), nullptr);

      // There is still only a single leaf in a one-child tree,
      // thus next and previous do not exist.
      EXPECT_EQ(grandchild.NextLeaf(), nullptr);
      EXPECT_EQ(grandchild.PreviousLeaf(), nullptr);
    }
  }
}

TEST(VectorTreeTest, OneChildPolicyHasAncestor) {
  const auto tree = verible::testing::MakeOneChildPolicyExampleTree();

  {
    const auto& child = tree.Children().front();

    EXPECT_FALSE(tree.HasAncestor(&child));
    EXPECT_TRUE(child.HasAncestor(&tree));

    {
      const auto& grandchild = child.Children().front();

      EXPECT_FALSE(child.HasAncestor(&grandchild));
      EXPECT_TRUE(grandchild.HasAncestor(&child));

      EXPECT_FALSE(tree.HasAncestor(&grandchild));
      EXPECT_TRUE(grandchild.HasAncestor(&tree));
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
  auto& lchild = ltree.Children()[0];
  auto& rchild = rtree.Children()[0];
  lchild.Value().right = 32;
  rchild.Value().right = 77;
  const auto result_pair = DeepEqual(ltree, rtree);
  EXPECT_EQ(result_pair.left, &lchild);
  EXPECT_EQ(result_pair.right, &rchild);
}

TEST(VectorTreeTest, DeepEqualOneChildDifferentGrandchildValues) {
  VectorTreeTestType ltree(verible::testing::MakeOneChildPolicyExampleTree());
  VectorTreeTestType rtree(verible::testing::MakeOneChildPolicyExampleTree());
  auto& lchild = ltree.Children()[0].Children()[0];  // only grandchild
  auto& rchild = rtree.Children()[0].Children()[0];  // only grandchild
  lchild.Value().right = 32;
  rchild.Value().right = 77;
  const auto result_pair = DeepEqual(ltree, rtree);
  EXPECT_EQ(result_pair.left, &lchild);
  EXPECT_EQ(result_pair.right, &rchild);
}

TEST(VectorTreeTest, DeepEqualOneChildGrandchildValuesHeterogeneous) {
  VectorTreeTestType ltree(verible::testing::MakeOneChildPolicyExampleTree());
  auto rtree = ltree.Transform<NameOnly>(NameOnlyConverter);
  {  // Match
    const auto result_pair = DeepEqual(ltree, rtree);
    EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
    EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
  }
  {                                          // Mismatch
    const std::vector<size_t> path({0, 0});  // only grandchild
    auto& lchild = ltree.DescendPath(path.begin(), path.end());
    auto& rchild = rtree.DescendPath(path.begin(), path.end());
    lchild.Value().name = "alex";
    rchild.Value().name = "james";
    const auto result_pair = DeepEqual(ltree, rtree);
    EXPECT_EQ(result_pair.left, &lchild);
    EXPECT_EQ(result_pair.right, &rchild);
  }
}

template <typename T>
void VerifyFamilyTree(const VectorTree<T>& tree) {
  EXPECT_EQ(tree.Parent(), nullptr);
  EXPECT_EQ(tree.Root(), &tree);
  EXPECT_FALSE(tree.is_leaf());
  EXPECT_EQ(tree.NumAncestors(), 0);
  EXPECT_EQ(tree.BirthRank(), 0);

  const auto tree_path = MakePath(tree);
  EXPECT_TRUE(tree_path.empty());
  EXPECT_EQ(&tree.DescendPath(tree_path.begin(), tree_path.end()), &tree);

  for (int i = 0; i < 2; ++i) {
    const auto& child = tree.Children()[i];
    EXPECT_EQ(child.Parent(), &tree);
    EXPECT_EQ(child.Root(), &tree);
    EXPECT_FALSE(child.is_leaf());
    EXPECT_EQ(child.NumAncestors(), 1);
    EXPECT_EQ(child.BirthRank(), i);
    EXPECT_EQ(child.IsFirstChild(), i == 0);
    EXPECT_EQ(child.IsLastChild(), i == 1);

    const auto child_path = MakePath(child);
    EXPECT_THAT(child_path, ElementsAre(i));
    EXPECT_EQ(&tree.DescendPath(child_path.begin(), child_path.end()), &child);

    for (int j = 0; j < 2; ++j) {
      const auto& grandchild = child.Children()[j];
      EXPECT_EQ(grandchild.Parent(), &child);
      EXPECT_EQ(grandchild.Root(), &tree);
      EXPECT_TRUE(grandchild.is_leaf());
      EXPECT_EQ(grandchild.NumAncestors(), 2);
      EXPECT_EQ(grandchild.BirthRank(), j);
      EXPECT_EQ(grandchild.IsFirstChild(), j == 0);
      EXPECT_EQ(grandchild.IsLastChild(), j == 1);

      const auto grandchild_path = MakePath(grandchild);
      EXPECT_THAT(grandchild_path, ElementsAre(i, j));
      const auto begin = grandchild_path.begin(), end = grandchild_path.end();
      EXPECT_EQ(&tree.DescendPath(begin, end), &grandchild);
      EXPECT_EQ(&child.DescendPath(begin + 1, end), &grandchild);
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
  const auto tree(orig_tree);  // copied
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
    EXPECT_EQ(tree.LeftmostDescendant(),
              &tree.DescendPath(left_path.begin(), left_path.end()));
    EXPECT_EQ(tree.RightmostDescendant(),
              &tree.DescendPath(right_path.begin(), right_path.end()));
  }
  {  // Test const method variants.
    const auto& ctree(tree);
    EXPECT_EQ(ctree.LeftmostDescendant(),
              &ctree.DescendPath(left_path.begin(), left_path.end()));
    EXPECT_EQ(ctree.RightmostDescendant(),
              &ctree.DescendPath(right_path.begin(), right_path.end()));
  }
}

TEST(VectorTreeTest, FamilyTreeHasAncestor) {
  const auto tree = verible::testing::MakeExampleFamilyTree();
  const auto& child_0 = tree.Children()[0];
  const auto& child_1 = tree.Children()[1];
  const auto& grandchild_00 = child_0.Children()[0];
  const auto& grandchild_01 = child_0.Children()[1];
  const auto& grandchild_10 = child_1.Children()[0];
  const auto& grandchild_11 = child_1.Children()[1];

  EXPECT_FALSE(tree.HasAncestor(&child_0));
  EXPECT_FALSE(tree.HasAncestor(&child_1));
  EXPECT_FALSE(tree.HasAncestor(&grandchild_00));
  EXPECT_FALSE(tree.HasAncestor(&grandchild_01));
  EXPECT_FALSE(tree.HasAncestor(&grandchild_10));
  EXPECT_FALSE(tree.HasAncestor(&grandchild_11));

  EXPECT_TRUE(child_0.HasAncestor(&tree));
  EXPECT_TRUE(child_1.HasAncestor(&tree));
  EXPECT_TRUE(grandchild_00.HasAncestor(&tree));
  EXPECT_TRUE(grandchild_01.HasAncestor(&tree));
  EXPECT_TRUE(grandchild_10.HasAncestor(&tree));
  EXPECT_TRUE(grandchild_11.HasAncestor(&tree));

  EXPECT_FALSE(child_0.HasAncestor(&child_1));
  EXPECT_FALSE(child_1.HasAncestor(&child_0));

  EXPECT_FALSE(child_0.HasAncestor(&grandchild_00));
  EXPECT_FALSE(child_0.HasAncestor(&grandchild_01));
  EXPECT_FALSE(child_0.HasAncestor(&grandchild_10));
  EXPECT_FALSE(child_0.HasAncestor(&grandchild_11));
  EXPECT_FALSE(child_1.HasAncestor(&grandchild_00));
  EXPECT_FALSE(child_1.HasAncestor(&grandchild_01));
  EXPECT_FALSE(child_1.HasAncestor(&grandchild_10));
  EXPECT_FALSE(child_1.HasAncestor(&grandchild_11));

  EXPECT_TRUE(grandchild_00.HasAncestor(&child_0));
  EXPECT_FALSE(grandchild_00.HasAncestor(&child_1));
  EXPECT_TRUE(grandchild_01.HasAncestor(&child_0));
  EXPECT_FALSE(grandchild_01.HasAncestor(&child_1));
  EXPECT_FALSE(grandchild_10.HasAncestor(&child_0));
  EXPECT_TRUE(grandchild_10.HasAncestor(&child_1));
  EXPECT_FALSE(grandchild_11.HasAncestor(&child_0));
  EXPECT_TRUE(grandchild_11.HasAncestor(&child_1));
}

TEST(VectorTreeTest, FamilyTreeNextPreviousSiblings) {
  auto tree = verible::testing::MakeExampleFamilyTree();
  auto& child_0 = tree.Children()[0];
  auto& child_1 = tree.Children()[1];
  auto& grandchild_00 = child_0.Children()[0];
  auto& grandchild_01 = child_0.Children()[1];
  auto& grandchild_10 = child_1.Children()[0];
  auto& grandchild_11 = child_1.Children()[1];

  // Verify child generation.
  EXPECT_EQ(child_0.NextSibling(), &child_1);
  EXPECT_EQ(child_1.NextSibling(), nullptr);
  EXPECT_EQ(child_0.PreviousSibling(), nullptr);
  EXPECT_EQ(child_1.PreviousSibling(), &child_0);

  // Verify grandchild generation.
  EXPECT_EQ(grandchild_00.NextSibling(), &grandchild_01);
  EXPECT_EQ(grandchild_01.NextSibling(), nullptr);
  EXPECT_EQ(grandchild_10.NextSibling(), &grandchild_11);
  EXPECT_EQ(grandchild_11.NextSibling(), nullptr);
  EXPECT_EQ(grandchild_00.PreviousSibling(), nullptr);
  EXPECT_EQ(grandchild_01.PreviousSibling(), &grandchild_00);
  EXPECT_EQ(grandchild_10.PreviousSibling(), nullptr);
  EXPECT_EQ(grandchild_11.PreviousSibling(), &grandchild_10);

  {  // same but testing const method variants.
    const auto& cchild_0(child_0);
    const auto& cchild_1(child_1);
    const auto& cgrandchild_00(grandchild_00);
    const auto& cgrandchild_01(grandchild_01);
    const auto& cgrandchild_10(grandchild_10);
    const auto& cgrandchild_11(grandchild_11);

    // Verify child generation.
    EXPECT_EQ(cchild_0.NextSibling(), &cchild_1);
    EXPECT_EQ(cchild_1.NextSibling(), nullptr);
    EXPECT_EQ(cchild_0.PreviousSibling(), nullptr);
    EXPECT_EQ(cchild_1.PreviousSibling(), &cchild_0);

    // Verify grandchild generation.
    EXPECT_EQ(cgrandchild_00.NextSibling(), &cgrandchild_01);
    EXPECT_EQ(cgrandchild_01.NextSibling(), nullptr);
    EXPECT_EQ(cgrandchild_10.NextSibling(), &cgrandchild_11);
    EXPECT_EQ(cgrandchild_11.NextSibling(), nullptr);
    EXPECT_EQ(cgrandchild_00.PreviousSibling(), nullptr);
    EXPECT_EQ(cgrandchild_01.PreviousSibling(), &cgrandchild_00);
    EXPECT_EQ(cgrandchild_10.PreviousSibling(), nullptr);
    EXPECT_EQ(cgrandchild_11.PreviousSibling(), &cgrandchild_10);
  }
}

TEST(VectorTreeTest, FamilyTreeNextPreviousLeafChain) {
  auto tree = verible::testing::MakeExampleFamilyTree();
  auto& grandchild_00 = tree.Children()[0].Children()[0];
  auto& grandchild_01 = tree.Children()[0].Children()[1];
  auto& grandchild_10 = tree.Children()[1].Children()[0];
  auto& grandchild_11 = tree.Children()[1].Children()[1];

  // Verify forward links.
  EXPECT_EQ(grandchild_00.NextLeaf(), &grandchild_01);
  EXPECT_EQ(grandchild_01.NextLeaf(), &grandchild_10);
  EXPECT_EQ(grandchild_10.NextLeaf(), &grandchild_11);
  EXPECT_EQ(grandchild_11.NextLeaf(), nullptr);

  // Verify reverse links.
  EXPECT_EQ(grandchild_00.PreviousLeaf(), nullptr);
  EXPECT_EQ(grandchild_01.PreviousLeaf(), &grandchild_00);
  EXPECT_EQ(grandchild_10.PreviousLeaf(), &grandchild_01);
  EXPECT_EQ(grandchild_11.PreviousLeaf(), &grandchild_10);

  {  // same but testing const method variants.
    const auto& cgrandchild_00(grandchild_00);
    const auto& cgrandchild_01(grandchild_01);
    const auto& cgrandchild_10(grandchild_10);
    const auto& cgrandchild_11(grandchild_11);

    // Verify forward links.
    EXPECT_EQ(cgrandchild_00.NextLeaf(), &cgrandchild_01);
    EXPECT_EQ(cgrandchild_01.NextLeaf(), &cgrandchild_10);
    EXPECT_EQ(cgrandchild_10.NextLeaf(), &cgrandchild_11);
    EXPECT_EQ(cgrandchild_11.NextLeaf(), nullptr);

    // Verify reverse links.
    EXPECT_EQ(cgrandchild_00.PreviousLeaf(), nullptr);
    EXPECT_EQ(cgrandchild_01.PreviousLeaf(), &cgrandchild_00);
    EXPECT_EQ(cgrandchild_10.PreviousLeaf(), &cgrandchild_01);
    EXPECT_EQ(cgrandchild_11.PreviousLeaf(), &cgrandchild_10);
  }
}

TEST(VectorTreeTest, FamilyTreeMembersTransformed) {
  const auto orig_tree = verible::testing::MakeExampleFamilyTree();
  const auto tree = orig_tree.Transform<NameOnly>(NameOnlyConverter);
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
      auto& lchild = ltree.DescendPath(path.begin(), path.end());
      auto& rchild = rtree.DescendPath(path.begin(), path.end());

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
      auto& lchild = ltree.DescendPath(path.begin(), path.end());
      auto& rchild = rtree.DescendPath(path.begin(), path.end());

      rchild.NewChild(NamedInterval(8, 9, "black-sheep"));
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
      const auto& lparent = ltree.DescendPath(path.begin(), path.end() - 1);
      const auto& rparent = rtree.DescendPath(path.begin(), path.end() - 1);
      auto& rchild = rtree.DescendPath(path.begin(), path.end());

      rchild.NewSibling(NamedInterval(8, 9, "black-sheep"));
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
    auto& lchild = ltree.DescendPath(path.begin(), path.end());
    auto& rchild = rtree.DescendPath(path.begin(), path.end());

    lchild.Children().clear();
    const auto result_pair = StructureEqual(ltree, rtree);
    EXPECT_EQ(result_pair.left, &lchild);
    EXPECT_EQ(result_pair.right, &rchild);
  }
}

bool EqualNamedIntervalIgnoreName(const NamedInterval& l,
                                  const NamedInterval& r) {
  return l.left == r.left && l.right == r.right;  // ignore .name
}

TEST(VectorTreeTest, FamilyTreeMembersDeepEqualCustomComparator) {
  // Mutate grandchildren structure.
  for (size_t i = 0; i < 2; ++i) {
    for (size_t j = 0; j < 2; ++j) {
      auto ltree = verible::testing::MakeExampleFamilyTree();
      auto rtree = verible::testing::MakeExampleFamilyTree();
      const std::vector<size_t> path({i, j});
      auto& lchild = ltree.DescendPath(path.begin(), path.end());
      auto& rchild = rtree.DescendPath(path.begin(), path.end());
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
  typedef VectorTree<int> tree_type;
  tree_type tree1{0}, tree2{0};
  EXPECT_EQ(tree1.NearestCommonAncestor(&tree2), nullptr);
  EXPECT_EQ(tree2.NearestCommonAncestor(&tree1), nullptr);
}

TEST(VectorTreeTest, NearestCommonAncestorNoneConst) {
  typedef VectorTree<int> tree_type;
  const tree_type tree1{0}, tree2{0};
  EXPECT_EQ(tree1.NearestCommonAncestor(&tree2), nullptr);
  EXPECT_EQ(tree2.NearestCommonAncestor(&tree1), nullptr);
}

TEST(VectorTreeTest, NearestCommonAncestorSameMutable) {
  typedef VectorTree<int> tree_type;
  tree_type tree{0};
  EXPECT_EQ(tree.NearestCommonAncestor(&tree), &tree);
  EXPECT_EQ(tree.NearestCommonAncestor(&tree), &tree);
}

TEST(VectorTreeTest, NearestCommonAncestorSameConst) {
  typedef VectorTree<int> tree_type;
  const tree_type tree{0};
  EXPECT_EQ(tree.NearestCommonAncestor(&tree), &tree);
  EXPECT_EQ(tree.NearestCommonAncestor(&tree), &tree);
}

TEST(VectorTreeTest, NearestCommonAncestorOneIsRootConst) {
  typedef VectorTree<int> tree_type;
  const tree_type tree(1,            //
                       tree_type(2,  //
                                 tree_type(4), tree_type(5)),
                       tree_type(3,  //
                                 tree_type(6), tree_type(7)));

  for (int i = 0; i < 2; ++i) {
    {
      const auto path = {i};
      auto& child = tree.DescendPath(path.begin(), path.end());
      EXPECT_EQ(tree.NearestCommonAncestor(&child), &tree);
      EXPECT_EQ(child.NearestCommonAncestor(&tree), &tree);
    }
    for (int j = 0; j < 2; ++j) {
      const auto path = {i, j};
      auto& grandchild = tree.DescendPath(path.begin(), path.end());
      EXPECT_EQ(tree.NearestCommonAncestor(&grandchild), &tree);
      EXPECT_EQ(grandchild.NearestCommonAncestor(&tree), &tree);
    }
  }
}

TEST(VectorTreeTest, NearestCommonAncestorNeitherIsRootConst) {
  typedef VectorTree<int> tree_type;
  const tree_type tree(1,            //
                       tree_type(2,  //
                                 tree_type(4), tree_type(5)),
                       tree_type(3,  //
                                 tree_type(6), tree_type(7)));
  auto& left = tree.Children()[0];
  auto& right = tree.Children()[1];
  EXPECT_EQ(left.NearestCommonAncestor(&right), &tree);
  EXPECT_EQ(right.NearestCommonAncestor(&left), &tree);

  for (int i = 0; i < 2; ++i) {
    {
      const auto left_path = {0, i};
      auto& left_grandchild =
          tree.DescendPath(left_path.begin(), left_path.end());
      EXPECT_EQ(left.NearestCommonAncestor(&left_grandchild), &left);
      EXPECT_EQ(left_grandchild.NearestCommonAncestor(&left), &left);
      EXPECT_EQ(right.NearestCommonAncestor(&left_grandchild), &tree);
      EXPECT_EQ(left_grandchild.NearestCommonAncestor(&right), &tree);
    }
    {
      const auto right_path = {1, i};
      auto& right_grandchild =
          tree.DescendPath(right_path.begin(), right_path.end());
      EXPECT_EQ(right.NearestCommonAncestor(&right_grandchild), &right);
      EXPECT_EQ(right_grandchild.NearestCommonAncestor(&right), &right);
      EXPECT_EQ(left.NearestCommonAncestor(&right_grandchild), &tree);
      EXPECT_EQ(right_grandchild.NearestCommonAncestor(&left), &tree);
    }
  }
}

TEST(VectorTreeTest, ApplyPreOrderPrint) {
  const auto tree = verible::testing::MakeExampleFamilyTree();

  std::ostringstream stream;
  tree.ApplyPreOrder([&stream](const NamedInterval& interval) {
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
  const auto tree = orig_tree.Transform<NameOnly>(NameOnlyConverter);

  std::ostringstream stream;
  tree.ApplyPreOrder([&stream](const NameOnly& n) { stream << n; });
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
  tree.ApplyPostOrder([&stream](const NamedInterval& interval) {
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
  tree.ApplyPreOrder(verible::testing::VerifyInterval);
}

TEST(VectorTreeTest, ApplyPostOrderVerify) {
  const auto tree = verible::testing::MakeExampleFamilyTree();
  // Verify invariant at every node.
  tree.ApplyPostOrder(verible::testing::VerifyInterval);
}

TEST(VectorTreeTest, ApplyPreOrderTransformValue) {
  auto tree = verible::testing::MakeExampleFamilyTree();

  // Transform intervals.
  std::vector<absl::string_view> visit_order;
  const int shift = 2;
  tree.ApplyPreOrder([=, &visit_order](NamedInterval& interval) {
    visit_order.push_back(interval.name);
    interval.left += shift;
    interval.right += shift;
  });
  EXPECT_THAT(visit_order,
              ElementsAre("grandparent", "parent1", "child1", "child2",
                          "parent2", "child3", "child4"));

  // Print output for verification.
  std::ostringstream stream;
  tree.ApplyPreOrder([&stream](const NamedInterval& interval) {
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
  std::vector<absl::string_view> visit_order;
  const int shift = 2;
  tree.ApplyPreOrder([=, &visit_order](VectorTreeTestType& node) {
    auto& interval = node.Value();
    visit_order.push_back(interval.name);
    interval.left += shift;
    interval.right += shift;
  });
  EXPECT_THAT(visit_order,
              ElementsAre("grandparent", "parent1", "child1", "child2",
                          "parent2", "child3", "child4"));

  // Print output for verification.
  std::ostringstream stream;
  tree.ApplyPreOrder([&stream](const NamedInterval& interval) {
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
  std::vector<absl::string_view> visit_order;
  const int shift = 1;
  tree.ApplyPostOrder([=, &visit_order](NamedInterval& interval) {
    visit_order.push_back(interval.name);
    interval.left += shift;
    interval.right += shift;
  });
  EXPECT_THAT(visit_order, ElementsAre("child1", "child2", "parent1", "child3",
                                       "child4", "parent2", "grandparent"));

  // Print output for verification.
  std::ostringstream stream;
  tree.ApplyPostOrder([&stream](const NamedInterval& interval) {
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
  std::vector<absl::string_view> visit_order;
  const int shift = 1;
  tree.ApplyPostOrder([=, &visit_order](VectorTreeTestType& node) {
    auto& interval = node.Value();
    visit_order.push_back(interval.name);
    interval.left += shift;
    interval.right += shift;
  });
  EXPECT_THAT(visit_order, ElementsAre("child1", "child2", "parent1", "child3",
                                       "child4", "parent2", "grandparent"));

  // Print output for verification.
  std::ostringstream stream;
  tree.ApplyPostOrder([&stream](const NamedInterval& interval) {
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
  EXPECT_FALSE(tree.HoistOnlyChild());

  EXPECT_TRUE(tree.is_leaf());
  EXPECT_EQ(tree.Parent(), nullptr);
  EXPECT_EQ(tree.NumAncestors(), 0);
  EXPECT_EQ(tree.BirthRank(), 0);  // no parent
  EXPECT_EQ(tree.Root(), &tree);

  const auto& value = tree.Value();
  EXPECT_EQ(value.left, 0);
  EXPECT_EQ(value.right, 2);
  EXPECT_EQ(value.name, "root");

  ExpectPath(tree, "{}");
}

TEST(VectorTreeTest, HoistOnlyChildOneChildTreeGreatestAncestor) {
  VectorTreeTestType tree(verible::testing::MakeOneChildPolicyExampleTree());
  // effectively remove the "root" generation
  EXPECT_TRUE(tree.HoistOnlyChild());
  {
    const auto& child = tree;
    EXPECT_EQ(child.Parent(), nullptr);
    EXPECT_EQ(child.Root(), &tree);
    EXPECT_FALSE(child.is_leaf());
    EXPECT_EQ(child.NumAncestors(), 0);
    EXPECT_EQ(child.BirthRank(), 0);

    const auto& cvalue = child.Value();
    EXPECT_EQ(cvalue.left, 0);
    EXPECT_EQ(cvalue.right, 3);
    EXPECT_EQ(cvalue.name, "gen1");
    ExpectPath(child, "{}");

    EXPECT_EQ(child.NextSibling(), nullptr);
    EXPECT_EQ(child.PreviousSibling(), nullptr);

    // The invoking node need not be a leaf.
    EXPECT_EQ(child.NextLeaf(), nullptr);
    EXPECT_EQ(child.PreviousLeaf(), nullptr);

    {
      const auto& grandchild = child.Children().front();
      EXPECT_EQ(grandchild.Parent(), &child);
      EXPECT_EQ(grandchild.Root(), &tree);
      EXPECT_TRUE(grandchild.is_leaf());
      EXPECT_EQ(grandchild.NumAncestors(), 1);
      EXPECT_EQ(grandchild.BirthRank(), 0);

      const auto& gcvalue = grandchild.Value();
      EXPECT_EQ(gcvalue.left, 0);
      EXPECT_EQ(gcvalue.right, 3);
      EXPECT_EQ(gcvalue.name, "gen2");
      ExpectPath(grandchild, "{0}");

      // As the ancestry chain is linear, Leftmost == Rightmost.
      EXPECT_EQ(child.LeftmostDescendant(), &grandchild);
      EXPECT_EQ(child.RightmostDescendant(), &grandchild);
      EXPECT_EQ(tree.LeftmostDescendant(), &grandchild);
      EXPECT_EQ(tree.RightmostDescendant(), &grandchild);

      EXPECT_EQ(grandchild.NextSibling(), nullptr);
      EXPECT_EQ(grandchild.PreviousSibling(), nullptr);

      // There is still only a single leaf in a one-child tree,
      // thus next and previous do not exist.
      EXPECT_EQ(grandchild.NextLeaf(), nullptr);
      EXPECT_EQ(grandchild.PreviousLeaf(), nullptr);
    }
  }
}

TEST(VectorTreeTest, HoistOnlyChildOneChildTreeMiddleAncestor) {
  VectorTreeTestType tree(verible::testing::MakeOneChildPolicyExampleTree());
  EXPECT_TRUE(tree.Children().front().HoistOnlyChild());
  {
    const auto& value = tree.Value();
    EXPECT_EQ(value.left, 0);
    EXPECT_EQ(value.right, 3);
    EXPECT_EQ(value.name, "root");

    EXPECT_EQ(tree.NextSibling(), nullptr);
    EXPECT_EQ(tree.PreviousSibling(), nullptr);

    // The invoking node need not be a leaf.
    EXPECT_EQ(tree.NextLeaf(), nullptr);
    EXPECT_EQ(tree.PreviousLeaf(), nullptr);

    // The "gen1" node is removed, and now the grandchild is directly linked.
    {
      const auto& grandchild = tree.Children().front();
      EXPECT_EQ(grandchild.Parent(), &tree);
      EXPECT_EQ(grandchild.Root(), &tree);
      EXPECT_TRUE(grandchild.is_leaf());
      EXPECT_EQ(grandchild.NumAncestors(), 1);
      EXPECT_EQ(grandchild.BirthRank(), 0);

      const auto& gcvalue = grandchild.Value();
      EXPECT_EQ(gcvalue.left, 0);
      EXPECT_EQ(gcvalue.right, 3);
      EXPECT_EQ(gcvalue.name, "gen2");
      ExpectPath(grandchild, "{0}");

      // As the ancestry chain is linear, Leftmost == Rightmost.
      EXPECT_EQ(tree.LeftmostDescendant(), &grandchild);
      EXPECT_EQ(tree.RightmostDescendant(), &grandchild);

      EXPECT_EQ(grandchild.NextSibling(), nullptr);
      EXPECT_EQ(grandchild.PreviousSibling(), nullptr);

      // There is still only a single leaf in a one-child tree,
      // thus next and previous do not exist.
      EXPECT_EQ(grandchild.NextLeaf(), nullptr);
      EXPECT_EQ(grandchild.PreviousLeaf(), nullptr);
    }
  }
}

TEST(VectorTreeTest, HoistOnlyChildFamilyTree) {
  VectorTreeTestType tree(verible::testing::MakeExampleFamilyTree());
  // No change because each generation has more than one child.
  EXPECT_FALSE(tree.HoistOnlyChild());
}

// Copy-extract tree values from a node's direct children only.
// TODO(fangism): Adapt this into a public method of VectorTree.
template <typename T>
static std::vector<typename T::value_type> NodeValues(const T& node) {
  std::vector<typename T::value_type> result;
  result.reserve(node.Children().size());
  for (const auto& child : node.Children()) {
    result.emplace_back(child.Value());
  }
  return result;
}

TEST(VectorTreeTest, AdoptSubtreesFromEmptyToEmpty) {
  typedef VectorTree<int> tree_type;
  tree_type tree1(1), tree2(2);  // no subtrees
  EXPECT_TRUE(tree1.is_leaf());
  EXPECT_TRUE(tree2.is_leaf());

  tree1.AdoptSubtreesFrom(&tree2);
  EXPECT_TRUE(tree1.is_leaf());
  EXPECT_TRUE(tree2.is_leaf());
}

TEST(VectorTreeTest, AdoptSubtreesFromEmptyToNonempty) {
  typedef VectorTree<int> tree_type;
  tree_type tree1(1, tree_type(4)), tree2(2);
  EXPECT_THAT(NodeValues(tree1), ElementsAre(4));
  EXPECT_THAT(NodeValues(tree2), ElementsAre());

  tree1.AdoptSubtreesFrom(&tree2);
  EXPECT_THAT(NodeValues(tree1), ElementsAre(4));
  EXPECT_THAT(NodeValues(tree2), ElementsAre());
}

TEST(VectorTreeTest, AdoptSubtreesFromNonemptyToEmpty) {
  typedef VectorTree<int> tree_type;
  tree_type tree1(1), tree2(2, tree_type(5));
  EXPECT_THAT(NodeValues(tree1), ElementsAre());
  EXPECT_THAT(NodeValues(tree2), ElementsAre(5));

  tree1.AdoptSubtreesFrom(&tree2);
  EXPECT_THAT(NodeValues(tree1), ElementsAre(5));
  EXPECT_THAT(NodeValues(tree2), ElementsAre());
}

TEST(VectorTreeTest, AdoptSubtreesFromNonemptyToNonempty) {
  typedef VectorTree<int> tree_type;
  tree_type tree1(1, tree_type(3), tree_type(6)),
      tree2(2, tree_type(5), tree_type(8));
  EXPECT_THAT(NodeValues(tree1), ElementsAre(3, 6));
  EXPECT_THAT(NodeValues(tree2), ElementsAre(5, 8));

  tree1.AdoptSubtreesFrom(&tree2);
  EXPECT_THAT(NodeValues(tree1), ElementsAre(3, 6, 5, 8));
  EXPECT_THAT(NodeValues(tree2), ElementsAre());
}

TEST(VectorTreeTest, MergeConsecutiveSiblingsTooFewElements) {
  typedef VectorTree<int> tree_type;
  tree_type tree(1, tree_type(2));
  auto adder = [](int* left, const int& right) { *left += right; };
  EXPECT_THAT(NodeValues(tree), ElementsAre(2));
  EXPECT_DEATH(tree.MergeConsecutiveSiblings(0, adder), "");
}

TEST(VectorTreeTest, MergeConsecutiveSiblingsOutOfBounds) {
  typedef VectorTree<int> tree_type;
  tree_type tree(1, tree_type(2), tree_type(3));
  auto adder = [](int* left, const int& right) { *left += right; };
  EXPECT_THAT(NodeValues(tree), ElementsAre(2, 3));
  EXPECT_DEATH(tree.MergeConsecutiveSiblings(1, adder), "");
}

TEST(VectorTreeTest, MergeConsecutiveSiblingsAddLeaves) {
  typedef VectorTree<int> tree_type;
  tree_type tree(1, tree_type(2), tree_type(3), tree_type(4), tree_type(5));
  auto adder = [](int* left, const int& right) { *left += right; };
  EXPECT_THAT(NodeValues(tree), ElementsAre(2, 3, 4, 5));

  VLOG(1) << __FUNCTION__ << ": before first merge";

  tree.MergeConsecutiveSiblings(1, adder);  // combine middle two subtrees
  EXPECT_THAT(NodeValues(tree), ElementsAre(2, 7, 5));
  VLOG(1) << __FUNCTION__ << ": after first merge";

  tree.MergeConsecutiveSiblings(1, adder);  // combine last two subtrees
  EXPECT_THAT(NodeValues(tree), ElementsAre(2, 12));
  VLOG(1) << __FUNCTION__ << ": after second merge";

  tree.MergeConsecutiveSiblings(0, adder);  // combine only two subtrees
  EXPECT_THAT(NodeValues(tree), ElementsAre(14));
  VLOG(1) << __FUNCTION__ << ": after third merge";
}

TEST(VectorTreeTest, MergeConsecutiveSiblingsConcatenateSubtreesOnce) {
  typedef VectorTree<int> tree_type;
  tree_type tree(1,            //
                 tree_type(2,  //
                           tree_type(6), tree_type(7)),
                 tree_type(3,  //
                           tree_type(8), tree_type(9)),
                 tree_type(4,  //
                           tree_type(10), tree_type(11)),
                 tree_type(5,  //
                           tree_type(12), tree_type(13)));
  auto subtractor = [](int* left, const int& right) { *left -= right; };
  EXPECT_THAT(NodeValues(tree), ElementsAre(2, 3, 4, 5));

  tree.MergeConsecutiveSiblings(1, subtractor);  // combine middle two subtrees
  EXPECT_THAT(NodeValues(tree), ElementsAre(2, /* 3 - 4 */ -1, 5));
  EXPECT_THAT(NodeValues(tree.Children()[1]), ElementsAre(8, 9, 10, 11));
}

TEST(VectorTreeTest, MergeConsecutiveSiblingsConcatenateSubtrees) {
  typedef VectorTree<int> tree_type;
  tree_type tree(1,            //
                 tree_type(2,  //
                           tree_type(6), tree_type(7)),
                 tree_type(3,  //
                           tree_type(8), tree_type(9)),
                 tree_type(4,  //
                           tree_type(10), tree_type(11)),
                 tree_type(5,  //
                           tree_type(12), tree_type(13)));
  auto subtractor = [](int* left, const int& right) { *left -= right; };
  EXPECT_THAT(NodeValues(tree), ElementsAre(2, 3, 4, 5));

  tree.MergeConsecutiveSiblings(0, subtractor);  // combine first two subtrees
  EXPECT_THAT(NodeValues(tree), ElementsAre(/* 2 - 3 */ -1, 4, 5));
  EXPECT_THAT(NodeValues(tree.Children()[0]), ElementsAre(6, 7, 8, 9));

  tree.MergeConsecutiveSiblings(1, subtractor);  // combine last two subtrees
  EXPECT_THAT(NodeValues(tree), ElementsAre(-1, /* 4 - 5 */ -1));
  EXPECT_THAT(NodeValues(tree.Children()[1]), ElementsAre(10, 11, 12, 13));

  tree.MergeConsecutiveSiblings(0, subtractor);  // combine only two subtrees
  EXPECT_THAT(NodeValues(tree), ElementsAre(/* -1 - -1 */ 0));
  EXPECT_THAT(NodeValues(tree.Children()[0]),
              ElementsAre(6, 7, 8, 9, 10, 11, 12, 13));
}

TEST(VectorTreeTest, RemoveSelfFromParentRoot) {
  typedef VectorTree<int> tree_type;
  tree_type tree(1);
  EXPECT_DEATH(tree.RemoveSelfFromParent(), "");
}

TEST(VectorTreeTest, RemoveSelfFromParentFirstChild) {
  typedef VectorTree<int> tree_type;
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
  tree.Children().front().RemoveSelfFromParent();
  const auto result_pair = DeepEqual(tree, expect_tree);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(VectorTreeTest, RemoveSelfFromParentMiddleChildWithGrandchildren) {
  typedef VectorTree<int> tree_type;
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
  tree.Children()[1].RemoveSelfFromParent();
  const auto result_pair = DeepEqual(tree, expect_tree);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(VectorTreeTest, RemoveSelfFromParentMiddleChildWithoutGrandchildren) {
  typedef VectorTree<int> tree_type;
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
  tree.Children()[2].RemoveSelfFromParent();
  const auto result_pair = DeepEqual(tree, expect_tree);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(VectorTreeTest, RemoveSelfFromParentLastChild) {
  typedef VectorTree<int> tree_type;
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
  tree.Children().back().RemoveSelfFromParent();
  const auto result_pair = DeepEqual(tree, expect_tree);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(VectorTreeTest, FlattenOnceNoChildren) {
  typedef VectorTree<int> tree_type;
  tree_type tree(1);
  EXPECT_THAT(NodeValues(tree), ElementsAre());

  const tree_type expect_tree(1);
  tree.FlattenOnce();
  const auto result_pair = DeepEqual(tree, expect_tree);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(VectorTreeTest, FlattenOnceNoGrandchildren) {
  typedef VectorTree<int> tree_type;
  tree_type tree(1,  // no grandchildren
                 tree_type(2), tree_type(3), tree_type(4), tree_type(5));
  EXPECT_THAT(NodeValues(tree), ElementsAre(2, 3, 4, 5));

  const tree_type expect_tree(1);
  tree.FlattenOnce();
  const auto result_pair = DeepEqual(tree, expect_tree);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(VectorTreeTest, FlattenOnceOneGrandchild) {
  typedef VectorTree<int> tree_type;
  tree_type tree(1, tree_type(2, tree_type(3)));
  EXPECT_THAT(NodeValues(tree), ElementsAre(2));

  const tree_type expect_tree(1, tree_type(3));
  tree.FlattenOnce();
  const auto result_pair = DeepEqual(tree, expect_tree);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(VectorTreeTest, FlattenOnceMixed) {
  typedef VectorTree<int> tree_type;
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
  tree.FlattenOnce();
  const auto result_pair = DeepEqual(tree, expect_tree);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(VectorTreeTest, FlattenOnceAllNonempty) {
  typedef VectorTree<int> tree_type;
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
  tree.FlattenOnce();
  const auto result_pair = DeepEqual(tree, expect_tree);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(VectorTreeTest, FlattenOnceGreatgrandchildren) {
  typedef VectorTree<int> tree_type;
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
  tree.FlattenOnce();
  const auto result_pair = DeepEqual(tree, expect_tree);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(VectorTreeTest, FlattenOnlyChildrenWithChildrenNoChildren) {
  typedef VectorTree<int> tree_type;
  tree_type tree(1);
  EXPECT_THAT(NodeValues(tree), ElementsAre());

  const tree_type expect_tree(1);
  std::vector<size_t> new_offsets;
  tree.FlattenOnlyChildrenWithChildren(&new_offsets);
  const auto result_pair = DeepEqual(tree, expect_tree);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
  EXPECT_TRUE(new_offsets.empty());
}

TEST(VectorTreeTest, FlattenOnlyChildrenWithChildrenNoChildrenNoOffsets) {
  typedef VectorTree<int> tree_type;
  tree_type tree(1);
  EXPECT_THAT(NodeValues(tree), ElementsAre());

  const tree_type expect_tree(1);
  tree.FlattenOnlyChildrenWithChildren(/* no offsets */);
  const auto result_pair = DeepEqual(tree, expect_tree);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(VectorTreeTest, FlattenOnlyChildrenWithChildrenNoGrandchildren) {
  typedef VectorTree<int> tree_type;
  tree_type tree(1,  // no grandchildren
                 tree_type(2), tree_type(3), tree_type(4), tree_type(5));
  EXPECT_THAT(NodeValues(tree), ElementsAre(2, 3, 4, 5));

  const tree_type expect_tree(1,  // all children preserved
                              tree_type(2), tree_type(3), tree_type(4),
                              tree_type(5));
  std::vector<size_t> new_offsets;
  tree.FlattenOnlyChildrenWithChildren(&new_offsets);
  const auto result_pair = DeepEqual(tree, expect_tree);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
  EXPECT_THAT(new_offsets, ElementsAre(0, 1, 2, 3));
}

TEST(VectorTreeTest, FlattenOnlyChildrenWithChildrenOneGrandchild) {
  typedef VectorTree<int> tree_type;
  tree_type tree(1, tree_type(2, tree_type(3)));
  EXPECT_THAT(NodeValues(tree), ElementsAre(2));

  const tree_type expect_tree(1, tree_type(3));
  std::vector<size_t> new_offsets;
  tree.FlattenOnlyChildrenWithChildren(&new_offsets);
  const auto result_pair = DeepEqual(tree, expect_tree);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
  EXPECT_THAT(new_offsets, ElementsAre(0));
}

TEST(VectorTreeTest, FlattenOnlyChildrenWithChildrenOneGrandchildNoOffsets) {
  typedef VectorTree<int> tree_type;
  tree_type tree(1, tree_type(2, tree_type(3)));
  EXPECT_THAT(NodeValues(tree), ElementsAre(2));

  const tree_type expect_tree(1, tree_type(3));
  tree.FlattenOnlyChildrenWithChildren(/* no offsets */);
  const auto result_pair = DeepEqual(tree, expect_tree);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(VectorTreeTest, FlattenOnlyChildrenWithChildrenTwoGrandchildren) {
  typedef VectorTree<int> tree_type;
  tree_type tree(1, tree_type(2, tree_type(3), tree_type(7)));
  EXPECT_THAT(NodeValues(tree), ElementsAre(2));

  const tree_type expect_tree(1, tree_type(3), tree_type(7));
  std::vector<size_t> new_offsets;
  tree.FlattenOnlyChildrenWithChildren(&new_offsets);
  const auto result_pair = DeepEqual(tree, expect_tree);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
  EXPECT_THAT(new_offsets, ElementsAre(0));
}

TEST(VectorTreeTest, FlattenOnlyChildrenWithChildrenMixed) {
  typedef VectorTree<int> tree_type;
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
  tree.FlattenOnlyChildrenWithChildren(&new_offsets);
  const auto result_pair = DeepEqual(tree, expect_tree);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
  EXPECT_THAT(new_offsets, ElementsAre(0, 1, 3, 4));
}

TEST(VectorTreeTest, FlattenOnlyChildrenWithChildrenAllNonempty) {
  typedef VectorTree<int> tree_type;
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
  tree.FlattenOnlyChildrenWithChildren(&new_offsets);
  const auto result_pair = DeepEqual(tree, expect_tree);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
  EXPECT_THAT(new_offsets, ElementsAre(0, 2, 4, 6));
}

TEST(VectorTreeTest, FlattenOnlyChildrenWithChildrenGreatgrandchildren) {
  typedef VectorTree<int> tree_type;
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
  tree.FlattenOnlyChildrenWithChildren(&new_offsets);
  const auto result_pair = DeepEqual(tree, expect_tree);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
  EXPECT_THAT(new_offsets, ElementsAre(0, 1, 2, 3));
}

TEST(VectorTreeTest, FlattenOneChildEmpty) {
  typedef VectorTree<int> tree_type;
  tree_type tree(4);  // no children
  EXPECT_DEATH(tree.FlattenOneChild(0), "");
}

TEST(VectorTreeTest, FlattenOneChildOnlyChildNoGrandchildren) {
  typedef VectorTree<int> tree_type;
  tree_type tree(4, tree_type(2));  // no grandchildren
  const tree_type expect_tree(4);
  tree.FlattenOneChild(0);
  const auto result_pair = DeepEqual(tree, expect_tree);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(VectorTreeTest, FlattenOneChildOnlyChildOneGrandchild) {
  typedef VectorTree<int> tree_type;
  tree_type tree(4, tree_type(2, tree_type(11)));  // with grandchild
  const tree_type expect_tree(4, tree_type(11));
  tree.FlattenOneChild(0);
  const auto result_pair = DeepEqual(tree, expect_tree);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(VectorTreeTest, FlattenOneChildFirstChildInFamilyTree) {
  typedef VectorTree<int> tree_type;
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
  tree.FlattenOneChild(0);
  const auto result_pair = DeepEqual(tree, expect_tree);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(VectorTreeTest, FlattenOneChildMiddleChildInFamilyTree) {
  typedef VectorTree<int> tree_type;
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
  tree.FlattenOneChild(1);
  const auto result_pair = DeepEqual(tree, expect_tree);
  EXPECT_EQ(result_pair.left, nullptr) << *result_pair.left;
  EXPECT_EQ(result_pair.right, nullptr) << *result_pair.right;
}

TEST(VectorTreeTest, FlattenOneChildLastChildInFamilyTree) {
  typedef VectorTree<int> tree_type;
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
  tree.FlattenOneChild(3);
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
  tree.PrintTree(&stream,
                 [](std::ostream& s,
                    const decltype(tree)::value_type& value) -> std::ostream& {
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

}  // namespace
}  // namespace verible
