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

#include "verilog/analysis/checkers/plusarg_assignment_rule.h"

#include <set>
#include <string>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "common/analysis/citation.h"
#include "common/analysis/lint_rule_status.h"
#include "common/analysis/matcher/bound_symbol_manager.h"
#include "common/analysis/matcher/matcher.h"
#include "common/text/concrete_syntax_leaf.h"
#include "common/text/symbol.h"
#include "common/text/syntax_tree_context.h"
#include "common/text/token_info.h"
#include "verilog/CST/verilog_matchers.h"
#include "verilog/analysis/descriptions.h"
#include "verilog/analysis/lint_rule_registry.h"

namespace verilog {
namespace analysis {

using verible::GetStyleGuideCitation;
using verible::matcher::Matcher;

VERILOG_REGISTER_LINT_RULE(PlusargAssignmentRule);

absl::string_view PlusargAssignmentRule::Name() { return "plusarg-assignment"; }
const char PlusargAssignmentRule::kTopic[] = "plusarg-value-assignment";
const char PlusargAssignmentRule::kForbiddenFunctionName[] = "$test$plusargs";
const char PlusargAssignmentRule::kCorrectFunctionName[] = "$value$plusargs";

std::string PlusargAssignmentRule::GetDescription(
    DescriptionType description_type) {
  return absl::StrCat("Checks that plusargs are always assigned a value, by ",
                      "ensuring that plusargs are never accessed using the ",
                      Codify(kForbiddenFunctionName, description_type),
                      " system task. See ", GetStyleGuideCitation(kTopic), ".");
}

static const Matcher& IdMatcher() {
  static const Matcher matcher(SystemTFIdentifierLeaf().Bind("name"));
  return matcher;
}

void PlusargAssignmentRule::HandleSymbol(
    const verible::Symbol& symbol, const verible::SyntaxTreeContext& context) {
  verible::matcher::BoundSymbolManager manager;
  if (IdMatcher().Matches(symbol, &manager)) {
    if (auto leaf = manager.GetAs<verible::SyntaxTreeLeaf>("name")) {
      if (kForbiddenFunctionName == leaf->get().text()) {
        violations_.insert(
            verible::LintViolation(leaf->get(), FormatReason(), context));
      }
    }
  }
}

verible::LintRuleStatus PlusargAssignmentRule::Report() const {
  return verible::LintRuleStatus(violations_, Name(),
                                 GetStyleGuideCitation(kTopic));
}

std::string PlusargAssignmentRule::FormatReason() const {
  return absl::StrCat("Do not use ", kForbiddenFunctionName,
                      " to access plusargs, use ", kCorrectFunctionName,
                      " instead.");
}

}  // namespace analysis
}  // namespace verilog
