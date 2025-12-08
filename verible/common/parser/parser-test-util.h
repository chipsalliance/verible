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

// Utility functions for parser testing.

#ifndef VERIBLE_COMMON_PARSER_PARSER_TEST_UTIL_H_
#define VERIBLE_COMMON_PARSER_PARSER_TEST_UTIL_H_

#include <string>  // for string
#include <string_view>
#include <vector>

#include "absl/status/status.h"
#include "gtest/gtest.h"
#include "verible/common/analysis/matcher/descent-path.h"
#include "verible/common/text/parser-verifier.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/token-info-test-util.h"
#include "verible/common/util/logging.h"

namespace verible {

// Tests the parser on source text that is valid input.
// class AnalyzerType is any class with a absl::Status AnalyzerType::Analyze()
// method.
template <class AnalyzerType>
void TestParserAcceptValid(std::string_view code, int i) {
  VLOG(1) << "test_data[" << i << "] = '" << code << "'\n";
  AnalyzerType analyzer(code, "<<inline-test>>");
  absl::Status status = analyzer.Analyze();
  if (!status.ok()) {
    // Print more detailed error message.
    const auto &rejected_tokens = analyzer.GetRejectedTokens();
    if (!rejected_tokens.empty()) {
      EXPECT_TRUE(status.ok())
          << "Rejected valid code:\n"
          << code << "\nRejected token: " << rejected_tokens[0].token_info;
    } else {
      EXPECT_TRUE(status.ok()) << "Rejected valid code:\n" << code;
    }
  }
  EXPECT_TRUE(analyzer.SyntaxTree().get()) << "Missing tree on code:\n" << code;
}

// Tests the parser on source text that is invalid input.
// class AnalyzerType is any class with a absl::Status AnalyzerType::Analyze()
// method.
template <class AnalyzerType>
void TestParserRejectInvalid(const TokenInfoTestData &test, int i) {
  VLOG(1) << "test_data[" << i << "] = '" << test.code << "'\n";
  ASSERT_FALSE(test.expected_tokens.empty());
  // Find the first not-don't-care token.
  int iteration = 0;
  do {
    AnalyzerType analyzer(test.code, "<<inline-test>>");  // copies code
    const absl::Status status = analyzer.Analyze();
    EXPECT_FALSE(status.ok())
        << "Accepted invalid code (iteration: " << iteration << "):\n"
        << test.code;
    const auto &rejected_tokens = analyzer.GetRejectedTokens();
    ASSERT_FALSE(rejected_tokens.empty());

    const std::string_view base_text = analyzer.Data().Contents();
    const auto expected_error_tokens = test.FindImportantTokens(base_text);
    ASSERT_FALSE(expected_error_tokens.empty());
    // Only check the first rejected token, ignore the rest.
    const auto &expected_error_token = expected_error_tokens.front();
    EXPECT_EQ(expected_error_token, rejected_tokens[0].token_info);
    ++iteration;
    // Run the analyzer a second time to make sure the parser cleared
    // away any state, and finds the same error.
  } while (iteration < 2);
}

struct ErrorRecoveryTestCase {
  // Code containing a syntax error.
  std::string code;
  // Node path that is expected to exist due to error-recovery.
  // TODO(b/64093049): generalize to use AST matcher classes.
  matcher::DescentPath tree_path;
};

template <class AnalyzerType>
void TestParserErrorRecovered(const ErrorRecoveryTestCase &test, int i) {
  VLOG(1) << "test_data[" << i << "] = '" << test.code << "'\n";
  int iteration = 0;
  do {
    AnalyzerType analyzer(test.code, "<<inline-test>>");
    absl::Status status = analyzer.Analyze();
    EXPECT_FALSE(status.ok())
        << "Accepted invalid code (iteration: " << iteration << "):\n"
        << test.code;
    const auto rejected_tokens = analyzer.GetRejectedTokens();
    EXPECT_FALSE(rejected_tokens.empty());
    // Only check the first rejected token, ignore the rest.
    const auto &tree = analyzer.SyntaxTree();
    const auto matching_paths = matcher::GetAllDescendantsFromPath(
        *ABSL_DIE_IF_NULL(tree), test.tree_path);
    EXPECT_FALSE(matching_paths.empty())
        << "Expected tree path not found.  code:\n"
        << test.code;
    ++iteration;
    // Run the analyzer a second time to make sure the parser cleared
    // away any state, and produces the same result.
  } while (iteration < 2);
}

template <class AnalyzerType>
void TestParserAllMatched(std::string_view code, int i) {
  VLOG(1) << "test_data[" << i << "] = '" << code << "'\n";

  AnalyzerType analyzer(code, "<<inline-test>>");
  absl::Status status = analyzer.Analyze();
  EXPECT_TRUE(status.ok()) << status.message() << "\nRejected: "
                           << analyzer.GetRejectedTokens().front().token_info;

  const Symbol *tree_ptr = analyzer.SyntaxTree().get();
  EXPECT_NE(tree_ptr, nullptr) << "Missing syntax tree with input:\n" << code;
  if (tree_ptr == nullptr) return;  // Already failed, abort this test case.
  const Symbol &root = *tree_ptr;

  ParserVerifier verifier(root, analyzer.Data().GetTokenStreamView());
  const auto unmatched = verifier.Verify();

  EXPECT_EQ(unmatched.size(), 0)
      << "On code:\n"
      << code << "\nFirst unmatched token: " << unmatched.front();
}

}  // namespace verible

#endif  // VERIBLE_COMMON_PARSER_PARSER_TEST_UTIL_H_
