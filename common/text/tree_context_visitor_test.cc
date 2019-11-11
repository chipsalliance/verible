// Copyright 2017-2019 The Verible Authors.
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

#include "common/text/tree_context_visitor.h"

#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "common/text/tree_builder_test_util.h"

namespace verible {
namespace {

// Test class demonstrating visitation and context tracking
class RecordingVisitor : public TreeContextVisitor {
 public:
  void Visit(const SyntaxTreeLeaf& leaf) override {
    context_history_.push_back(Context());
  }

  void Visit(const SyntaxTreeNode& node) override {
    context_history_.push_back(Context());
    TreeContextVisitor::Visit(node);
  }

  static std::vector<int> ContextToTags(const SyntaxTreeContext& context) {
    std::vector<int> values;
    for (const auto& ancestor : context) {
      values.push_back(ancestor->Tag().tag);
    }
    return values;
  }

  std::vector<std::vector<int>> ContextTagHistory() const {
    std::vector<std::vector<int>> result;
    for (const auto& context : context_history_) {
      result.emplace_back(ContextToTags(context));
    }
    return result;
  }

 private:
  std::vector<SyntaxTreeContext> context_history_;
};

TEST(TreeContextVisitorTest, LoneNode) {
  auto tree = Node();
  RecordingVisitor r;
  tree->Accept(&r);
  const std::vector<std::vector<int>> expect = {
      {},  // the one-and-only node has no parent
  };
  EXPECT_EQ(r.ContextTagHistory(), expect);
}

TEST(TreeContextVisitorTest, ThinTree) {
  auto tree = TNode(3, TNode(4, TNode(5)));
  RecordingVisitor r;
  tree->Accept(&r);
  const std::vector<std::vector<int>> expect = {
      {},
      {3},
      {3, 4},
  };
  EXPECT_EQ(r.ContextTagHistory(), expect);
}

TEST(TreeContextVisitorTest, ThinTreeWithLeaf) {
  auto tree = TNode(3, TNode(4, TNode(5, XLeaf(1))));
  RecordingVisitor r;
  tree->Accept(&r);
  const std::vector<std::vector<int>> expect = {
      {},
      {3},
      {3, 4},
      {3, 4, 5},
  };
  EXPECT_EQ(r.ContextTagHistory(), expect);
}

TEST(TreeContextVisitorTest, FlatTree) {
  auto tree = TNode(3, TNode(4), XLeaf(5), TNode(6));
  RecordingVisitor r;
  tree->Accept(&r);
  const std::vector<std::vector<int>> expect = {
      {},
      {3},
      {3},
      {3},
  };
  EXPECT_EQ(r.ContextTagHistory(), expect);
}

TEST(TreeContextVisitorTest, FullTree) {
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
  const std::vector<std::vector<int>> expect = {
      {},  {3}, {3, 4}, {3, 4},    {3, 4, 1},    {3, 4, 1},
      {3}, {3}, {3, 6}, {3, 6, 2}, {3, 6, 2, 7}, {3, 6, 2},
  };
  EXPECT_EQ(r.ContextTagHistory(), expect);
}

}  // namespace
}  // namespace verible
