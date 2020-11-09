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

#include "verilog/analysis/checkers/packed_dimensions_rule.h"

#include <algorithm>  // for std::distance
#include <set>
#include <string>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "common/analysis/citation.h"
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

using verible::GetStyleGuideCitation;
using verible::LintRuleStatus;
using verible::LintViolation;
using verible::SyntaxTreeContext;
using verible::matcher::Matcher;

VERILOG_REGISTER_LINT_RULE(PackedDimensionsRule);

absl::string_view PackedDimensionsRule::Name() {
  return "packed-dimensions-range-ordering";
}
const char PackedDimensionsRule::kTopic[] = "packed-ordering";
const char PackedDimensionsRule::kMessage[] =
    "Declare packed dimension range in little-endian (decreasing) order, "
    "e.g. [N-1:0].";

std::string PackedDimensionsRule::GetDescription(
    DescriptionType description_type) {
  return absl::StrCat(
      "Checks that packed dimension ranges are declare in little-endian "
      "(decreasing) order, e.g. ",
      Codify("[N-1:0]", description_type), ". See ",
      GetStyleGuideCitation(kTopic), ".");
}

static const Matcher& DimensionRangeMatcher() {
  static const Matcher matcher(NodekDimensionRange());
  return matcher;
}

void PackedDimensionsRule::HandleSymbol(
    const verible::Symbol& symbol, const verible::SyntaxTreeContext& context) {
  if (!ContextIsInsidePackedDimensions(context)) return;

  verible::matcher::BoundSymbolManager manager;
  if (DimensionRangeMatcher().Matches(symbol, &manager)) {
    // Check whether or not bounds are numeric constants, including 0.
    // If one can conclude that left < right, then record as violation.

    const auto& left = *ABSL_DIE_IF_NULL(GetDimensionRangeLeftBound(symbol));
    const auto& right = *ABSL_DIE_IF_NULL(GetDimensionRangeRightBound(symbol));
    int left_value, right_value;
    const bool left_is_constant = ConstantIntegerValue(left, &left_value);
    const bool right_is_constant = ConstantIntegerValue(right, &right_value);
    const bool left_is_zero = left_is_constant && (left_value == 0);
    const bool right_is_zero = right_is_constant && (right_value == 0);

    if ((left_is_zero && !right_is_zero) ||
        (left_is_constant && right_is_constant && left_value < right_value)) {
      const verible::TokenInfo token(TK_OTHER,
                                     verible::StringSpanOfSymbol(left, right));
      violations_.insert(LintViolation(token, kMessage, context));
    }
  }
}

LintRuleStatus PackedDimensionsRule::Report() const {
  return LintRuleStatus(violations_, Name(), GetStyleGuideCitation(kTopic));
}

}  // namespace analysis
}  // namespace verilog
