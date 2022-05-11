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

#include "common/util/expandable_tree_view.h"

#include <sstream>
#include <vector>

#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "common/util/vector_tree.h"
#include "common/util/vector_tree_test_util.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace verible {
namespace {

using ::testing::ElementsAre;
using verible::testing::NamedInterval;
using verible::testing::VectorTreeTestType;

using ExpandableTreeViewTestType =
    ExpandableTreeView<VectorTree<NamedInterval>>;

// Test that basic Tree View operations work on a singleton node.
TEST(ExpandableTreeViewTest, RootOnly) {
  const VectorTreeTestType tree(verible::testing::MakeRootOnlyExampleTree());
  const auto& value = tree.Value();

  // View testing.
  const ExpandableTreeViewTestType tree_view(tree);
  EXPECT_EQ(&tree_view.Value().Value(), &value);
  EXPECT_TRUE(tree_view.Value().IsExpanded());

  // Iterator testing.
  ExpandableTreeViewTestType::iterator iter(tree_view.begin()),
      end(tree_view.end());

  ASSERT_NE(iter, end);
  EXPECT_EQ(&*iter, &value);
  ExpandableTreeViewTestType::iterator copy(iter);

  ++iter;
  EXPECT_NE(iter, copy);
  EXPECT_EQ(iter, end);  // There was only one node to visit, the root.
}

TEST(ExpandableTreeViewTest, OneChildPolicyFullyExpanded) {
  const auto tree = verible::testing::MakeOneChildPolicyExampleTree();
  const ExpandableTreeViewTestType tree_view(tree);

  // Iterator testing.
  ExpandableTreeViewTestType::iterator iter(tree_view.begin()),
      end(tree_view.end());

  ASSERT_NE(iter, end);
  ExpandableTreeViewTestType::iterator copy(iter);
  EXPECT_EQ(iter->left, 0);
  EXPECT_EQ(iter->right, 3);
  // Fully expanded means reaching deepest child first.
  EXPECT_EQ(iter->name, "gen2");

  ++iter;
  EXPECT_NE(iter, copy);
  // There was only one node to visit, the only grandchild.
  EXPECT_EQ(iter, end);
}

TEST(ExpandableTreeViewTest, OneChildPolicyUnexpanded) {
  const auto tree = verible::testing::MakeOneChildPolicyExampleTree();
  ExpandableTreeViewTestType tree_view(tree);
  tree_view.Value().Unexpand();  // Mark root node as un-expanded.
  EXPECT_FALSE(tree_view.Value().IsExpanded());

  // Iterator testing.
  ExpandableTreeViewTestType::iterator iter(tree_view.begin()),
      end(tree_view.end());

  ASSERT_NE(iter, end);
  ExpandableTreeViewTestType::iterator copy(iter);
  EXPECT_EQ(iter->left, 0);
  EXPECT_EQ(iter->right, 3);
  // Unexpanded: skip any of root's children
  EXPECT_EQ(iter->name, "root");

  ++iter;
  EXPECT_NE(iter, copy);
  // There was only one node to visit, the root.
  EXPECT_EQ(iter, end);
}

TEST(ExpandableTreeViewTest, OneChildPolicyPartiallyExpanded) {
  const auto tree = verible::testing::MakeOneChildPolicyExampleTree();
  ExpandableTreeViewTestType tree_view(tree);
  tree_view[0].Value().Unexpand();  // Mark root node as un-expanded.
  EXPECT_FALSE(tree_view[0].Value().IsExpanded());

  // Iterator testing.
  ExpandableTreeViewTestType::iterator iter(tree_view.begin()),
      end(tree_view.end());

  ASSERT_NE(iter, end);
  ExpandableTreeViewTestType::iterator copy(iter);
  EXPECT_EQ(iter->left, 0);
  EXPECT_EQ(iter->right, 3);
  // Partially expanded: don't expand beyond direct child
  EXPECT_EQ(iter->name, "gen1");

  ++iter;
  EXPECT_NE(iter, copy);
  // There was only one node to visit, the root's only child.
  EXPECT_EQ(iter, end);
}

TEST(ExpandableTreeViewTest, FamilyTreeFullyExpanded) {
  const auto tree = verible::testing::MakeExampleFamilyTree();
  const ExpandableTreeViewTestType tree_view(tree);

  // Fully-expanded, expect to visit all grandchildren, and nothing else.
  ExpandableTreeViewTestType::iterator iter(tree_view.begin()),
      end(tree_view.end());
  ASSERT_NE(iter, end);
  EXPECT_EQ(iter->name, "child1");

  iter++;  // post-increment
  ASSERT_NE(iter, end);
  EXPECT_EQ(iter->name, "child2");

  iter++;
  ASSERT_NE(iter, end);
  EXPECT_EQ(iter->name, "child3");

  iter++;
  ASSERT_NE(iter, end);
  EXPECT_EQ(iter->name, "child4");

  iter++;
  EXPECT_EQ(iter, end);
}

TEST(ExpandableTreeViewTest, FamilyTreeFullyExpandedStrJoin) {
  const auto tree = verible::testing::MakeExampleFamilyTree();
  const ExpandableTreeViewTestType tree_view(tree);

  std::ostringstream stream;
  stream << absl::StrJoin(tree_view.begin(), tree_view.end(), "\n",
                          absl::StreamFormatter());
  EXPECT_EQ(stream.str(),
            "(0, 1, child1)\n(1, 2, child2)\n(2, 3, child3)\n(3, 4, child4)");
}

TEST(ExpandableTreeViewTest, FamilyTreeUnexpanded) {
  const auto tree = verible::testing::MakeExampleFamilyTree();
  ExpandableTreeViewTestType tree_view(tree);
  tree_view.Value().Unexpand();  // expect to only visit root

  ExpandableTreeViewTestType::iterator iter(tree_view.begin()),
      end(tree_view.end());
  ASSERT_NE(iter, end);
  EXPECT_EQ(iter->name, "grandparent");

  iter++;
  EXPECT_EQ(iter, end);
}

TEST(ExpandableTreeViewTest, FamilyTreeExpandedMixed) {
  const auto tree = verible::testing::MakeExampleFamilyTree();
  ExpandableTreeViewTestType tree_view(tree);

  // Unexpand only one of the parents.
  tree_view[0].Value().Unexpand();

  ExpandableTreeViewTestType::iterator iter(tree_view.begin()),
      end(tree_view.end());
  ASSERT_NE(iter, end);
  EXPECT_EQ(iter->name, "parent1");

  iter++;
  ASSERT_NE(iter, end);
  EXPECT_EQ(iter->name, "child3");

  iter++;
  ASSERT_NE(iter, end);
  EXPECT_EQ(iter->name, "child4");

  iter++;
  EXPECT_EQ(iter, end);
}

TEST(ExpandableTreeViewTest, FamilyTreeExpandedMixed2) {
  const auto tree = verible::testing::MakeExampleFamilyTree();
  ExpandableTreeViewTestType tree_view(tree);

  // Unexpand only one of the parents.
  tree_view[1].Value().Unexpand();

  ExpandableTreeViewTestType::iterator iter(tree_view.begin()),
      end(tree_view.end());
  ASSERT_NE(iter, end);
  EXPECT_EQ(iter->name, "child1");

  iter++;
  ASSERT_NE(iter, end);
  EXPECT_EQ(iter->name, "child2");

  iter++;
  ASSERT_NE(iter, end);
  EXPECT_EQ(iter->name, "parent2");

  iter++;
  EXPECT_EQ(iter, end);
}

TEST(ExpandableTreeViewTest, FamilyTreeExpandedOneLevel) {
  const auto tree = verible::testing::MakeExampleFamilyTree();
  ExpandableTreeViewTestType tree_view(tree);

  // Expand only one level.
  tree_view[0].Value().Unexpand();
  tree_view[1].Value().Unexpand();

  ExpandableTreeViewTestType::iterator iter(tree_view.begin()),
      end(tree_view.end());
  ASSERT_NE(iter, end);
  EXPECT_EQ(iter->name, "parent1");

  iter++;
  ASSERT_NE(iter, end);
  EXPECT_EQ(iter->name, "parent2");

  iter++;
  EXPECT_EQ(iter, end);
}

// Tests that function can be applied that un-expands nodes in the tree view.
TEST(ExpandableTreeViewTest, FamilyTreeApplyPreOrder) {
  const auto tree = verible::testing::MakeExampleFamilyTree();
  ExpandableTreeViewTestType tree_view(tree);

  std::vector<absl::string_view> visit_order;
  tree_view.ApplyPreOrder(
      [=, &visit_order](
          VectorTree<TreeViewNodeInfo<VectorTree<NamedInterval>>>& node) {
        const absl::string_view name = node.Value().Value().name;
        visit_order.push_back(name);
        if (name[0] == 'p') {
          node.Value().Unexpand();
        }
      });

  EXPECT_THAT(visit_order,
              ElementsAre("grandparent", "parent1", "child1", "child2",
                          "parent2", "child3", "child4"));

  ExpandableTreeViewTestType::iterator iter(tree_view.begin()),
      end(tree_view.end());
  ASSERT_NE(iter, end);
  EXPECT_EQ(iter->name, "parent1");

  iter++;
  ASSERT_NE(iter, end);
  EXPECT_EQ(iter->name, "parent2");

  iter++;
  EXPECT_EQ(iter, end);
}

// Tests that function can be applied that un-expands nodes in the tree view.
TEST(ExpandableTreeViewTest, FamilyTreeApplyPostOrder) {
  const auto tree = verible::testing::MakeExampleFamilyTree();
  ExpandableTreeViewTestType tree_view(tree);

  std::vector<absl::string_view> visit_order;
  tree_view.ApplyPostOrder(
      [=, &visit_order](
          VectorTree<TreeViewNodeInfo<VectorTree<NamedInterval>>>& node) {
        const absl::string_view name = node.Value().Value().name;
        visit_order.push_back(name);
        if (name[0] == 'p' && name.back() == '1') {
          node.Value().Unexpand();
        }
      });

  EXPECT_THAT(visit_order, ElementsAre("child1", "child2", "parent1", "child3",
                                       "child4", "parent2", "grandparent"));

  ExpandableTreeViewTestType::iterator iter(tree_view.begin()),
      end(tree_view.end());
  ASSERT_NE(iter, end);
  EXPECT_EQ(iter->name, "parent1");

  iter++;
  ASSERT_NE(iter, end);
  EXPECT_EQ(iter->name, "child3");

  iter++;
  ASSERT_NE(iter, end);
  EXPECT_EQ(iter->name, "child4");

  iter++;
  EXPECT_EQ(iter, end);
}

}  // namespace
}  // namespace verible
