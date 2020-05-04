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

#include "common/text/tree_path_visitor.h"

#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "common/text/tree_builder_test_util.h"

namespace verible {
namespace {

using ::testing::ElementsAreArray;

// Test class demonstrating visitation and path tracking
class RecordingVisitor : public TreePathVisitor {
 public:
  void Visit(const SyntaxTreeLeaf& leaf) override {
    path_history_.push_back(Path());
  }

  void Visit(const SyntaxTreeNode& node) override {
    path_history_.push_back(Path());
    TreePathVisitor::Visit(node);
  }

  const std::vector<SyntaxTreePath>& PathTagHistory() const {
    return path_history_;
  }

 private:
  std::vector<SyntaxTreePath> path_history_;
};

TEST(TreePathVisitorTest, LoneNode) {
  auto tree = Node();
  RecordingVisitor r;
  tree->Accept(&r);
  const std::vector<SyntaxTreePath> expect = {
      {},  // the one-and-only node has no parent
  };
  EXPECT_THAT(r.PathTagHistory(), ElementsAreArray(expect));
}

TEST(TreePathVisitorTest, LoneLeaf) {
  auto tree = XLeaf(0);
  RecordingVisitor r;
  tree->Accept(&r);
  const std::vector<SyntaxTreePath> expect = {
      {},  // the one-and-only leaf has no parent
  };
  EXPECT_THAT(r.PathTagHistory(), ElementsAreArray(expect));
}

TEST(TreePathVisitorTest, NodeWithOnlyNullptrs) {
  auto tree = TNode(1, nullptr, nullptr);
  RecordingVisitor r;
  tree->Accept(&r);
  const std::vector<SyntaxTreePath> expect = {
      {},
  };
  EXPECT_THAT(r.PathTagHistory(), ElementsAreArray(expect));
}

TEST(TreePathVisitorTest, NodeWithSomeNullptrs) {
  auto tree = TNode(1, nullptr, Node(), nullptr, Node());
  RecordingVisitor r;
  tree->Accept(&r);
  const std::vector<SyntaxTreePath> expect = {
      {},
      {1},
      {3},
  };
  EXPECT_THAT(r.PathTagHistory(), ElementsAreArray(expect));
}

TEST(TreePathVisitorTest, NodeWithSomeNullptrs2) {
  auto tree = TNode(1, Node(), Node(), nullptr, Node(), nullptr);
  RecordingVisitor r;
  tree->Accept(&r);
  const std::vector<SyntaxTreePath> expect = {
      {},
      {0},
      {1},
      {3},
  };
  EXPECT_THAT(r.PathTagHistory(), ElementsAreArray(expect));
}

TEST(TreePathVisitorTest, ThinTree) {
  auto tree = TNode(3, TNode(4, TNode(5)));
  RecordingVisitor r;
  tree->Accept(&r);
  const std::vector<SyntaxTreePath> expect = {
      {},
      {0},
      {0, 0},
  };
  EXPECT_THAT(r.PathTagHistory(), ElementsAreArray(expect));
}

TEST(TreePathVisitorTest, ThinTreeWithLeaf) {
  auto tree = TNode(3, TNode(4, TNode(5, XLeaf(1))));
  RecordingVisitor r;
  tree->Accept(&r);
  const std::vector<SyntaxTreePath> expect = {
      {},
      {0},
      {0, 0},
      {0, 0, 0},
  };
  EXPECT_THAT(r.PathTagHistory(), ElementsAreArray(expect));
}

TEST(TreePathVisitorTest, FlatTree) {
  auto tree = TNode(3, TNode(4), XLeaf(5), TNode(6));
  RecordingVisitor r;
  tree->Accept(&r);
  const std::vector<SyntaxTreePath> expect = {
      {},
      {0},
      {1},
      {2},
  };
  EXPECT_THAT(r.PathTagHistory(), ElementsAreArray(expect));
}

TEST(TreePathVisitorTest, FullTree) {
  auto tree = TNode(3,                             //
                    TNode(4,                       //
                          XLeaf(99),               //
                          TNode(1,                 //
                                XLeaf(99),         //
                                XLeaf(0))),        //
                    XLeaf(5),                      //
                    TNode(6,                       //
                          TNode(2,                 //
                                TNode(7,           //
                                      XLeaf(99)),  //
                                TNode(8))));
  RecordingVisitor r;
  tree->Accept(&r);
  const std::vector<SyntaxTreePath> expect = {
      {},  {0}, {0, 0}, {0, 1},    {0, 1, 0},    {0, 1, 1},
      {1}, {2}, {2, 0}, {2, 0, 0}, {2, 0, 0, 0}, {2, 0, 1},
  };
  EXPECT_THAT(r.PathTagHistory(), ElementsAreArray(expect));
}

TEST(TreePathVisitorTest, FullTreeWithNullptrs) {
  auto tree = TNode(3,                             //
                    nullptr,                       //
                    TNode(4,                       //
                          nullptr,                 //
                          XLeaf(99),               //
                          nullptr,                 //
                          TNode(1,                 //
                                XLeaf(99),         //
                                nullptr,           //
                                XLeaf(0))),        //
                    nullptr,                       //
                    XLeaf(5),                      //
                    nullptr,                       //
                    nullptr,                       //
                    TNode(6,                       //
                          nullptr,                 //
                          TNode(2,                 //
                                nullptr,           //
                                TNode(7,           //
                                      nullptr,     //
                                      XLeaf(99)),  //
                                TNode(8))),        //
                    nullptr);
  RecordingVisitor r;
  tree->Accept(&r);
  const std::vector<SyntaxTreePath> expect = {
      {},  {1}, {1, 1}, {1, 3},    {1, 3, 0},    {1, 3, 2},
      {3}, {6}, {6, 1}, {6, 1, 1}, {6, 1, 1, 1}, {6, 1, 2},
  };
  EXPECT_THAT(r.PathTagHistory(), ElementsAreArray(expect));
}

}  // namespace
}  // namespace verible
