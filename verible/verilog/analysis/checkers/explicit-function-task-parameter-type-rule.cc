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

#include "verible/verilog/analysis/checkers/explicit-function-task-parameter-type-rule.h"

#include <set>
#include <string_view>

#include "verible/common/analysis/lint-rule-status.h"
#include "verible/common/analysis/matcher/bound-symbol-manager.h"
#include "verible/common/analysis/matcher/matcher.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/syntax-tree-context.h"
#include "verible/common/util/logging.h"
#include "verible/verilog/CST/port.h"
#include "verible/verilog/CST/type.h"
#include "verible/verilog/CST/verilog-matchers.h"
#include "verible/verilog/analysis/descriptions.h"
#include "verible/verilog/analysis/lint-rule-registry.h"

namespace verilog {
namespace analysis {

using verible::LintRuleStatus;
using verible::LintViolation;
using verible::SyntaxTreeContext;
using Matcher = verible::matcher::Matcher;

// Register ExplicitFunctionTaskParameterTypeRule
VERILOG_REGISTER_LINT_RULE(ExplicitFunctionTaskParameterTypeRule);

static constexpr std::string_view kMessage =
    "Explicitly define a storage type for every function parameter.";

const LintRuleDescriptor &
ExplicitFunctionTaskParameterTypeRule::GetDescriptor() {
  static const LintRuleDescriptor d{
      .name = "explicit-function-task-parameter-type",
      .topic = "function-task-argument-types",
      .desc =
          "Checks that every function and task parameter is declared "
          "with an explicit storage type.",
  };
  return d;
}

static const Matcher &PortMatcher() {
  static const Matcher matcher(NodekPortItem());
  return matcher;
}

void ExplicitFunctionTaskParameterTypeRule::HandleSymbol(
    const verible::Symbol &symbol, const SyntaxTreeContext &context) {
  verible::matcher::BoundSymbolManager manager;
  if (PortMatcher().Matches(symbol, &manager)) {
    const auto *type_node = GetTypeOfTaskFunctionPortItem(symbol);
    if (!IsStorageTypeOfDataTypeSpecified(*ABSL_DIE_IF_NULL(type_node))) {
      const auto *port_id = GetIdentifierFromTaskFunctionPortItem(symbol);
      violations_.insert(LintViolation(*port_id, kMessage, context));
    }
  }
}

LintRuleStatus ExplicitFunctionTaskParameterTypeRule::Report() const {
  return LintRuleStatus(violations_, GetDescriptor());
}

}  // namespace analysis
}  // namespace verilog
