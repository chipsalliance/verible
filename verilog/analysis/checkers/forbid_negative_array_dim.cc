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

#include "verilog/analysis/checkers/forbid_negative_array_dim.h"

#include <set>
#include <string>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "common/analysis/lint_rule_status.h"
#include "common/analysis/matcher/bound_symbol_manager.h"
#include "common/analysis/matcher/matcher.h"
#include "common/text/symbol.h"
#include "common/text/syntax_tree_context.h"
#include "common/text/tree_utils.h"
#include "common/util/logging.h"
#include "verilog/CST/context_functions.h"
#include "verilog/CST/dimensions.h"
#include "verilog/CST/expression.h"
#include "verilog/CST/verilog_matchers.h"
#include "verilog/analysis/descriptions.h"
#include "verilog/analysis/lint_rule_registry.h"

namespace verilog {
namespace analysis {

using verible::matcher::Matcher;

// Register the lint rule
VERILOG_REGISTER_LINT_RULE(ForbidNegativeArrayDim);

static constexpr absl::string_view kMessage =
    "Avoid using negative constant literals for array dimensions.";

const LintRuleDescriptor& ForbidNegativeArrayDim::GetDescriptor() {
  static const LintRuleDescriptor d{
      .name = "forbid-negative-array-dim",
      .topic = "forbid-negative-array-dim",
      .desc = "Check for negative constant literals inside array dimensions.",
  };
  return d;
}

// Matches the begin node.
static const Matcher& UnaryPrefixExprMatcher() {
  static const Matcher matcher(NodekUnaryPrefixExpression());
  return matcher;
}

void ForbidNegativeArrayDim::HandleSymbol(
    const verible::Symbol& symbol, const verible::SyntaxTreeContext& context) {
  if (!context.IsInsideFirst(
          {NodeEnum::kPackedDimensions, NodeEnum::kUnpackedDimensions},
          {NodeEnum::kBinaryExpression, NodeEnum::kUnaryPrefixExpression})) {
    return;
  }

  verible::matcher::BoundSymbolManager manager;
  if (UnaryPrefixExprMatcher().Matches(symbol, &manager)) {
    const auto& node = SymbolCastToNode(symbol);
    const auto& children = node.children();
    const auto& leaf_symbol = *children[0].get();
    const auto& term =
        verible::down_cast<const verible::SyntaxTreeLeaf&>(leaf_symbol);

    // mandatory? {leaf, node}
    if (children.size() <= 1) return;

    int value = -1;
    const auto& child_node = children[1];
    const bool is_constant = ConstantIntegerValue(*child_node.get(), &value);

    const verible::TokenInfo token(TK_OTHER,
                                   verible::StringSpanOfSymbol(symbol));
    if (is_constant && value > 0 && term.get().text() == "-") {
      violations_.insert(verible::LintViolation(token, kMessage, context));
    }
  }
}

verible::LintRuleStatus ForbidNegativeArrayDim::Report() const {
  return verible::LintRuleStatus(violations_, GetDescriptor());
}

}  // namespace analysis
}  // namespace verilog
