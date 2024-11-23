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

#include "verible/common/text/tree-context-visitor.h"

#include <vector>

#include "verible/common/strings/display-utils.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/syntax-tree-context.h"
#include "verible/common/util/logging.h"

namespace verible {

void TreeContextVisitor::Visit(const SyntaxTreeNode &node) {
  const SyntaxTreeContext::AutoPop p(&current_context_, &node);
  for (const auto &child : node.children()) {
    if (child) child->Accept(this);
  }
}

namespace {
template <class V>
class AutoPopBack {
 public:
  explicit AutoPopBack(V *v) : vec_(v) { vec_->push_back(0); }
  ~AutoPopBack() { vec_->pop_back(); }

 private:
  V *vec_;
};
}  // namespace

void TreeContextPathVisitor::Visit(const SyntaxTreeNode &node) {
  const SyntaxTreeContext::AutoPop c(&current_context_, &node);
  const AutoPopBack<SyntaxTreePath> p(&current_path_);
  for (const auto &child : node.children()) {
    if (child) child->Accept(this);
    ++current_path_.back();
  }
}

SequenceStreamFormatter<SyntaxTreePath> TreePathFormatter(
    const SyntaxTreePath &path) {
  return SequenceFormatter(path, ",", "[", "]");
}

SyntaxTreePath NextSiblingPath(const SyntaxTreePath &path) {
  CHECK(!path.empty());
  auto next = path;
  ++next.back();
  return next;
}

static int CompareSyntaxTreePath(const SyntaxTreePath &a,
                                 const SyntaxTreePath &b, int index) {
  // a[index] ? b[index]
  if (int(a.size()) > index && int(b.size()) > index) {
    if (a[index] < b[index]) return -1;
    if (a[index] > b[index]) return 1;
    if (a[index] == b[index]) return CompareSyntaxTreePath(a, b, index + 1);
  }
  // a[index] ? (out-of-bounds)
  if (int(a.size()) > index) return (a[index] < 0) ? -1 : 1;
  // (out-of-bounds) ? b[index]
  if (int(b.size()) > index) return (0 > b[index]) ? 1 : -1;
  // (out-of-bounds) == (out-of-bounds)
  return 0;
}

int CompareSyntaxTreePath(const SyntaxTreePath &a, const SyntaxTreePath &b) {
  return CompareSyntaxTreePath(a, b, 0);
}

}  // namespace verible
