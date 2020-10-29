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

#include "verilog/analysis/checkers/explicit_function_task_parameter_type_rule.h"

#include <set>
#include <string>

#include "absl/strings/str_cat.h"
#include "common/analysis/citation.h"
#include "common/analysis/lint_rule_status.h"
#include "common/analysis/matcher/bound_symbol_manager.h"
#include "common/analysis/matcher/matcher.h"
#include "common/text/concrete_syntax_leaf.h"
#include "common/text/symbol.h"
#include "common/text/syntax_tree_context.h"
#include "common/util/logging.h"
#include "verilog/CST/port.h"
#include "verilog/CST/type.h"
#include "verilog/CST/verilog_matchers.h"
#include "verilog/analysis/descriptions.h"
#include "verilog/analysis/lint_rule_registry.h"

namespace verilog {
namespace analysis {

using verible::GetStyleGuideCitation;
using verible::LintRuleStatus;
using verible::LintViolation;
using verible::SyntaxTreeContext;
using Matcher = verible::matcher::Matcher;

// Register ExplicitFunctionTaskParameterTypeRule
VERILOG_REGISTER_LINT_RULE(ExplicitFunctionTaskParameterTypeRule);

absl::string_view ExplicitFunctionTaskParameterTypeRule::Name() {
  return "explicit-function-task-parameter-type";
}
const char ExplicitFunctionTaskParameterTypeRule::kTopic[] =
    "function-task-argument-types";
const char ExplicitFunctionTaskParameterTypeRule::kMessage[] =
    "Explicitly define a storage type for every function parameter.";

std::string ExplicitFunctionTaskParameterTypeRule::GetDescription(
    DescriptionType description_type) {
  return absl::StrCat(
      "Checks that every function and task parameter is declared with an "
      "explicit storage type. See ",
      GetStyleGuideCitation(kTopic), ".");
}

static const Matcher& PortMatcher() {
  static const Matcher matcher(NodekPortItem());
  return matcher;
}

void ExplicitFunctionTaskParameterTypeRule::HandleSymbol(
    const verible::Symbol& symbol, const SyntaxTreeContext& context) {
  verible::matcher::BoundSymbolManager manager;
  if (PortMatcher().Matches(symbol, &manager)) {
    const auto* type_node = GetTypeOfTaskFunctionPortItem(symbol);
    if (!IsStorageTypeOfDataTypeSpecified(*ABSL_DIE_IF_NULL(type_node))) {
      const auto* port_id = GetIdentifierFromTaskFunctionPortItem(symbol);
      violations_.insert(LintViolation(*port_id, kMessage, context));
    }
  }
}

LintRuleStatus ExplicitFunctionTaskParameterTypeRule::Report() const {
  return LintRuleStatus(violations_, Name(), GetStyleGuideCitation(kTopic));
}

}  // namespace analysis
}  // namespace verilog
