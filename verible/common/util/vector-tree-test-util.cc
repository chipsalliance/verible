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

#include "verible/common/util/vector-tree-test-util.h"

#include <iostream>

#include "gtest/gtest.h"
#include "verible/common/util/vector-tree.h"

namespace verible {
namespace testing {

std::ostream &operator<<(std::ostream &stream, const NamedInterval &i) {
  return stream << '(' << i.left << ", " << i.right << ", " << i.name << ')';
}

using VectorTreeTestType = VectorTree<NamedInterval>;

// Convenient type alias for constructing test trees.
// Calling this like a function actually invokes the recursive constructor
// and builds entire trees using recursive initializer lists.
using MakeTree = VectorTreeTestType;

VectorTreeTestType MakeRootOnlyExampleTree() {
  return MakeTree({0, 2, "root"});
}

VectorTreeTestType MakeOneChildPolicyExampleTree() {
  return MakeTree({0, 3, "root"},                    // wrap me
                  MakeTree({0, 3, "gen1"},           // wrap me
                           MakeTree({0, 3, "gen2"})  // wrap me
                           ));
}

VectorTreeTestType MakeExampleFamilyTree() {
  return MakeTree({0, 4, "grandparent"},                 // wrap me
                  MakeTree({0, 2, "parent1"},            // wrap me
                           MakeTree({0, 1, "child1"}),   // wrap me
                           MakeTree({1, 2, "child2"})),  // wrap me
                  MakeTree({2, 4, "parent2"},            // wrap me
                           MakeTree({2, 3, "child3"}),   // wrap me
                           MakeTree({3, 4, "child4"}))   // wrap me
  );
}

void IntervalPrinter(std::ostream *stream, const NamedInterval &interval) {
  *stream << interval << '\n';
}

// Verify the invariant that parent spans same interval range as children.
void VerifyInterval(const VectorTreeTestType &node) {
  const auto &children = node.Children();
  if (!children.empty()) {
    const auto &interval = node.Value();
    EXPECT_EQ(interval.left, children.front().Value().left);
    EXPECT_EQ(interval.right, children.back().Value().right);
  }
}

}  // namespace testing
}  // namespace verible
