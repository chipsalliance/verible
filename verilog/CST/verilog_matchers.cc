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

#include "verilog/CST/verilog_matchers.h"

#include "common/analysis/matcher/matcher_builders.h"
#include "common/text/symbol.h"
#include "verilog/CST/verilog_nonterminals.h"

namespace verilog {

#define N(tag) verible::NodeTag(NodeEnum::tag)

// Define every syntax tree node matcher object.
#define CONSIDER(tag) const NodeMatcher<NodeEnum::tag> Node##tag;
#include "verilog/CST/verilog_nonterminals_foreach.inc"  // IWYU pragma: keep
#undef CONSIDER

// Define every single-node path matcher object.
#define CONSIDER(tag)                                     \
  const verible::matcher::PathMatchBuilder<1> Path##tag = \
      verible::matcher::MakePathMatcher({N(tag)});
#include "verilog/CST/verilog_nonterminals_foreach.inc"  // IWYU pragma: keep
#undef CONSIDER

}  // namespace verilog
