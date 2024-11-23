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

#ifndef VERIBLE_VERILOG_CST_MATCH_TEST_UTILS_H_
#define VERIBLE_VERILOG_CST_MATCH_TEST_UTILS_H_

#include <functional>
#include <vector>

#include "absl/strings/string_view.h"
#include "verible/common/analysis/syntax-tree-search-test-utils.h"
#include "verible/common/analysis/syntax-tree-search.h"
#include "verible/common/text/text-structure.h"

namespace verilog {

// Parses Verilog source into a syntax tree, runs 'match_collector' to collect
// findings, and set-compares the findings against those expected from the
// 'test_case'.
// Test will terminate early if there are lexical or syntax errors.
void TestVerilogSyntaxRangeMatches(
    absl::string_view test_name,
    const verible::SyntaxTreeSearchTestCase &test_case,
    const std::function<std::vector<verible::TreeSearchMatch>(
        const verible::TextStructureView &)> &match_collector);

}  // namespace verilog

#endif  // VERIBLE_VERILOG_CST_MATCH_TEST_UTILS_H_
