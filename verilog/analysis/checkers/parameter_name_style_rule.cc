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

#include "verilog/analysis/checkers/parameter_name_style_rule.h"

#include <memory>
#include <set>
#include <string>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "common/analysis/lint_rule_status.h"
#include "common/analysis/matcher/bound_symbol_manager.h"
#include "common/analysis/matcher/matcher.h"
#include "common/text/config_utils.h"
#include "common/text/symbol.h"
#include "common/text/syntax_tree_context.h"
#include "common/text/token_info.h"
#include "re2/re2.h"
#include "verilog/CST/parameters.h"
#include "verilog/CST/verilog_matchers.h"
#include "verilog/analysis/descriptions.h"
#include "verilog/analysis/lint_rule_registry.h"
#include "verilog/parser/verilog_token_enum.h"

namespace verilog {
namespace analysis {

VERILOG_REGISTER_LINT_RULE(ParameterNameStyleRule);

using verible::LintRuleStatus;
using verible::LintViolation;
using verible::SyntaxTreeContext;
using Matcher = verible::matcher::Matcher;

// PascalCase, may end in _[0-9]+
static constexpr absl::string_view localparam_default_regex =
    "([A-Z0-9]+[a-z0-9]*)+(_[0-9]+)?";

// PascalCase (may end in _[0-9]+) or UPPER_SNAKE_CASE
static constexpr absl::string_view parameter_default_regex =
    "(([A-Z0-9]+[a-z0-9]*)+(_[0-9]+)?)|([A-Z_0-9]+)";

ParameterNameStyleRule::ParameterNameStyleRule()
    : localparam_style_regex_(std::make_unique<re2::RE2>(
          localparam_default_regex, re2::RE2::Quiet)),
      parameter_style_regex_(std::make_unique<re2::RE2>(parameter_default_regex,
                                                        re2::RE2::Quiet)) {}

const LintRuleDescriptor &ParameterNameStyleRule::GetDescriptor() {
  static const LintRuleDescriptor d{
      .name = "parameter-name-style",
      .topic = "constants",
      .desc =
          "Checks that parameter and localparm names conform to a naming "
          "convention defined by RE2 regular expressions. The default regex "
          "pattern for boht localparam and parameter names is PascalCase with "
          "an optional _digit suffix. Parameters may also be UPPER_SNAKE_CASE. "
          "Refer "
          "to "
          "https://github.com/chipsalliance/verible/tree/master/verilog/tools/"
          "lint#readme for more detail on verible regex patterns.",
      .param = {{"localparam_style_regex",
                 std::string(localparam_default_regex),
                 "A regex used to check localparam name style."},
                {"parameter_style_regex", std::string(parameter_default_regex),
                 "A regex used to check parameter name style."}},
  };

  return d;
}

static const Matcher &ParamDeclMatcher() {
  static const Matcher matcher(NodekParamDeclaration());
  return matcher;
}

std::string ParameterNameStyleRule::CreateLocalparamViolationMessage() {
  return absl::StrCat(
      "Localparam name does not match the naming convention ",
      "defined by regex pattern: ", localparam_style_regex_->pattern());
}

std::string ParameterNameStyleRule::CreateParameterViolationMessage() {
  return absl::StrCat(
      "Parameter name does not match the naming convention ",
      "defined by regex pattern: ", parameter_style_regex_->pattern());
}

void ParameterNameStyleRule::HandleSymbol(const verible::Symbol &symbol,
                                          const SyntaxTreeContext &context) {
  verible::matcher::BoundSymbolManager manager;
  if (ParamDeclMatcher().Matches(symbol, &manager)) {
    if (IsParamTypeDeclaration(symbol)) return;

    const verilog_tokentype param_decl_token = GetParamKeyword(symbol);

    auto identifiers = GetAllParameterNameTokens(symbol);

    for (const auto *id : identifiers) {
      const auto name = id->text();
      switch (param_decl_token) {
        case TK_localparam:
          if (!RE2::FullMatch(name, *localparam_style_regex_)) {
            violations_.insert(LintViolation(
                *id, CreateLocalparamViolationMessage(), context));
          }
          break;

        case TK_parameter:
          if (!RE2::FullMatch(name, *parameter_style_regex_)) {
            violations_.insert(
                LintViolation(*id, CreateParameterViolationMessage(), context));
          }
          break;

        default:
          break;
      }
    }
  }
}

absl::Status ParameterNameStyleRule::Configure(
    absl::string_view configuration) {
  using verible::config::SetRegex;
  absl::Status s = verible::ParseNameValues(
      configuration,
      {{"localparam_style_regex", SetRegex(&localparam_style_regex_)},
       {"parameter_style_regex", SetRegex(&parameter_style_regex_)}});

  return s;
}

LintRuleStatus ParameterNameStyleRule::Report() const {
  return LintRuleStatus(violations_, GetDescriptor());
}

}  // namespace analysis
}  // namespace verilog
