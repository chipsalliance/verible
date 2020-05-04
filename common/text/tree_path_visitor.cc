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

#include "common/text/concrete_syntax_tree.h"

namespace verible {

namespace {
template <class V>
class AutoPopBack {
 public:
  explicit AutoPopBack(V* v) : vec_(v) { vec_->push_back(0); }
  ~AutoPopBack() { vec_->pop_back(); }

 private:
  V* vec_;
};
}  // namespace

void TreePathVisitor::Visit(const SyntaxTreeNode& node) {
  const AutoPopBack<SyntaxTreePath> p(&current_path_);
  for (const auto& child : node.children()) {
    if (child) child->Accept(this);
    ++current_path_.back();
  }
}

}  // namespace verible
