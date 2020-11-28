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

#include "verilog/analysis/checkers/signal_name_style_rule.h"

#include <set>
#include <string>

#include "absl/strings/str_cat.h"
#include "common/analysis/citation.h"
#include "common/analysis/lint_rule_status.h"
#include "common/analysis/matcher/bound_symbol_manager.h"
#include "common/analysis/matcher/matcher.h"
#include "common/strings/naming_utils.h"
#include "common/text/concrete_syntax_leaf.h"
#include "common/text/symbol.h"
#include "common/text/syntax_tree_context.h"
#include "common/text/token_info.h"
#include "common/util/logging.h"
#include "verilog/CST/data.h"
#include "verilog/CST/identifier.h"
#include "verilog/CST/net.h"
#include "verilog/CST/port.h"
#include "verilog/CST/verilog_matchers.h"
#include "verilog/analysis/descriptions.h"
#include "verilog/analysis/lint_rule_registry.h"

namespace verilog {
namespace analysis {

VERILOG_REGISTER_LINT_RULE(SignalNameStyleRule);

using verible::GetStyleGuideCitation;
using verible::LintRuleStatus;
using verible::LintViolation;
using verible::SyntaxTreeContext;
using verible::matcher::Matcher;

absl::string_view SignalNameStyleRule::Name() { return "signal-name-style"; }
const char SignalNameStyleRule::kTopic[] = "signal-conventions";
const char SignalNameStyleRule::kMessage[] =
    "Signal names must use lower_snake_case naming convention.";

std::string SignalNameStyleRule::GetDescription(
    DescriptionType description_type) {
  return absl::StrCat(
      "Checks that signal names use lower_snake_case naming convention. "
      "Signals are defined as \"a net, variable, or port within a "
      "SystemVerilog design\".  See ",
      GetStyleGuideCitation(kTopic), ".");
}

static const Matcher& PortMatcher() {
  static const Matcher matcher(NodekPortDeclaration());
  return matcher;
}

static const Matcher& NetMatcher() {
  static const Matcher matcher(NodekNetDeclaration());
  return matcher;
}

static const Matcher& DataMatcher() {
  static const Matcher matcher(NodekDataDeclaration());
  return matcher;
}

void SignalNameStyleRule::HandleSymbol(const verible::Symbol& symbol,
                                       const SyntaxTreeContext& context) {
  verible::matcher::BoundSymbolManager manager;
  if (PortMatcher().Matches(symbol, &manager)) {
    const auto* identifier_leaf =
        GetIdentifierFromModulePortDeclaration(symbol);
    const auto name = ABSL_DIE_IF_NULL(identifier_leaf)->get().text();
    if (!verible::IsLowerSnakeCaseWithDigits(name))
      violations_.insert(
          LintViolation(identifier_leaf->get(), kMessage, context));
  } else if (NetMatcher().Matches(symbol, &manager)) {
    const auto identifier_leaves = GetIdentifiersFromNetDeclaration(symbol);
    for (auto& leaf : identifier_leaves) {
      const auto name = leaf->text();
      if (!verible::IsLowerSnakeCaseWithDigits(name))
        violations_.insert(LintViolation(*leaf, kMessage, context));
    }
  } else if (DataMatcher().Matches(symbol, &manager)) {
    const auto identifier_leaves = GetIdentifiersFromDataDeclaration(symbol);
    for (auto& leaf : identifier_leaves) {
      const auto name = leaf->text();
      if (!verible::IsLowerSnakeCaseWithDigits(name))
        violations_.insert(LintViolation(*leaf, kMessage, context));
    }
  }
}

LintRuleStatus SignalNameStyleRule::Report() const {
  return LintRuleStatus(violations_, Name(), GetStyleGuideCitation(kTopic));
}

}  // namespace analysis
}  // namespace verilog
