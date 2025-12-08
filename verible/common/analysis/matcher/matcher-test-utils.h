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

#ifndef VERIBLE_COMMON_ANALYSIS_MATCHER_MATCHER_TEST_UTILS_H_
#define VERIBLE_COMMON_ANALYSIS_MATCHER_MATCHER_TEST_UTILS_H_

#include <map>
#include <string>
#include <string_view>

#include "absl/status/status.h"
#include "gtest/gtest.h"
#include "verible/common/analysis/matcher/matcher.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/symbol.h"

namespace verible {
namespace matcher {

// Test case for running a matcher against raw text.
// Expects text to parse correctly.
struct RawMatcherTestCase {
  // Matcher object to test.
  const Matcher matcher;

  // Source text to parse and analyze.
  const std::string code;

  // Number of matches to expect.
  const int num_matches;
};

void ExpectMatchesInAST(const Symbol &tree, const Matcher &matcher,
                        int num_matches, std::string_view code);

// Runs a raw test case. Expects test.code to be correctly parsed by
// analyzer A.
template <class A>
void RunRawMatcherTestCase(const RawMatcherTestCase &test) {
  A analyzer(test.code, "<<inline-test>>");
  absl::Status status = analyzer.Analyze();
  EXPECT_TRUE(status.ok()) << "code with error:\n" << test.code;

  auto *tree = analyzer.SyntaxTree().get();
  EXPECT_TRUE(tree != nullptr);

  ExpectMatchesInAST(*tree, test.matcher, test.num_matches, test.code);
}

// Describes a test case for a matcher
struct MatcherTestCase {
  // Matcher object to test.
  const Matcher matcher;

  // Root of syntax tree.
  const SymbolPtr root;

  // True if expected to match.
  const bool expected_result;

  // Expected bindings in match.
  const std::map<std::string, SymbolTag> expected_bound_nodes;
};

void RunMatcherTestCase(const MatcherTestCase &test);

}  // namespace matcher
}  // namespace verible

#endif  // VERIBLE_COMMON_ANALYSIS_MATCHER_MATCHER_TEST_UTILS_H_
