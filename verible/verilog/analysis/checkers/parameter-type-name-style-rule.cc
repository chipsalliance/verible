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

#include "verible/verilog/analysis/checkers/parameter-type-name-style-rule.h"

#include <set>
#include <string_view>

#include "absl/strings/match.h"
#include "verible/common/analysis/lint-rule-status.h"
#include "verible/common/analysis/matcher/bound-symbol-manager.h"
#include "verible/common/analysis/matcher/matcher.h"
#include "verible/common/strings/naming-utils.h"
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

// Register ParameterTypeNameStyleRule.
VERILOG_REGISTER_LINT_RULE(ParameterTypeNameStyleRule);

static constexpr std::string_view kMessage =
    "Parameter type names must use the lower_snake_case naming convention"
    " and end with _t.";

const LintRuleDescriptor &ParameterTypeNameStyleRule::GetDescriptor() {
  static const LintRuleDescriptor d{
      .name = "parameter-type-name-style",
      .topic = "parametrized-objects",
      .desc =
          "Checks that parameter type names follow the lower_snake_case naming "
          "convention and end with _t.",
  };
  return d;
}

static const Matcher &ParamDeclMatcher() {
  static const Matcher matcher(NodekParamDeclaration());
  return matcher;
}

void ParameterTypeNameStyleRule::HandleSymbol(
    const verible::Symbol &symbol, const SyntaxTreeContext &context) {
  verible::matcher::BoundSymbolManager manager;
  if (ParamDeclMatcher().Matches(symbol, &manager)) {
    const verible::TokenInfo *param_name_token = nullptr;
    if (!IsParamTypeDeclaration(symbol)) return;

    param_name_token = GetSymbolIdentifierFromParamDeclaration(symbol);
    const auto param_name = param_name_token->text();

    if (!verible::IsLowerSnakeCaseWithDigits(param_name) ||
        !absl::EndsWith(param_name, "_t")) {
      violations_.insert(LintViolation(*param_name_token, kMessage, context));
    }
  }
}

LintRuleStatus ParameterTypeNameStyleRule::Report() const {
  return LintRuleStatus(violations_, GetDescriptor());
}

}  // namespace analysis
}  // namespace verilog
