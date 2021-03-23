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

// Used for reporting the outcome of a LintRule.

#ifndef VERIBLE_COMMON_ANALYSIS_LINT_RULE_STATUS_H_
#define VERIBLE_COMMON_ANALYSIS_LINT_RULE_STATUS_H_

#include <functional>
#include <iosfwd>
#include <set>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "common/strings/line_column_map.h"
#include "common/text/symbol.h"
#include "common/text/syntax_tree_context.h"
#include "common/text/token_info.h"

namespace verible {

// LintViolation is a class that represents a single rule violation.
struct LintViolation {
  // This construct records a token stream lint violation.
  LintViolation(const TokenInfo& token, const std::string& reason)
      : root(nullptr), token(token), reason(reason), context() {}

  // This construct records a syntax tree lint violation.
  // Use this variation when the violation can be localized to a single token.
  LintViolation(const TokenInfo& token, const std::string& reason,
                const SyntaxTreeContext& context)
      : root(nullptr), token(token), reason(reason), context(context) {}

  // This construct records a syntax tree lint violation.
  // Use this variation when the range of violation is a subtree that spans
  // multiple tokens.  The violation will be reported at the location of
  // the left-most leaf of the subtree.
  LintViolation(const Symbol& root, const std::string& reason,
                const SyntaxTreeContext& context);

  // root is a reference into original ConcreteSyntaxTree that
  // linter was run against. LintViolations should not outlive this tree.
  // It should point to the root symbol that the linter failed on.
  const Symbol* root = nullptr;

  // The token at which the error occurs, which includes location information.
  const TokenInfo token;

  // The reason why the violation occurs.
  std::string reason;

  // The context (list of ancestors) of the offending token.
  // For non-syntax-tree analyses, leave this blank.
  const SyntaxTreeContext context;

  bool operator<(const LintViolation& r) const {
    // compares addresses of violations, which correspond to substring
    // locations
    return token.text().data() < r.token.text().data();
  }
};

// LintRuleStatus represents the result of running a single lint rule.
struct LintRuleStatus {
  LintRuleStatus() : violations() {}

  LintRuleStatus(const std::set<LintViolation>& vs, absl::string_view rule_name,
                 const std::string& url)
      : lint_rule_name(rule_name), url(url), violations(vs) {}

  explicit LintRuleStatus(const std::set<LintViolation>& vs) : violations(vs) {}

  bool isOk() const { return violations.empty(); }

  // Remove subset of violations that is waived from report.
  // If `is_waived`() is true, remove the finding from the set of violations.
  void WaiveViolations(std::function<bool(const LintViolation&)>&& is_waived);

  // Name of the lint rule that produced this status.
  absl::string_view lint_rule_name;

  // Hold link to engdoc summary of violated rule
  std::string url;

  // Contains all violations of the LintRule
  std::set<LintViolation> violations;
};

// LintStatusFormatter is a class for printing LintRuleStatus's and
// LintViolations to an output stream
// Usage:
//   string filename  = ...
//   string code_text = ...
//   LintRuleStatus status = ...
//   LintStatusFormatter formatter(code_text);
//   formatter.FormatLintRuleStatus(&std::cout, status, filename)
class LintStatusFormatter {
 public:
  // Constructor takes a reference to the original text in order to setup
  // line_column_map
  explicit LintStatusFormatter(absl::string_view text)
      : line_column_map_(text) {}

  // Formats and outputs status to stream.
  // Path is the file path of original file. This is needed because it is not
  // contained in status.
  // Base is the string_view of the entire contents, used only for byte offset
  // calculation.
  void FormatLintRuleStatus(std::ostream* stream, const LintRuleStatus& status,
                            absl::string_view base,
                            absl::string_view path) const;

  // Formats, sorts and outputs status to stream with additional vulnerable code
  // line printed when enabled.
  // The violations contained in the statuses are sorted by their occurrence
  // in the code and are not grouped by the status object.
  // Path is the file path of original file. This is needed because it is not
  // contained in status.
  // Base is the string_view of the entire contents, used only for byte offset
  // calculation.
  void FormatLintRuleStatuses(
      std::ostream* stream, const std::vector<LintRuleStatus>& statuses,
      absl::string_view base, absl::string_view path,
      const std::vector<absl::string_view>& lines) const;

  // Formats and outputs violation on stream.
  // Path is file path of original file and url is a link to the ratified rule
  // that is being violated.
  // Base is the string_view of the entire contents, used only for byte offset
  // calculation.
  void FormatViolation(std::ostream* stream, const LintViolation& violation,
                       absl::string_view base, absl::string_view path,
                       absl::string_view url,
                       absl::string_view rule_name) const;

 private:
  // Translates byte offsets, which are supplied by LintViolations via
  // locations field, to line:column
  LineColumnMap line_column_map_;
};

}  // namespace verible

#endif  // VERIBLE_COMMON_ANALYSIS_LINT_RULE_STATUS_H_
