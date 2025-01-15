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

#ifndef VERIBLE_COMMON_UTIL_VECTOR_TREE_TEST_UTIL_H_
#define VERIBLE_COMMON_UTIL_VECTOR_TREE_TEST_UTIL_H_

#include <cstddef>
#include <iosfwd>
#include <string_view>
#include <vector>

#include "verible/common/util/tree-operations.h"
#include "verible/common/util/vector-tree.h"

namespace verible {
namespace testing {

// Just a test struct for instantiating the VectorTree class template.
struct NamedInterval {
  int left;
  int right;
  std::string_view name;

  NamedInterval(int l, int r, std::string_view n)
      : left(l), right(r), name(n) {}

  NamedInterval(const NamedInterval &) = default;
  NamedInterval(NamedInterval &&) = default;
  NamedInterval &operator=(const NamedInterval &) = default;
  NamedInterval &operator=(NamedInterval &&) = default;

  bool operator==(const NamedInterval &other) const {
    return left == other.left && right == other.right && name == other.name;
  }
};

std::ostream &operator<<(std::ostream &stream, const NamedInterval &i);

using VectorTreeTestType = VectorTree<NamedInterval>;

VectorTreeTestType MakeRootOnlyExampleTree();

VectorTreeTestType MakeOneChildPolicyExampleTree();

VectorTreeTestType MakeExampleFamilyTree();

template <class T>
std::vector<size_t> MakePath(const VectorTree<T> &node) {
  std::vector<size_t> path;
  verible::Path(node, path);
  return path;
}

void IntervalPrinter(std::ostream *stream, const NamedInterval &interval);

// Verify the invariant that parent spans same interval range as children.
void VerifyInterval(const VectorTreeTestType &node);

}  // namespace testing
}  // namespace verible

#endif  // VERIBLE_COMMON_UTIL_VECTOR_TREE_TEST_UTIL_H_
