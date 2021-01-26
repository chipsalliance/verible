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

#include "verilog/analysis/checkers/undersized_binary_literal_rule.h"

#include <cstddef>
#include <set>
#include <string>

#include "absl/strings/numbers.h"
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
#include "common/util/logging.h"
#include "verilog/CST/numbers.h"
#include "verilog/CST/verilog_matchers.h"
#include "verilog/analysis/descriptions.h"
#include "verilog/analysis/lint_rule_registry.h"

namespace verilog {
namespace analysis {

using verible::GetStyleGuideCitation;
using verible::LintRuleStatus;
using verible::LintViolation;
using verible::SyntaxTreeContext;
using verible::SyntaxTreeLeaf;
using verible::matcher::Matcher;

// Register UndersizedBinaryLiteralRule
VERILOG_REGISTER_LINT_RULE(UndersizedBinaryLiteralRule);

absl::string_view UndersizedBinaryLiteralRule::Name() {
  return "undersized-binary-literal";
}
const char UndersizedBinaryLiteralRule::kTopic[] = "number-literals";

std::string UndersizedBinaryLiteralRule::GetDescription(
    DescriptionType description_type) {
  return absl::StrCat(
      "Checks that the digits of binary literals match their declared "
      "width. See ",
      GetStyleGuideCitation(kTopic), ".");
}

// Broadly, start by matching all number nodes with a
// constant width and based literal.
// TODO(fangism): If more precision is needed than what the inner matcher
// provides, pass a more specific predicate matching function instead.

static const Matcher& NumberMatcher() {
  static const Matcher matcher(NodekNumber(
      NumberHasConstantWidth().Bind("width"),
      NumberHasBasedLiteral(NumberIsBinary().Bind("base"),
                            NumberHasBinaryDigits().Bind("digits"))));
  return matcher;
}

void UndersizedBinaryLiteralRule::HandleSymbol(
    const verible::Symbol& symbol, const SyntaxTreeContext& context) {
  verible::matcher::BoundSymbolManager manager;
  if (NumberMatcher().Matches(symbol, &manager)) {
    if (auto width_leaf = manager.GetAs<SyntaxTreeLeaf>("width")) {
      if (auto base_leaf = manager.GetAs<SyntaxTreeLeaf>("base")) {
        if (auto digits_leaf = manager.GetAs<SyntaxTreeLeaf>("digits")) {
          auto width_text = width_leaf->get().text();
          auto base_text = base_leaf->get().text();
          auto digits_text = digits_leaf->get().text();
          size_t width;
          if (absl::SimpleAtoi(width_text, &width)) {
            const BasedNumber number(base_text, digits_text);
            CHECK(number.ok)
                << "Expecting valid numeric literal from lexer, but got: "
                << digits_text;
            // Detect binary values, whose literal width is shorter than the
            // declared width.
            // Allow 'b0 and 'b? as an exception.
            CHECK_EQ(number.base,
                     'b');  // guaranteed by matching TK_BinBase
            if (width > number.literal.length() && number.literal != "0" &&
                number.literal != "?") {
              violations_.insert(LintViolation(
                  digits_leaf->get(),
                  FormatReason(width_text, base_text, digits_text), context));
            }
          }  // else width is not constant, so ignore
        }
      }
    }
  }
}

// Generate string representation of why lint error occurred at leaf
std::string UndersizedBinaryLiteralRule::FormatReason(
    absl::string_view width, absl::string_view base,
    absl::string_view literal) {
  return absl::StrCat("Binary literal ", width, base, literal,
                      " is shorter than its declared width: ", width, ".");
}

LintRuleStatus UndersizedBinaryLiteralRule::Report() const {
  return LintRuleStatus(violations_, Name(), GetStyleGuideCitation(kTopic));
}

}  // namespace analysis
}  // namespace verilog
