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

#include "verible/verilog/analysis/checkers/unpacked-dimensions-rule.h"

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
#include "verible/verilog/CST/verilog-nonterminals.h"
#include "verible/verilog/analysis/descriptions.h"
#include "verible/verilog/analysis/lint-rule-registry.h"
#include "verible/verilog/parser/verilog-token-enum.h"

namespace verilog {
namespace analysis {

using verible::LintRuleStatus;
using verible::LintViolation;
using verible::matcher::Matcher;

VERILOG_REGISTER_LINT_RULE(UnpackedDimensionsRule);

static constexpr absl::string_view kMessageScalarInOrder =
    "When an unpacked dimension range is zero-based ([0:N-1]), "
    "declare size as [N] instead.";
static constexpr absl::string_view kMessageScalarReversed =
    "Unpacked dimension range must be declared in big-endian ([0:N-1]) order.  "
    "Declare zero-based big-endian unpacked dimensions sized as [N].";
static constexpr absl::string_view kMessageReorder =
    "Declare unpacked dimension range in big-endian (increasing) order, "
    "e.g. [N:N+M].";

const LintRuleDescriptor &UnpackedDimensionsRule::GetDescriptor() {
  static const LintRuleDescriptor d{
      .name = "unpacked-dimensions-range-ordering",
      .topic = "unpacked-ordering",
      .desc =
          "Checks that unpacked dimension ranges are declared in "
          "big-endian order `[0:N-1]`, "
          "and when an unpacked dimension range is zero-based "
          "`[0:N-1]`, the size is declared as `[N]` instead.",
  };
  return d;
}

static const Matcher &DimensionRangeMatcher() {
  static const Matcher matcher(NodekDimensionRange());
  return matcher;
}

void UnpackedDimensionsRule::HandleSymbol(
    const verible::Symbol &symbol, const verible::SyntaxTreeContext &context) {
  if (!ContextIsInsideUnpackedDimensions(context) ||
      context.IsInside(NodeEnum::kGateInstance)) {
    return;
  }
  verible::matcher::BoundSymbolManager manager;
  if (DimensionRangeMatcher().Matches(symbol, &manager)) {
    // Check whether or not bounds are numeric constants, including 0.
    // If one can conclude that left > right, then record as violation.

    const auto &left = *ABSL_DIE_IF_NULL(GetDimensionRangeLeftBound(symbol));
    const auto &right = *ABSL_DIE_IF_NULL(GetDimensionRangeRightBound(symbol));
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
  return LintRuleStatus(violations_, GetDescriptor());
}

}  // namespace analysis
}  // namespace verilog
