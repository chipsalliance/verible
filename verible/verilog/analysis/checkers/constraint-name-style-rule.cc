// Copyright 2017-2023 The Verible Authors.
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

#include "verible/verilog/analysis/checkers/constraint-name-style-rule.h"

#include <set>
#include <string>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "re2/re2.h"
#include "verible/common/analysis/lint-rule-status.h"
#include "verible/common/analysis/matcher/bound-symbol-manager.h"
#include "verible/common/analysis/matcher/matcher.h"
#include "verible/common/text/config-utils.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/syntax-tree-context.h"
#include "verible/common/text/token-info.h"
#include "verible/verilog/CST/constraints.h"
#include "verible/verilog/CST/verilog-matchers.h"
#include "verible/verilog/analysis/descriptions.h"
#include "verible/verilog/analysis/lint-rule-registry.h"

namespace verilog {
namespace analysis {

using verible::LintRuleStatus;
using verible::LintViolation;
using verible::SyntaxTreeContext;
using verible::matcher::Matcher;

// Register ConstraintNameStyleRule.
VERILOG_REGISTER_LINT_RULE(ConstraintNameStyleRule);

const LintRuleDescriptor &ConstraintNameStyleRule::GetDescriptor() {
  static const LintRuleDescriptor d{
      .name = "constraint-name-style",
      .topic = "constraints",
      .desc =
          "Check that constraint names follow the required name style "
          "specified by a regular expression.",
      .param = {{"pattern", kSuffix.data()}},
  };
  return d;
}

absl::Status ConstraintNameStyleRule::Configure(
    absl::string_view configuration) {
  return verible::ParseNameValues(
      configuration, {{"pattern", verible::config::SetRegex(&regex)}});
}

static const Matcher &ConstraintMatcher() {
  static const Matcher matcher(NodekConstraintDeclaration());
  return matcher;
}

void ConstraintNameStyleRule::HandleSymbol(const verible::Symbol &symbol,
                                           const SyntaxTreeContext &context) {
  verible::matcher::BoundSymbolManager manager;
  if (ConstraintMatcher().Matches(symbol, &manager)) {
    // Since an out-of-line definition is always followed by a forward
    // declaration somewhere else (in this case inside a class), we can just
    // ignore all out-of-line definitions to  avoid duplicate lint errors on
    // the same name.
    if (IsOutOfLineConstraintDefinition(symbol)) {
      return;
    }

    const auto *identifier_token =
        GetSymbolIdentifierFromConstraintDeclaration(symbol);
    if (!identifier_token) return;

    const absl::string_view constraint_name = identifier_token->text();

    if (!RE2::FullMatch(constraint_name, *regex)) {
      violations_.insert(
          LintViolation(*identifier_token, FormatReason(), context));
    }
  }
}

std::string ConstraintNameStyleRule::FormatReason() const {
  return absl::StrCat("Constraint names must obey the following regex: ",
                      regex->pattern());
}

LintRuleStatus ConstraintNameStyleRule::Report() const {
  return LintRuleStatus(violations_, GetDescriptor());
}

}  // namespace analysis
}  // namespace verilog
