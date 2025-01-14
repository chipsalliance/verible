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

#include "verible/verilog/CST/match-test-utils.h"

#include <functional>
#include <sstream>
#include <string_view>
#include <vector>

#include "gtest/gtest.h"
#include "verible/common/analysis/syntax-tree-search-test-utils.h"
#include "verible/common/analysis/syntax-tree-search.h"
#include "verible/common/text/text-structure.h"
#include "verible/verilog/analysis/verilog-analyzer.h"

#undef ASSERT_OK
#define ASSERT_OK(value) ASSERT_TRUE((value).ok())

namespace verilog {

using verible::SyntaxTreeSearchTestCase;
using verible::TextStructureView;
using verible::TreeSearchMatch;

void TestVerilogSyntaxRangeMatches(
    std::string_view test_name, const SyntaxTreeSearchTestCase &test_case,
    const std::function<std::vector<TreeSearchMatch>(const TextStructureView &)>
        &match_collector) {
  const std::string_view code(test_case.code);
  // Parse Verilog source code into syntax tree.
  VerilogAnalyzer analyzer(code, "test-file");
  const TextStructureView &text_structure(analyzer.Data());
  const std::string_view code_copy = text_structure.Contents();
  ASSERT_OK(analyzer.Analyze()) << test_name << " failed on:\n" << code;

  // Run the match collector to gather results.
  const std::vector<TreeSearchMatch> matches(match_collector(text_structure));

  // Evaluate set-difference of findings.
  std::ostringstream diffs;
  EXPECT_TRUE(test_case.ExactMatchFindings(matches, code_copy, &diffs))
      << test_name << " failed on:\n"
      << code << "\ndiffs:\n"
      << diffs.str();
}

}  // namespace verilog
