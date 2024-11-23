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

#include "verible/verilog/analysis/checkers/enum-name-style-rule.h"

#include <memory>
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
#include "verible/common/util/logging.h"
#include "verible/verilog/CST/type.h"
#include "verible/verilog/CST/verilog-matchers.h"
#include "verible/verilog/analysis/descriptions.h"
#include "verible/verilog/analysis/lint-rule-registry.h"

namespace verilog {
namespace analysis {

VERILOG_REGISTER_LINT_RULE(EnumNameStyleRule);

using verible::LintRuleStatus;
using verible::LintViolation;
using verible::SyntaxTreeContext;
using verible::matcher::Matcher;

static constexpr absl::string_view kDefaultStyleRegex = "[a-z_0-9]+(_t|_e)";

EnumNameStyleRule::EnumNameStyleRule()
    : style_regex_(
          std::make_unique<re2::RE2>(kDefaultStyleRegex, re2::RE2::Quiet)) {}

const LintRuleDescriptor &EnumNameStyleRule::GetDescriptor() {
  static const LintRuleDescriptor d{
      .name = "enum-name-style",
      .topic = "enumerations",
      .desc =
          "Checks that enum type names follow a naming convention defined by a "
          "RE2 regular expression. The default regex pattern expects "
          "\"lower_snake_case\" with either a \"_t\" or \"_e\" suffix. Refer "
          "to "
          "https://github.com/chipsalliance/verible/tree/master/verilog/tools/"
          "lint#readme for more detail on verible regex patterns.",
      .param = {{"style_regex", std::string(kDefaultStyleRegex),
                 "A regex used to check enum type name style."}},
  };
  return d;
}

static const Matcher &TypedefMatcher() {
  static const Matcher matcher(NodekTypeDeclaration());
  return matcher;
}

std::string EnumNameStyleRule::CreateViolationMessage() {
  return absl::StrCat("Enum type name does not match the naming convention ",
                      "defined by regex pattern: ", style_regex_->pattern());
}

void EnumNameStyleRule::HandleSymbol(const verible::Symbol &symbol,
                                     const SyntaxTreeContext &context) {
  verible::matcher::BoundSymbolManager manager;
  if (TypedefMatcher().Matches(symbol, &manager)) {
    // TODO: This can be changed to checking type of child (by index) when we
    // have consistent shape for all kTypeDeclaration nodes.
    if (!FindAllEnumTypes(symbol).empty()) {
      const auto *identifier_leaf = GetIdentifierFromTypeDeclaration(symbol);
      const auto name = ABSL_DIE_IF_NULL(identifier_leaf)->get().text();
      if (!RE2::FullMatch(name, *style_regex_)) {
        violations_.insert(LintViolation(identifier_leaf->get(),
                                         CreateViolationMessage(), context));
      }
    } else {
      // Not an enum definition
      return;
    }
  }
}

absl::Status EnumNameStyleRule::Configure(absl::string_view configuration) {
  using verible::config::SetRegex;
  absl::Status s = verible::ParseNameValues(
      configuration, {{"style_regex", SetRegex(&style_regex_)}});
  return s;
}

LintRuleStatus EnumNameStyleRule::Report() const {
  return LintRuleStatus(violations_, GetDescriptor());
}

}  // namespace analysis
}  // namespace verilog
