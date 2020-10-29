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

#include "verilog/analysis/checkers/unpacked_dimensions_rule.h"

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

VERILOG_REGISTER_LINT_RULE(UnpackedDimensionsRule);

absl::string_view UnpackedDimensionsRule::Name() {
  return "unpacked-dimensions-range-ordering";
}
const char UnpackedDimensionsRule::kTopic[] = "unpacked-ordering";

const char kMessageScalarInOrder[] =
    "When an unpacked dimension range is zero-based ([0:N-1]), "
    "declare size as [N] instead.";
const char kMessageScalarReversed[] =
    "Unpacked dimension range must be declared in big-endian ([0:N-1]) order.  "
    "Declare zero-based big-endian unpacked dimensions sized as [N].";
const char kMessageReorder[] =
    "Declare unpacked dimension range in big-endian (increasing) order, "
    "e.g. [N:N+M].";

std::string UnpackedDimensionsRule::GetDescription(
    DescriptionType description_type) {
  return absl::StrCat(
      "Checks that unpacked dimension ranges are declared in big-endian "
      "order, ",
      Codify("[0:N-1]", description_type),
      " and when an unpacked dimension range is zero-based, ",
      Codify("[0:N-1]", description_type), ", the size is declared as ",
      Codify("[N]", description_type), " instead. See ",
      GetStyleGuideCitation(kTopic), ".");
}

static const Matcher& DimensionRangeMatcher() {
  static const Matcher matcher(NodekDimensionRange());
  return matcher;
}

void UnpackedDimensionsRule::HandleSymbol(
    const verible::Symbol& symbol, const verible::SyntaxTreeContext& context) {
  if (!ContextIsInsideUnpackedDimensions(context) ||
      context.IsInside(NodeEnum::kGateInstance))
    return;

  verible::matcher::BoundSymbolManager manager;
  if (DimensionRangeMatcher().Matches(symbol, &manager)) {
    // Check whether or not bounds are numeric constants, including 0.
    // If one can conclude that left > right, then record as violation.

    const auto& left = *ABSL_DIE_IF_NULL(GetDimensionRangeLeftBound(symbol));
    const auto& right = *ABSL_DIE_IF_NULL(GetDimensionRangeRightBound(symbol));
    int left_value, right_value;
    const bool left_is_constant = ConstantIntegerValue(left, &left_value);
    const bool right_is_constant = ConstantIntegerValue(right, &right_value);
    const bool left_is_zero = left_is_constant && (left_value == 0);
    const bool right_is_zero = right_is_constant && (right_value == 0);

    const verible::TokenInfo token(TK_OTHER,
                                   verible::StringSpanOfSymbol(left, right));
    if (left_is_zero) {
      violations_.insert(LintViolation(token, kMessageScalarInOrder, context));
    } else if (right_is_zero) {
      violations_.insert(LintViolation(token, kMessageScalarReversed, context));
    } else if (left_is_constant && right_is_constant &&
               left_value > right_value) {
      violations_.insert(LintViolation(token, kMessageReorder, context));
    }
  }
}

LintRuleStatus UnpackedDimensionsRule::Report() const {
  return LintRuleStatus(violations_, Name(), GetStyleGuideCitation(kTopic));
}

}  // namespace analysis
}  // namespace verilog
