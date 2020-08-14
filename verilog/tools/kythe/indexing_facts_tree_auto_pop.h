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

#ifndef VERIBLE_VERILOG_TOOLS_KYTHE_INDEXING_FACTS_TREE_AUTO_POP_H_
#define VERIBLE_VERILOG_TOOLS_KYTHE_INDEXING_FACTS_TREE_AUTO_POP_H_

#include <vector>

#include "verilog/tools/kythe/indexing_facts_tree.h"

namespace verilog {
namespace kythe {

// TODO(fangism): move/refactor to common/util
namespace {

template <class V, class T>
class AutoPopBack {
 public:
  AutoPopBack(V* v, T* t) : vec_(v) { vec_->push_back(std::move(t)); }
  AutoPopBack(V* v, const T& t) : vec_(v) { vec_->push_back(t); }
  ~AutoPopBack() { vec_->pop_back(); }

 private:
  V* vec_;
};

}  // namespace

}  // namespace kythe
}  // namespace verilog

#endif  // VERIBLE_VERILOG_TOOLS_KYTHE_INDEXING_FACTS_TREE_AUTO_POP_H_
