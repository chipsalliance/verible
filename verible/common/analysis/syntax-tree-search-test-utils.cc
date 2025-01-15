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

#include "verible/common/analysis/syntax-tree-search-test-utils.h"

#include <iostream>
#include <iterator>
#include <set>
#include <string_view>
#include <vector>

#include "verible/common/analysis/syntax-tree-search.h"
#include "verible/common/text/token-info.h"
#include "verible/common/text/tree-utils.h"
#include "verible/common/util/algorithm.h"
#include "verible/common/util/logging.h"

namespace verible {

// Returns 0 if strings' ranges are equal,
// negative if left is 'less' than right,
// positive if left is 'greater' than right.
// Compares lower bounds, and then upper bounds.
static int CompareStringRanges(std::string_view left, std::string_view right) {
  // tuple-compare the bounds of the string_view ranges (lexicographical)
  {
    const int delta = std::distance(right.begin(), left.begin());
    if (delta != 0) {
      return delta;
    }
  }
  {
    const int delta = std::distance(right.end(), left.end());
    if (delta != 0) {
      return delta;
    }
  }
  // string_views point into same buffer, and thus should have equal contents
  CHECK_EQ(left, right);
  return 0;
}

struct LessStringRanges {
  bool operator()(std::string_view left, std::string_view right) const {
    return CompareStringRanges(left, right) < 0;
  }
};

using StringRangeSet = std::set<std::string_view, LessStringRanges>;

// This function helps find symmetric differences between two sets
// of findings (actual vs. expected) based on locations.
static int CompareFindingLocation(std::string_view lhs, const TokenInfo &rhs) {
  const int delta = CompareStringRanges(lhs, rhs.text());
  // Then compare enums, where we only care about equality.
  return delta;
}

// TODO(b/151371397): refactor this for re-use for multi-findings style tests.
bool SyntaxTreeSearchTestCase::ExactMatchFindings(
    const std::vector<TreeSearchMatch> &actual_findings, std::string_view base,
    std::ostream *diffstream) const {
  // Convert actual_findings into string ranges.  Ignore matches' context.
  StringRangeSet actual_findings_ranges;
  for (const auto &finding : actual_findings) {
    if (finding.match == nullptr) continue;
    const auto &match_symbol(*finding.match);
    const std::string_view spanned_text = StringSpanOfSymbol(match_symbol);
    // Spanned text can be empty when a subtree is devoid of leaves.
    if (spanned_text.empty()) continue;
    actual_findings_ranges.insert(spanned_text);
  }

  // Due to the order in which findings are visited, we can assert that
  // the reported findings are thus ordered.
  // TODO(fangism): It wouldn't be too expensive to verify this assertion.
  // The expected findings are also ordered by construction.
  const auto expected_findings = FindImportantTokens(base);
  // Thus, we can use an algorithm like std::set_symmetric_difference().

  // These containers will catch unmatched differences found.
  std::vector<std::string_view> unmatched_actual_findings;
  std::vector<TokenInfo> unmatched_expected_findings;

  set_symmetric_difference_split(
      actual_findings_ranges.begin(), actual_findings_ranges.end(),
      expected_findings.begin(), expected_findings.end(),
      std::back_inserter(unmatched_actual_findings),
      std::back_inserter(unmatched_expected_findings), &CompareFindingLocation);

  bool all_match = true;
  const TokenInfo::Context context(base);
  if (!unmatched_actual_findings.empty()) {
    all_match = false;
    *diffstream
        << "The following actual findings did not match the expected ones:\n";
    for (const auto &finding : unmatched_actual_findings) {
      constexpr int kIgnored = -1;
      TokenInfo(kIgnored, finding).ToStream(*diffstream, context) << std::endl;
    }
  }
  if (!unmatched_expected_findings.empty()) {
    all_match = false;
    *diffstream
        << "The following expected findings did not match the ones found:\n";
    for (const auto &finding : unmatched_expected_findings) {
      finding.ToStream(*diffstream, context) << std::endl;
    }
  }

  return all_match;
}

}  // namespace verible
