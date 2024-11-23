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

#include "verible/verilog/analysis/checkers/packed-dimensions-rule.h"

#include <set>

#include "absl/strings/string_view.h"
#include "verible/common/analysis/lint-rule-status.h"
#include "verible/common/analysis/matcher/bound-symbol-manager.h"
#include "verible/common/analysis/matcher/matcher.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/syntax-tree-context.h"
#include "verible/common/text/token-info.h"
#include "verible/common/text/tree-utils.h"
#include "verible/common/util/logging.h"
#include "verible/verilog/CST/context-functions.h"
#include "verible/verilog/CST/dimensions.h"
#include "verible/verilog/CST/expression.h"
#include "verible/verilog/CST/verilog-matchers.h"
#include "verible/verilog/analysis/descriptions.h"
#include "verible/verilog/analysis/lint-rule-registry.h"
#include "verible/verilog/parser/verilog-token-enum.h"

namespace verilog {
namespace analysis {

using verible::LintRuleStatus;
using verible::LintViolation;
using verible::matcher::Matcher;

VERILOG_REGISTER_LINT_RULE(PackedDimensionsRule);

static constexpr absl::string_view kMessage =
    "Declare packed dimension range in little-endian (decreasing) order, "
    "e.g. [N-1:0].";

const LintRuleDescriptor &PackedDimensionsRule::GetDescriptor() {
  static const LintRuleDescriptor d{
      .name = "packed-dimensions-range-ordering",
      .topic = "packed-ordering",
      .desc =
          "Checks that packed dimension ranges are declare in little-endian "
          "(decreasing) order, e.g. `[N-1:0]`.",
  };
  return d;
}

static const Matcher &DimensionRangeMatcher() {
  static const Matcher matcher(NodekDimensionRange());
  return matcher;
}

void PackedDimensionsRule::HandleSymbol(
    const verible::Symbol &symbol, const verible::SyntaxTreeContext &context) {
  if (!ContextIsInsidePackedDimensions(context)) return;

  verible::matcher::BoundSymbolManager manager;
  if (DimensionRangeMatcher().Matches(symbol, &manager)) {
    // Check whether or not bounds are numeric constants, including 0.
    // If one can conclude that left < right, then record as violation.

    const auto &left = *ABSL_DIE_IF_NULL(GetDimensionRangeLeftBound(symbol));
    const auto &right = *ABSL_DIE_IF_NULL(GetDimensionRangeRightBound(symbol));
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
  return LintRuleStatus(violations_, GetDescriptor());
}

}  // namespace analysis
}  // namespace verilog
