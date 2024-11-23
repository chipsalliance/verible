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

#include "verible/common/analysis/linter-test-utils.h"

#include <iostream>
#include <iterator>
#include <set>
#include <vector>

#include "absl/strings/string_view.h"
#include "verible/common/analysis/lint-rule-status.h"
#include "verible/common/text/token-info.h"
#include "verible/common/util/algorithm.h"

namespace verible {

// This function helps find symmetric differences between two sets
// of violations (actual vs. expected).
// By comparing only the left-bound of the corresponding text ranges,
// this assumes that no two violations start at the same location.
// This assumption could be removed later, if needed.
static int CompareViolation(const LintViolation &lhs, const TokenInfo &rhs) {
  {
    const int delta =
        std::distance(rhs.text().begin(), lhs.token.text().begin());
    if (delta != 0) {
      return delta;
    }
  }
  {
    const int delta = std::distance(rhs.text().end(), lhs.token.text().end());
    if (delta != 0) {
      return delta;
    }
  }
  // Then compare enums, where we only care about equality.
  return rhs.token_enum() - lhs.token.token_enum();
}

// TODO(b/151371397): refactor this for re-use for multi-findings style tests.
bool LintTestCase::ExactMatchFindings(
    const std::set<LintViolation> &found_violations, absl::string_view base,
    std::ostream *diffstream) const {
  // Due to the order in which violations are visited, we can assert that
  // the reported violations are thus ordered.
  // TODO(fangism): It wouldn't be too expensive to verify this assertion.
  // The expected violations are also ordered by construction.
  const auto expected_violations = FindImportantTokens(base);
  // Thus, we can use an algorithm like std::set_symmetric_difference().

  std::vector<LintViolation> unmatched_found_violations;
  std::vector<TokenInfo> unmatched_expected_violations;

  set_symmetric_difference_split(
      found_violations.begin(), found_violations.end(),
      expected_violations.begin(), expected_violations.end(),
      std::back_inserter(unmatched_found_violations),
      std::back_inserter(unmatched_expected_violations), &CompareViolation);

  bool all_match = true;
  // TODO(fangism): plumb through a language-specific token enum translator,
  // through a TokenInfo::Context object.
  const TokenInfo::Context context(base);
  if (!unmatched_found_violations.empty()) {
    all_match = false;
    *diffstream
        << "FOUND these violations, but did not match the expected ones:\n";
    for (const auto &violation : unmatched_found_violations) {
      violation.token.ToStream(*diffstream, context) << std::endl;
    }
  }
  if (!unmatched_expected_violations.empty()) {
    all_match = false;
    *diffstream
        << "EXPECTED these violations, but did not match the ones found:\n";
    for (const auto &violation : unmatched_expected_violations) {
      violation.ToStream(*diffstream, context) << std::endl;
    }
  }

  return all_match;
}

}  // namespace verible
