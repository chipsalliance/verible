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

#include "common/analysis/lint_rule_status.h"

#include <algorithm>
#include <functional>
#include <iostream>
#include <iterator>
#include <ostream>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "common/strings/line_column_map.h"
#include "common/text/concrete_syntax_leaf.h"
#include "common/text/symbol.h"
#include "common/text/syntax_tree_context.h"
#include "common/text/token_info.h"
#include "common/text/tree_utils.h"
#include "common/util/spacer.h"

namespace verible {

static TokenInfo SymbolToToken(const Symbol& root) {
  const auto* leaf = GetLeftmostLeaf(root);
  if (leaf) {
    return leaf->get();
  }
  // There shouldn't be any leaf-less subtrees.
  return TokenInfo::EOFToken();
}

LintViolation::LintViolation(const Symbol& root, const std::string& reason,
                             const SyntaxTreeContext& context)
    : root(&root),
      token(SymbolToToken(root)),
      reason(reason),
      context(context) {}

void LintStatusFormatter::FormatLintRuleStatus(std::ostream* stream,
                                               const LintRuleStatus& status,
                                               absl::string_view base,
                                               absl::string_view path) const {
  for (const auto& violation : status.violations) {
    FormatViolation(stream, violation, base, path, status.url,
                    status.lint_rule_name);
    (*stream) << std::endl;
  }
}

struct LintViolationWithStatus {
  const LintViolation* violation;
  const LintRuleStatus* status;

  LintViolationWithStatus(const LintViolation* v, const LintRuleStatus* s)
      : violation(v), status(s) {}

  bool operator<(const LintViolationWithStatus& r) const {
    // compares addresses which correspond to locations within the same string
    return violation->token.text().data() < r.violation->token.text().data();
  }
};

void LintStatusFormatter::FormatLintRuleStatuses(
    std::ostream* stream, const std::vector<LintRuleStatus>& statuses,
    absl::string_view base, absl::string_view path,
    const std::vector<absl::string_view>& lines) const {
  std::set<LintViolationWithStatus> violations;

  // TODO(fangism): rewrite as a linear time merge of pre-ordered sub-sequences
  for (auto& status : statuses) {
    for (auto& violation : status.violations) {
      violations.insert(LintViolationWithStatus(&violation, &status));
    }
  }

  for (auto violation : violations) {
    FormatViolation(stream, *violation.violation, base, path,
                    violation.status->url, violation.status->lint_rule_name);
    *stream << std::endl;
    auto cursor = line_column_map_(violation.violation->token.left(base));
    if (cursor.line < static_cast<int>(lines.size())) {
      *stream << lines[cursor.line] << std::endl;
      *stream << verible::Spacer(cursor.column) << "^" << std::endl;
    }
  }
}

// Formats and outputs violation on stream
// Path is file path of original file and url is a link to violated rule
void LintStatusFormatter::FormatViolation(std::ostream* stream,
                                          const LintViolation& violation,
                                          absl::string_view base,
                                          absl::string_view path,
                                          absl::string_view url,
                                          absl::string_view rule_name) const {
  // TODO(fangism): Use the context member to print which named construct or
  // design element the violation appears in (or full stack thereof).
  (*stream) << path << ':' << line_column_map_(violation.token.left(base))
            << ": " << violation.reason << ' ' << url << " [" << rule_name
            << ']';
}

void LintRuleStatus::WaiveViolations(
    std::function<bool(const LintViolation&)>&& is_waived) {
  std::set<LintViolation> filtered_violations;
  std::remove_copy_if(
      violations.begin(), violations.end(),
      std::inserter(filtered_violations, filtered_violations.begin()),
      is_waived);
  violations.swap(filtered_violations);
}

}  // namespace verible
