// Copyright 2017-2026 The Verible Authors.
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

#include "verible/verilog/analysis/checkers/enum-constant-name-style-rule.h"

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
#include "verible/verilog/CST/type.h"
#include "verible/verilog/CST/verilog-matchers.h"
#include "verible/verilog/analysis/descriptions.h"
#include "verible/verilog/analysis/lint-rule-registry.h"

namespace verilog {
namespace analysis {

VERILOG_REGISTER_LINT_RULE(EnumConstantNameStyleRule);

using verible::LintRuleStatus;
using verible::LintViolation;
using verible::SyntaxTreeContext;
using Matcher = verible::matcher::Matcher;

static constexpr std::string_view kDefaultStyleRegex =
    "k([A-Z0-9]+[a-z0-9]*)+(_[0-9]+)?";

EnumConstantNameStyleRule::EnumConstantNameStyleRule()
    : style_regex_(
          std::make_unique<re2::RE2>(kDefaultStyleRegex, re2::RE2::Quiet)) {}

const LintRuleDescriptor &EnumConstantNameStyleRule::GetDescriptor() {
  static const LintRuleDescriptor d{
      .name = "enum-constant-name-style",
      .topic = "enumerations",
      .desc =
          "Checks that enum constant names (enumeration values) follow a "
          "naming convention defined by a RE2 regular expression. The default "
          "regex pattern expects \"kCamelCase\". Refer to "
          "https://github.com/chipsalliance/verible/tree/master/verilog/tools/"
          "lint#readme for more detail on verible regex patterns.",
      .param = {{"style_regex", std::string(kDefaultStyleRegex),
                 "A regex used to check enum constant name style."}},
  };
  return d;
}

static const Matcher &EnumNameMatcher() {
  static const Matcher matcher(NodekEnumName());
  return matcher;
}

std::string EnumConstantNameStyleRule::CreateViolationMessage(
    std::string_view name) const {
  return absl::StrCat("Enum constant '", name,
                      "' does not match the naming convention defined by "
                      "regex pattern: ",
                      style_regex_->pattern());
}

void EnumConstantNameStyleRule::HandleSymbol(const verible::Symbol &symbol,
                                             const SyntaxTreeContext &context) {
  verible::matcher::BoundSymbolManager manager;
  if (EnumNameMatcher().Matches(symbol, &manager)) {
    const auto *identifier_leaf = GetSymbolIdentifierFromEnumName(symbol);
    if (identifier_leaf == nullptr) return;
    const auto name = identifier_leaf->get().text();
    if (!RE2::FullMatch(name, *style_regex_)) {
      violations_.insert(LintViolation(identifier_leaf->get(),
                                       CreateViolationMessage(name), context));
    }
  }
}

absl::Status EnumConstantNameStyleRule::Configure(
    std::string_view configuration) {
  using verible::config::SetRegex;
  return verible::ParseNameValues(configuration,
                                  {{"style_regex", SetRegex(&style_regex_)}});
}

LintRuleStatus EnumConstantNameStyleRule::Report() const {
  return LintRuleStatus(violations_, GetDescriptor());
}

}  // namespace analysis
}  // namespace verilog
