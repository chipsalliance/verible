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

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "common/strings/line_column_map.h"
#include "common/text/concrete_syntax_leaf.h"
#include "common/text/symbol.h"
#include "common/text/syntax_tree_context.h"
#include "common/text/token_info.h"
#include "common/text/tree_utils.h"
#include "common/util/spacer.h"

namespace verible {

std::string AutoFix::Apply(absl::string_view base) const {
  std::string result;
  auto prev_start = base.cbegin();
  for (const auto& edit : edits_) {
    CHECK_LE(base.cbegin(), edit.fragment.cbegin());
    CHECK_GE(base.cend(), edit.fragment.cend());

    const absl::string_view text_before(
        prev_start, std::distance(prev_start, edit.fragment.cbegin()));
    absl::StrAppend(&result, text_before, edit.replacement);

    prev_start = edit.fragment.cend();
  }
  const absl::string_view text_after(prev_start,
                                     std::distance(prev_start, base.cend()));
  return absl::StrCat(result, text_after);
}

bool AutoFix::AddEdits(const std::set<ReplacementEdit>& new_edits) {
  // Check for conflicts
  for (const auto& edit : new_edits) {
    if (edits_.find(edit) != edits_.end()) {
      return false;
    }
  }
  edits_.insert(new_edits.cbegin(), new_edits.cend());
  return true;
}

static TokenInfo SymbolToToken(const Symbol& root) {
  const auto* leaf = GetLeftmostLeaf(root);
  if (leaf) {
    return leaf->get();
  }
  // There shouldn't be any leaf-less subtrees.
  return TokenInfo::EOFToken();
}

LintViolation::LintViolation(const Symbol& root, const std::string& reason,
                             const SyntaxTreeContext& context,
                             const std::vector<AutoFix>& autofixes)
    : root(&root),
      token(SymbolToToken(root)),
      reason(reason),
      context(context),
      autofixes(autofixes) {}

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

void LintStatusFormatter::FormatLintRuleStatuses(
    std::ostream* stream, const std::vector<LintRuleStatus>& statuses,
    absl::string_view base, absl::string_view path,
    const std::vector<absl::string_view>& lines) const {
  std::set<LintViolationWithStatus> violations;

  // TODO(fangism): rewrite as a linear time merge of pre-ordered sub-sequences
  for (const auto& status : statuses) {
    for (const auto& violation : status.violations) {
      violations.insert(LintViolationWithStatus(&violation, &status));
    }
  }

  for (auto violation : violations) {
    FormatViolation(stream, *violation.violation, base, path,
                    violation.status->url, violation.status->lint_rule_name);
    if (!violation.violation->autofixes.empty()) {
      *stream << " (autofix available)";
    }
    *stream << std::endl;
    auto cursor = line_column_map_.GetLineColAtOffset(
        base, violation.violation->token.left(base));
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
  const verible::LineColumnRange range{
      line_column_map_.GetLineColAtOffset(base, violation.token.left(base)),
      line_column_map_.GetLineColAtOffset(base, violation.token.right(base))};

  (*stream) << path << ':' << range << ' ' << violation.reason << ' ' << url
            << " [" << rule_name << ']';
}

// Formats and outputs violation to a file stream in a syntax accepted by
// --waiver_files flag. Path is file path of original file
void LintStatusFormatter::FormatViolationWaiver(
    std::ostream* stream, const LintViolation& violation,
    absl::string_view base, absl::string_view path,
    absl::string_view rule_name) const {
  const verible::LineColumnRange range{
      line_column_map_.GetLineColAtOffset(base, violation.token.left(base)),
      line_column_map_.GetLineColAtOffset(base, violation.token.right(base))};

  (*stream) << "waive" << ' ' << "--rule=" << rule_name << ' '
            << "--line=" << range.start.line + 1 << ' ' << "--location="
            << "\"" << path << "\"";
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
