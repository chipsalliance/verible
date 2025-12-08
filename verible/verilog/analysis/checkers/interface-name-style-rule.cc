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

#include "verible/verilog/analysis/checkers/interface-name-style-rule.h"

#include <memory>
#include <set>
#include <string>
#include <string_view>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "re2/re2.h"
#include "verible/common/analysis/lint-rule-status.h"
#include "verible/common/analysis/matcher/bound-symbol-manager.h"
#include "verible/common/analysis/matcher/matcher.h"
#include "verible/common/text/config-utils.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/syntax-tree-context.h"
#include "verible/common/text/token-info.h"
#include "verible/verilog/CST/module.h"
#include "verible/verilog/CST/type.h"
#include "verible/verilog/CST/verilog-matchers.h"
#include "verible/verilog/analysis/descriptions.h"
#include "verible/verilog/analysis/lint-rule-registry.h"

namespace verilog {
namespace analysis {

VERILOG_REGISTER_LINT_RULE(InterfaceNameStyleRule);

using verible::LintRuleStatus;
using verible::LintViolation;
using verible::SyntaxTreeContext;
using verible::matcher::Matcher;

static constexpr std::string_view kLowerSnakeCaseWithSuffixRegex =
    "[a-z_0-9]+(_if)";
static constexpr std::string_view kDefaultStyleRegex =
    kLowerSnakeCaseWithSuffixRegex;

InterfaceNameStyleRule::InterfaceNameStyleRule()
    : style_regex_(
          std::make_unique<re2::RE2>(kDefaultStyleRegex, re2::RE2::Quiet)) {}

const LintRuleDescriptor &InterfaceNameStyleRule::GetDescriptor() {
  static const LintRuleDescriptor d{
      .name = "interface-name-style",
      .topic = "interface-conventions",
      .desc =
          "Checks that 'interface' names follow a naming convention defined by "
          "a RE2 regular expression. The default regex pattern expects "
          "\"lower_snake_case\" with a \"_if\" or \"_e\" suffix. Refer to "
          "https://github.com/chipsalliance/verible/tree/master/verilog/tools/"
          "lint#readme for more detail on regex patterns.",
      .param = {{"style_regex", std::string(kDefaultStyleRegex),
                 "A regex used to check interface name style."}},
  };
  return d;
}

static const Matcher &InterfaceMatcher() {
  static const Matcher matcher(NodekInterfaceDeclaration());
  return matcher;
}

std::string InterfaceNameStyleRule::CreateViolationMessage() {
  return absl::StrCat("Interface name does not match the naming convention ",
                      "defined by regex pattern: ", style_regex_->pattern());
}
void InterfaceNameStyleRule::HandleSymbol(const verible::Symbol &symbol,
                                          const SyntaxTreeContext &context) {
  verible::matcher::BoundSymbolManager manager;
  std::string_view name;
  const verible::TokenInfo *identifier_token;
  if (InterfaceMatcher().Matches(symbol, &manager)) {
    identifier_token = GetInterfaceNameToken(symbol);
    name = identifier_token->text();

    if (!RE2::FullMatch(name, *style_regex_)) {
      violations_.insert(
          LintViolation(*identifier_token, CreateViolationMessage(), context));
    }
  }
}

absl::Status InterfaceNameStyleRule::Configure(std::string_view configuration) {
  using verible::config::SetRegex;
  absl::Status s = verible::ParseNameValues(
      configuration, {{"style_regex", SetRegex(&style_regex_)}});
  return s;
}

LintRuleStatus InterfaceNameStyleRule::Report() const {
  return LintRuleStatus(violations_, GetDescriptor());
}

}  // namespace analysis
}  // namespace verilog
