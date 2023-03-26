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

#include "verilog/analysis/checkers/forbid_negative_array_dim.h"

#include <initializer_list>

#include "common/analysis/linter_test_utils.h"
#include "common/analysis/syntax_tree_linter_test_utils.h"
#include "common/text/symbol.h"
#include "gtest/gtest.h"
#include "verilog/CST/verilog_nonterminals.h"
#include "verilog/analysis/verilog_analyzer.h"
#include "verilog/parser/verilog_token_enum.h"

namespace verilog {
namespace analysis {
namespace {

using verible::LintTestCase;
using verible::RunLintTestCases;

TEST(ForbidNegativeArrayDim, ForbidNegativeArrayDims) {
  constexpr int kScalar = TK_OTHER;
  const std::initializer_list<LintTestCase> kTestCases = {
      {""},
      {"logic l [", {kScalar, "-10"}, ":0];"},
      {"logic [", {kScalar, "-10"}, ":-0] l;"},
      {"logic l [0:", {kScalar, "-10"}, "];"},
      {"logic l [+1:0];"},
      {"logic l [-p:0];"},
      {"logic l [10+(-5):0];"},
      {"logic l [(", {kScalar, "-5"}, "):0];"},
      {"logic l [-(-5):0];"},
  };
  RunLintTestCases<VerilogAnalyzer, ForbidNegativeArrayDim>(kTestCases);
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
