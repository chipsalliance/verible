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

#include "common/text/syntax_tree_context.h"

#include "common/text/concrete_syntax_tree.h"
#include "common/util/logging.h"

namespace verible {

const SyntaxTreeNode& SyntaxTreeContext::top() const {
  CHECK(!stack_.empty());
  return *ABSL_DIE_IF_NULL(stack_.back());
}

SyntaxTreeContext::AutoPop::AutoPop(SyntaxTreeContext* context,
                                    const SyntaxTreeNode& node) {
  context_ = context;
  context->Push(node);
}

SyntaxTreeContext::AutoPop::~AutoPop() { context_->Pop(); }

void SyntaxTreeContext::Pop() {
  CHECK(!stack_.empty());
  stack_.pop_back();
}

// Push a SyntaxTreeNode onto the stack
void SyntaxTreeContext::Push(const verible::SyntaxTreeNode& node) {
  stack_.push_back(&node);
}

}  // namespace verible
