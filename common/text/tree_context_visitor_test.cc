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

#include "common/text/tree_context_visitor.h"

#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "common/text/tree_builder_test_util.h"

namespace verible {
namespace {

using ::testing::ElementsAreArray;

static std::vector<int> ContextToTags(const SyntaxTreeContext& context) {
  std::vector<int> values;
  for (const auto& ancestor : context) {
    values.push_back(ancestor->Tag().tag);
  }
  return values;
}

// Test class demonstrating visitation and context tracking
template <class BaseVisitor>
class ContextRecorder : public BaseVisitor {
 public:
  void Visit(const SyntaxTreeLeaf& leaf) override {
    context_history_.push_back(BaseVisitor::Context());
  }

  void Visit(const SyntaxTreeNode& node) override {
    context_history_.push_back(BaseVisitor::Context());
    BaseVisitor::Visit(node);
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

template <class T>
static void TestContextRecorder(const SymbolPtr& tree,
                                const std::vector<std::vector<int>>& expect) {
  ContextRecorder<T> r;
  tree->Accept(&r);
  EXPECT_THAT(r.ContextTagHistory(), ElementsAreArray(expect));
}

// Test all classes that implement the context-tracking interface.
// It is important to test this way because the subclasses may not (or cannot)
// directly use the visitor in the base class, and might have to duplicate
// some functionality.  This ensures proper coverage, regardless of the
// implementation.
static void TestContextRecorders(const SymbolPtr& tree,
                                 const std::vector<std::vector<int>>& expect) {
  TestContextRecorder<TreeContextVisitor>(tree, expect);
  TestContextRecorder<TreeContextPathVisitor>(tree, expect);
}

TEST(TreeContextVisitorTest, LoneNode) {
  auto tree = Node();
  const std::vector<std::vector<int>> expect = {
      {},  // the one-and-only node has no parent
  };
  TestContextRecorders(tree, expect);
}

TEST(TreeContextVisitorTest, ThinTree) {
  auto tree = TNode(3, TNode(4, TNode(5)));
  const std::vector<std::vector<int>> expect = {
      {},
      {3},
      {3, 4},
  };
  TestContextRecorders(tree, expect);
}

TEST(TreeContextVisitorTest, ThinTreeWithLeaf) {
  auto tree = TNode(3, TNode(4, TNode(5, XLeaf(1))));
  const std::vector<std::vector<int>> expect = {
      {},
      {3},
      {3, 4},
      {3, 4, 5},
  };
  TestContextRecorders(tree, expect);
}

TEST(TreeContextVisitorTest, FlatTree) {
  auto tree = TNode(3, TNode(4), XLeaf(5), TNode(6));
  const std::vector<std::vector<int>> expect = {
      {},
      {3},
      {3},
      {3},
  };
  TestContextRecorders(tree, expect);
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
  const std::vector<std::vector<int>> expect = {
      {},  {3}, {3, 4}, {3, 4},    {3, 4, 1},    {3, 4, 1},
      {3}, {3}, {3, 6}, {3, 6, 2}, {3, 6, 2, 7}, {3, 6, 2},
  };
  TestContextRecorders(tree, expect);
}

// Test class demonstrating visitation and path tracking
class PathRecorder : public TreeContextPathVisitor {
 public:
  void Visit(const SyntaxTreeLeaf& leaf) override {
    path_history_.push_back(Path());
  }

  void Visit(const SyntaxTreeNode& node) override {
    path_history_.push_back(Path());
    TreeContextPathVisitor::Visit(node);
  }

  const std::vector<SyntaxTreePath>& PathTagHistory() const {
    return path_history_;
  }

 private:
  std::vector<SyntaxTreePath> path_history_;
};

TEST(TreePathVisitorTest, LoneNode) {
  auto tree = Node();
  const std::vector<SyntaxTreePath> expect = {
      {},  // the one-and-only node has no parent
  };
  PathRecorder r;
  tree->Accept(&r);
  EXPECT_THAT(r.PathTagHistory(), ElementsAreArray(expect));
}

TEST(TreePathVisitorTest, LoneLeaf) {
  auto tree = XLeaf(0);
  const std::vector<SyntaxTreePath> expect = {
      {},  // the one-and-only leaf has no parent
  };
  PathRecorder r;
  tree->Accept(&r);
  EXPECT_THAT(r.PathTagHistory(), ElementsAreArray(expect));
}

TEST(TreePathVisitorTest, NodeWithOnlyNullptrs) {
  auto tree = TNode(1, nullptr, nullptr);
  const std::vector<SyntaxTreePath> expect = {
      {},
  };
  PathRecorder r;
  tree->Accept(&r);
  EXPECT_THAT(r.PathTagHistory(), ElementsAreArray(expect));
}

TEST(TreePathVisitorTest, NodeWithSomeNullptrs) {
  auto tree = TNode(1, nullptr, Node(), nullptr, Node());
  const std::vector<SyntaxTreePath> expect = {
      {},
      {1},
      {3},
  };
  PathRecorder r;
  tree->Accept(&r);
  EXPECT_THAT(r.PathTagHistory(), ElementsAreArray(expect));
}

TEST(TreePathVisitorTest, NodeWithSomeNullptrs2) {
  auto tree = TNode(1, Node(), Node(), nullptr, Node(), nullptr);
  const std::vector<SyntaxTreePath> expect = {
      {},
      {0},
      {1},
      {3},
  };
  PathRecorder r;
  tree->Accept(&r);
  EXPECT_THAT(r.PathTagHistory(), ElementsAreArray(expect));
}

TEST(TreePathVisitorTest, ThinTree) {
  auto tree = TNode(3, TNode(4, TNode(5)));
  const std::vector<SyntaxTreePath> expect = {
      {},
      {0},
      {0, 0},
  };
  PathRecorder r;
  tree->Accept(&r);
  EXPECT_THAT(r.PathTagHistory(), ElementsAreArray(expect));
}

TEST(TreePathVisitorTest, ThinTreeWithLeaf) {
  auto tree = TNode(3, TNode(4, TNode(5, XLeaf(1))));
  const std::vector<SyntaxTreePath> expect = {
      {},
      {0},
      {0, 0},
      {0, 0, 0},
  };
  PathRecorder r;
  tree->Accept(&r);
  EXPECT_THAT(r.PathTagHistory(), ElementsAreArray(expect));
}

TEST(TreePathVisitorTest, FlatTree) {
  auto tree = TNode(3, TNode(4), XLeaf(5), TNode(6));
  const std::vector<SyntaxTreePath> expect = {
      {},
      {0},
      {1},
      {2},
  };
  PathRecorder r;
  tree->Accept(&r);
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
  const std::vector<SyntaxTreePath> expect = {
      {},  {0}, {0, 0}, {0, 1},    {0, 1, 0},    {0, 1, 1},
      {1}, {2}, {2, 0}, {2, 0, 0}, {2, 0, 0, 0}, {2, 0, 1},
  };
  PathRecorder r;
  tree->Accept(&r);
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
  const std::vector<SyntaxTreePath> expect = {
      {},  {1}, {1, 1}, {1, 3},    {1, 3, 0},    {1, 3, 2},
      {3}, {6}, {6, 1}, {6, 1, 1}, {6, 1, 1, 1}, {6, 1, 2},
  };
  PathRecorder r;
  tree->Accept(&r);
  EXPECT_THAT(r.PathTagHistory(), ElementsAreArray(expect));
}

}  // namespace
}  // namespace verible
