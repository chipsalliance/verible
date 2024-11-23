// Copyright 2017-2021 The Verible Authors.
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

#include "verible/common/util/vector-tree-iterators.h"

#include <cstddef>
#include <ostream>
#include <sstream>
#include <type_traits>
#include <vector>

#include "absl/strings/str_join.h"
#include "gtest/gtest.h"
#include "verible/common/util/tree-operations.h"
#include "verible/common/util/vector-tree.h"

namespace verible {

// Iterator value printers used by Google Test

template <typename TreeType>
static void PrintTo(const VectorTreeLeavesIterator<TreeType> &it,
                    std::ostream *os) {
  *os << "VectorTreeLeavesIterator(" << *it << ")";
}

template <typename TreeType>
static void PrintTo(const VectorTreePreOrderIterator<TreeType> &it,
                    std::ostream *os) {
  *os << "VectorTreePreOrderIterator(" << *it << ")";
}

template <typename TreeType>
static void PrintTo(const VectorTreePostOrderIterator<TreeType> &it,
                    std::ostream *os) {
  *os << "VectorTreePostOrderIterator(" << *it << ")";
}

namespace {

using Tree = verible::VectorTree<int>;

template <typename Iterator, typename TreeType>
void ExpectForwardIterator(TreeType *node, TreeType *next_node) {
  EXPECT_TRUE(std::is_default_constructible_v<Iterator>);
  EXPECT_TRUE(std::is_copy_constructible_v<Iterator>);
  EXPECT_TRUE(std::is_copy_assignable_v<Iterator>);
  EXPECT_TRUE(std::is_destructible_v<Iterator>);
  EXPECT_TRUE(std::is_swappable_v<Iterator>);

  Iterator node_it(node), next_node_it(next_node);
  Iterator node_it_2(node);
  // i == j
  EXPECT_TRUE(node_it == node_it_2);
  // i != j
  EXPECT_TRUE(node_it != next_node_it);
  // *i
  EXPECT_EQ(&(*node_it), node);
  EXPECT_EQ(&(*next_node_it), next_node);
  // i->member
  EXPECT_EQ(next_node_it->Value(), next_node->Value());
  // ++i
  {
    auto i = node_it;
    EXPECT_EQ(++i, next_node_it);
    EXPECT_EQ(i, next_node_it);
    EXPECT_NE(i, node_it);
  }
  // i++
  {
    auto i = node_it;
    EXPECT_EQ(i++, node_it);
    EXPECT_EQ(i, next_node_it);
    EXPECT_NE(i, node_it);
  }
  // *i++
  {
    auto i = node_it;
    EXPECT_EQ(&(*i++), node);
    EXPECT_EQ(i, next_node_it);
    EXPECT_NE(i, node_it);
  }

  if constexpr (!std::is_const_v<typename Iterator::value_type>) {
    auto tree = Tree(0,        //
                     Tree(1),  //
                     Tree(2));
    auto other_tree = Tree(200,         //
                           Tree(2001),  //
                           Tree(2002));
    auto it = Iterator(&tree.Children()[1]);
    // *i = j
    *it = other_tree;
    EXPECT_EQ(tree.Children()[1].Value(), 200);
  }
}

TEST(VectorTreeIteratorTest, ForwardIteratorInterface) {
  auto tree = Tree(0,               //
                   Tree(1),         //
                   Tree(2,          //
                        Tree(21),   //
                        Tree(22),   //
                        Tree(23)),  //
                   Tree(3));

  ExpectForwardIterator<VectorTreeLeavesIterator<Tree>>(
      &tree.Children()[0], &tree.Children()[1].Children()[0]);
  ExpectForwardIterator<VectorTreeLeavesIterator<const Tree>>(
      &tree.Children()[0], &tree.Children()[1].Children()[0]);

  ExpectForwardIterator<VectorTreePreOrderIterator<Tree>>(&tree.Children()[0],
                                                          &tree.Children()[1]);
  ExpectForwardIterator<VectorTreePreOrderIterator<const Tree>>(
      &tree.Children()[0], &tree.Children()[1]);

  ExpectForwardIterator<VectorTreePostOrderIterator<Tree>>(
      &tree.Children()[0], &tree.Children()[1].Children()[0]);
  ExpectForwardIterator<VectorTreePostOrderIterator<const Tree>>(
      &tree.Children()[0], &tree.Children()[1].Children()[0]);
}

struct TestCaseData {
  Tree tree;

  // RootNodeTraversal test data
  struct {
    std::vector<int> expected_sequence_leaves;
    std::vector<int> expected_sequence_pre_order;
    std::vector<int> expected_sequence_post_order;
  } root_node_traversal;

  // SubtreeTraversal and IteratorSubtreeTraversal test data
  struct {
    std::vector<size_t> subtree_path;
    std::vector<int> expected_sequence_leaves;
    std::vector<int> expected_sequence_pre_order;
    std::vector<int> expected_sequence_post_order;
  } subtree_traversal;
};

static const TestCaseData kTestCasesData[] = {
    {
        Tree(0),
        // RootNodeTraversal test data
        {
            {0},
            {0},
            {0},
        },
        // SubtreeTraversal and IteratorSubtreeTraversal test data (skipped)
        {},
    },
    {
        Tree(0,        //
             Tree(1),  //
             Tree(2),  //
             Tree(3)),
        // RootNodeTraversal test data
        {
            {1, 2, 3},
            {0, 1, 2, 3},
            {1, 2, 3, 0},
        },
        // SubtreeTraversal and IteratorSubtreeTraversal test data
        {
            {0},  // subtree path
            {1},
            {1},
            {1},
        },
    },
    {
        Tree(0,                   //
             Tree(1,              //
                  Tree(11,        //
                       Tree(111,  //
                            Tree(1111))))),
        // RootNodeTraversal test data
        {
            {1111},
            {0, 1, 11, 111, 1111},
            {1111, 111, 11, 1, 0},
        },
        // SubtreeTraversal and IteratorSubtreeTraversal test data
        {
            {0, 0},  // subtree path
            {1111},
            {11, 111, 1111},
            {1111, 111, 11},
        },
    },
    {
        Tree(0,                      //
             Tree(1,                 //
                  Tree(11,           //
                       Tree(111),    //
                       Tree(112)),   //
                  Tree(12),          //
                  Tree(13)),         //
             Tree(2,                 //
                  Tree(21),          //
                  Tree(22),          //
                  Tree(23,           //
                       Tree(231),    //
                       Tree(232))),  //
             Tree(3)),
        // RootNodeTraversal test data
        {
            {111, 112, 12, 13, 21, 22, 231, 232, 3},
            {0, 1, 11, 111, 112, 12, 13, 2, 21, 22, 23, 231, 232, 3},
            {111, 112, 11, 12, 13, 1, 21, 22, 231, 232, 23, 2, 3, 0},
        },
        // SubtreeTraversal and IteratorSubtreeTraversal test data
        {
            {0},  // subtree path
            {111, 112, 12, 13},
            {1, 11, 111, 112, 12, 13},
            {111, 112, 11, 12, 13, 1},
        },
    },
    {
        Tree(0,                     //
             Tree(1),               //
             Tree(2,                //
                  Tree(21,          //
                       Tree(211),   //
                       Tree(212)),  //
                  Tree(22),         //
                  Tree(23)),        //
             Tree(3,                //
                  Tree(31),         //
                  Tree(32),         //
                  Tree(33,          //
                       Tree(331),   //
                       Tree(332)))),
        // RootNodeTraversal test data
        {
            {1, 211, 212, 22, 23, 31, 32, 331, 332},
            {0, 1, 2, 21, 211, 212, 22, 23, 3, 31, 32, 33, 331, 332},
            {1, 211, 212, 21, 22, 23, 2, 31, 32, 331, 332, 33, 3, 0},
        },
        // SubtreeTraversal and IteratorSubtreeTraversal test data
        {
            {2},  // subtree path
            {31, 32, 331, 332},
            {3, 31, 32, 33, 331, 332},
            {31, 32, 331, 332, 33, 3},
        },
    },
};

template <typename NodesRange, typename ValuesRange>
void ExpectNodesRangesValuesEq(const NodesRange &nodes,
                               const ValuesRange &expected_values) {
  auto expected_it = expected_values.begin();

  for (const auto &node : nodes) {
    EXPECT_NE(expected_it, expected_values.end());
    EXPECT_EQ(node.Value(), *expected_it);
    ++expected_it;
  }
  EXPECT_EQ(expected_it, expected_values.end());
}

TEST(VectorTreeIteratorTest, RootNodeTraversal) {
  for (const auto &data : kTestCasesData) {
    std::ostringstream trace_msg;
    trace_msg << "Input tree:\n" << data.tree;
    SCOPED_TRACE(trace_msg.str());

    {
      SCOPED_TRACE("VectorTreeLeavesTraversal");
      ExpectNodesRangesValuesEq(
          VectorTreeLeavesTraversal(data.tree),
          data.root_node_traversal.expected_sequence_leaves);
    }
    {
      SCOPED_TRACE("VectorTreePreOrderTraversal");
      ExpectNodesRangesValuesEq(
          VectorTreePreOrderTraversal(data.tree),
          data.root_node_traversal.expected_sequence_pre_order);
    }
    {
      SCOPED_TRACE("VectorTreePostOrderTraversal");
      ExpectNodesRangesValuesEq(
          VectorTreePostOrderTraversal(data.tree),
          data.root_node_traversal.expected_sequence_post_order);
    }
  }
}

TEST(VectorTreeIteratorTest, SubtreeTraversal) {
  for (const auto &data : kTestCasesData) {
    const auto subtree_path = data.subtree_traversal.subtree_path;
    if (subtree_path.empty()) continue;

    const auto &subtree =
        DescendPath(data.tree, subtree_path.begin(), subtree_path.end());

    std::ostringstream trace_msg;
    trace_msg << "Input tree:\n"
              << data.tree
              << "\nSubtree path: " << absl::StrJoin(subtree_path, ".");
    SCOPED_TRACE(trace_msg.str());

    {
      SCOPED_TRACE("VectorTreeLeavesTraversal");
      ExpectNodesRangesValuesEq(
          VectorTreeLeavesTraversal(subtree),
          data.subtree_traversal.expected_sequence_leaves);
    }
    {
      SCOPED_TRACE("VectorTreePreOrderTraversal");
      ExpectNodesRangesValuesEq(
          VectorTreePreOrderTraversal(subtree),
          data.subtree_traversal.expected_sequence_pre_order);
    }
    {
      SCOPED_TRACE("VectorTreePostOrderTraversal");
      ExpectNodesRangesValuesEq(
          VectorTreePostOrderTraversal(subtree),
          data.subtree_traversal.expected_sequence_post_order);
    }
  }
}

TEST(VectorTreeIteratorTest, IteratorSubtreeTraversal) {
  for (const auto &data : kTestCasesData) {
    const auto subtree_path = data.subtree_traversal.subtree_path;
    if (subtree_path.empty()) continue;

    const auto &subtree =
        DescendPath(data.tree, subtree_path.begin(), subtree_path.end());

    std::ostringstream trace_msg;
    trace_msg << "Input tree:\n"
              << data.tree
              << "\nSubtree path: " << absl::StrJoin(subtree_path, ".");
    SCOPED_TRACE(trace_msg.str());

    // VectorTreeLeavesIterator doesn't support subranges
    {
      SCOPED_TRACE("VectorTreePreOrderIterator");
      ExpectNodesRangesValuesEq(
          VectorTreePreOrderIterator(&subtree),
          data.subtree_traversal.expected_sequence_pre_order);
    }
    {
      SCOPED_TRACE("VectorTreePostOrderIterator");
      ExpectNodesRangesValuesEq(
          VectorTreePostOrderIterator(&subtree),
          data.subtree_traversal.expected_sequence_post_order);
    }
  }
}

}  // namespace
}  // namespace verible
