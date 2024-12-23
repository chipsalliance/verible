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

#include "verible/verilog/analysis/checkers/parameter-name-style-rule.h"

#include <cstdint>
#include <memory>
#include <set>
#include <string>
#include <vector>

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
#include "verible/verilog/CST/parameters.h"
#include "verible/verilog/CST/verilog-matchers.h"
#include "verible/verilog/analysis/descriptions.h"
#include "verible/verilog/analysis/lint-rule-registry.h"
#include "verible/verilog/parser/verilog-token-enum.h"

namespace verilog {
namespace analysis {

VERILOG_REGISTER_LINT_RULE(ParameterNameStyleRule);

using verible::LintRuleStatus;
using verible::LintViolation;
using verible::SyntaxTreeContext;
using Matcher = verible::matcher::Matcher;

// Upper Camel Case (may end in _[0-9]+)
static constexpr absl::string_view kUpperCamelCaseRegex =
    "([A-Z0-9]+[a-z0-9]*)+(_[0-9]+)?";
// ALL_CAPS
static constexpr absl::string_view kAllCapsRegex = "[A-Z_0-9]+";

static constexpr absl::string_view kLocalparamDefaultRegex;
static constexpr absl::string_view kParameterDefaultRegex;

ParameterNameStyleRule::ParameterNameStyleRule()
    : localparam_style_regex_(std::make_unique<re2::RE2>(
          absl::StrCat("(", kUpperCamelCaseRegex, ")"), re2::RE2::Quiet)),
      parameter_style_regex_(std::make_unique<re2::RE2>(
          absl::StrCat("(", kUpperCamelCaseRegex, ")|(", kAllCapsRegex, ")"),
          re2::RE2::Quiet)) {}

const LintRuleDescriptor &ParameterNameStyleRule::GetDescriptor() {
  static const LintRuleDescriptor d{
      .name = "parameter-name-style",
      .topic = "constants",
      .desc =
          "Checks that parameter and localparm names conform to a naming "
          "convention based on a choice of 'CamelCase', 'ALL_CAPS' and a user "
          "defined regex ORed together. Empty configurtaion: no style "
          "enforcement. Refer to "
          "https://github.com/chipsalliance/verible/tree/master/verilog/tools/"
          "lint#readme for more detail on verible regex patterns.",
      .param = {{"localparam_style", "CamelCase", "Style of localparam names"},
                {"parameter_style", "CamelCase|ALL_CAPS",
                 "Style of parameter names."},
                {"localparam_style_regex", std::string(kLocalparamDefaultRegex),
                 "A regex used to check localparam name style."},
                {"parameter_style_regex", std::string(kParameterDefaultRegex),
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

absl::Status ParameterNameStyleRule::AppendRegex(
    std::unique_ptr<re2::RE2> *rule_regex, absl::string_view regex_str) {
  if (*rule_regex == nullptr) {
    *rule_regex = std::make_unique<re2::RE2>(absl::StrCat("(", regex_str, ")"),
                                             re2::RE2::Quiet);
  } else {
    *rule_regex = std::make_unique<re2::RE2>(
        absl::StrCat((*rule_regex)->pattern(), "|(", regex_str, ")"),
        re2::RE2::Quiet);
  }

  if ((*rule_regex)->ok()) {
    return absl::OkStatus();
  }

  std::string error_msg = absl::StrCat("Failed to parse regular expression: ",
                                       (*rule_regex)->error());
  return absl::InvalidArgumentError(error_msg);
}

absl::Status ParameterNameStyleRule::ConfigureRegex(
    std::unique_ptr<re2::RE2> *rule_regex, uint32_t config_style,
    std::unique_ptr<re2::RE2> *config_style_regex) {
  absl::Status s;

  int config_style_regex_length = (*config_style_regex)->pattern().length();

  // If no rule is set, no style enforcement
  if (config_style == 0 && config_style_regex_length == 0) {
    // Set the regex pattern to match everything
    *rule_regex = std::make_unique<re2::RE2>(".*", re2::RE2::Quiet);
    return absl::OkStatus();
  }

  // Clear rule_regex
  *rule_regex = nullptr;

  // Append UpperCamelCase regex to rule_regex (if enabled)
  if (config_style & kUpperCamelCase) {
    s = AppendRegex(rule_regex, kUpperCamelCaseRegex);
    if (!s.ok()) {
      return s;
    }
  }

  // Append ALL_CAPS regex to rule_regex (if enabled)
  if (config_style & kAllCaps) {
    s = AppendRegex(rule_regex, kAllCapsRegex);
    if (!s.ok()) {
      return s;
    }
  }

  // Append regex from config_style_regex (if provided by the user)
  if (config_style_regex_length > 0) {
    s = AppendRegex(rule_regex, (*config_style_regex)->pattern());
  }

  return s;
}

absl::Status ParameterNameStyleRule::Configure(
    absl::string_view configuration) {
  // same sequence as enum StyleChoicesBits
  static const std::vector<absl::string_view> choices = {"CamelCase",
                                                         "ALL_CAPS"};

  uint32_t localparam_style = kUpperCamelCase;
  uint32_t parameter_style = kUpperCamelCase | kAllCaps;
  std::unique_ptr<re2::RE2> localparam_style_regex =
      std::make_unique<re2::RE2>(kLocalparamDefaultRegex, re2::RE2::Quiet);
  std::unique_ptr<re2::RE2> parameter_style_regex =
      std::make_unique<re2::RE2>(kParameterDefaultRegex, re2::RE2::Quiet);

  using verible::config::SetNamedBits;
  using verible::config::SetRegex;

  absl::Status s = verible::ParseNameValues(
      configuration,
      {{"localparam_style", SetNamedBits(&localparam_style, choices)},
       {"parameter_style", SetNamedBits(&parameter_style, choices)},
       {"localparam_style_regex", SetRegex(&localparam_style_regex)},
       {"parameter_style_regex", SetRegex(&parameter_style_regex)}});

  if (!s.ok()) {
    return s;
  }

  // Form a regex to use based on *_style, and *_style_regex
  s = ConfigureRegex(&localparam_style_regex_, localparam_style,
                     &localparam_style_regex);
  if (!s.ok()) {
    return s;
  }

  s = ConfigureRegex(&parameter_style_regex_, parameter_style,
                     &parameter_style_regex);
  return s;
}

LintRuleStatus ParameterNameStyleRule::Report() const {
  return LintRuleStatus(violations_, GetDescriptor());
}

}  // namespace analysis
}  // namespace verilog
