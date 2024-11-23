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

#include "verible/verilog/analysis/checkers/positive-meaning-parameter-name-rule.h"

#include <set>

#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "verible/common/analysis/lint-rule-status.h"
#include "verible/common/analysis/matcher/bound-symbol-manager.h"
#include "verible/common/analysis/matcher/matcher.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/syntax-tree-context.h"
#include "verible/common/text/token-info.h"
#include "verible/verilog/CST/parameters.h"
#include "verible/verilog/CST/verilog-matchers.h"
#include "verible/verilog/analysis/descriptions.h"
#include "verible/verilog/analysis/lint-rule-registry.h"

namespace verilog {
namespace analysis {

using verible::LintRuleStatus;
using verible::LintViolation;
using verible::SyntaxTreeContext;
using verible::matcher::Matcher;

// Register PositiveMeaningParameterNameRule.
VERILOG_REGISTER_LINT_RULE(PositiveMeaningParameterNameRule);

static constexpr absl::string_view kMessage =
    "Use positive naming for parameters, start the name with 'enable' instead.";

const LintRuleDescriptor &PositiveMeaningParameterNameRule::GetDescriptor() {
  static const LintRuleDescriptor d{
      .name = "positive-meaning-parameter-name",
      .topic = "binary-parameters",
      .desc =
          "Checks that no parameter name starts with 'disable', using positive "
          "naming (starting with 'enable') is recommended.",
  };
  return d;
}

static const Matcher &ParamDeclMatcher() {
  static const Matcher matcher(NodekParamDeclaration());
  return matcher;
}

void PositiveMeaningParameterNameRule::HandleSymbol(
    const verible::Symbol &symbol, const SyntaxTreeContext &context) {
  verible::matcher::BoundSymbolManager manager;
  if (ParamDeclMatcher().Matches(symbol, &manager)) {
    if (IsParamTypeDeclaration(symbol)) return;

    auto identifiers = GetAllParameterNameTokens(symbol);
    for (const auto &id : identifiers) {
      const auto param_name = id->text();

      if (absl::StartsWithIgnoreCase(param_name, "disable")) {
        violations_.insert(LintViolation(
            *id, absl::StrCat(kMessage, "  (got: ", param_name, ")"), context));
      }
    }
  }
}

LintRuleStatus PositiveMeaningParameterNameRule::Report() const {
  return LintRuleStatus(violations_, GetDescriptor());
}

}  // namespace analysis
}  // namespace verilog
