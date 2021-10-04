// Copyright 2017-2021 The Verible Authors.
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

#include <cctype>
#include <cstddef>
#include <set>
#include <string>

#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "common/analysis/lint_rule_status.h"
#include "common/analysis/matcher/bound_symbol_manager.h"
#include "common/analysis/matcher/matcher.h"
#include "common/text/concrete_syntax_leaf.h"
#include "common/text/config_utils.h"
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

using verible::AutoFix;
using verible::down_cast;
using verible::LintRuleStatus;
using verible::LintViolation;
using verible::SyntaxTreeContext;
using verible::SyntaxTreeLeaf;
using verible::SyntaxTreeNode;
using verible::matcher::Matcher;

// Register UndersizedBinaryLiteralRule
VERILOG_REGISTER_LINT_RULE(UndersizedBinaryLiteralRule);

const LintRuleDescriptor& UndersizedBinaryLiteralRule::GetDescriptor() {
  static const LintRuleDescriptor d{
      .name = "undersized-numeric-literal",
      .topic = "number-literals",
      .desc =
          "Checks that the digits of binary literals for the configured "
          "bases match their declared width, i.e. has enough padding prefix "
          "zeros.",
      .param = {
          {"bin", "true", "Checking binary 'b literals."},
          {"oct", "false", "Checking octal 'o literals."},
          {"hex", "false", "Checking hexadecimal 'h literals."},
          {"lint_zero", "false",
           "Also generate a lint warning for value zero such as `32'h0`; "
           "autofix suggestions would be to zero-expand or untype `'0`."},
          {"autofix", "true",
           "Provide autofix suggestions, e.g. "
           "32'hAB provides suggested fix 32'h000000AB."},
      }};
  return d;
}

const std::vector<LintRuleAliasDescriptor>&
UndersizedBinaryLiteralRule::GetAliasDescriptors() {
  static const std::vector<LintRuleAliasDescriptor> d{
      {
          .name = "undersized-binary-literal",
          .param_defaults =
              {
                  {"bin", "true"},
              },
      },
  };
  return d;
}

// Broadly, start by matching all number nodes with a
// constant width and based literal.

static const Matcher& NumberMatcher() {
  static const Matcher matcher(
      NodekNumber(NumberHasConstantWidth().Bind("width"),
                  NumberHasBasedLiteral().Bind("literal")));
  return matcher;
}

void UndersizedBinaryLiteralRule::HandleSymbol(
    const verible::Symbol& symbol, const SyntaxTreeContext& context) {
  verible::matcher::BoundSymbolManager manager;
  if (!NumberMatcher().Matches(symbol, &manager)) return;
  const auto width_leaf = manager.GetAs<SyntaxTreeLeaf>("width");
  const auto literal_node = manager.GetAs<SyntaxTreeNode>("literal");
  if (!width_leaf || !literal_node) return;

  const auto width_text = width_leaf->get().text();
  size_t width;
  if (!absl::SimpleAtoi(width_text, &width)) return;

  const auto& base_digit_part = literal_node->children();
  auto base_leaf = down_cast<const SyntaxTreeLeaf*>(base_digit_part[0].get());
  auto digits_leaf = down_cast<const SyntaxTreeLeaf*>(base_digit_part[1].get());

  const auto base_text = base_leaf->get().text();
  const auto digits_text = digits_leaf->get().text();

  const BasedNumber number(base_text, digits_text);
  int bits_per_digit = 1;
  switch (number.base) {
    case 'd':
      return;  // Don't care about decimal values.
    case 'b':
      if (!check_bin_numbers_) return;
      bits_per_digit = 1;
      break;
    case 'o':
      if (!check_oct_numbers_) return;
      bits_per_digit = 3;
      break;
    case 'h':
      if (!check_hex_numbers_) return;
      bits_per_digit = 4;
      break;
    default:
      LOG(FATAL) << "Unexpected base '" << base_text << "'";  // Lexer issue ?
  }

  const int missing_bits = width - number.literal.length() * bits_per_digit;
  // if !lint_zero, "0" is an exceptions. Also "?" is always an exception
  if (missing_bits > 0 && (lint_zero_ || number.literal != "0") &&
      number.literal != "?") {
    std::vector<AutoFix> autofixes;

    // Special number zero (if lint_zero defined): suggest a '0 in this case
    if (number.literal == "0" && !number.signedness) {
      autofixes.push_back(
          AutoFix("Replace with unsized `0",
                  {{width_text, ""}, {base_text.substr(0, 2), "'"}}));
    }

    // Regular fix: prefix with leading zeroes.
    const int leading_0 = (missing_bits + bits_per_digit - 1) / bits_per_digit;
    autofixes.push_back(
        AutoFix("Left-expand leading zeroes",
                {{digits_text.substr(0, 0), std::string(leading_0, '0')}}));

    // For literals with small values that can be represented in one decimal
    // digit, this often might also be useful as decimal. Make this the final
    // suggestion.
    if (number.literal.size() == 1 && std::isdigit(number.literal[0])) {
      static const std::string desc = "Replace with decimal";
      if (number.signedness) {
        autofixes.push_back(AutoFix(desc, {{base_text.substr(0, 3), "'sd"}}));
      } else {
        autofixes.push_back(AutoFix(desc, {{base_text.substr(0, 2), "'d"}}));
      }
    }

    violations_.insert(LintViolation(
        digits_leaf->get(),
        FormatReason(width_text, base_text, number.base, digits_text), context,
        autofixes));
  }
}

// Generate string representation of why lint error occurred at leaf
std::string UndersizedBinaryLiteralRule::FormatReason(
    absl::string_view width, absl::string_view base_text, char base,
    absl::string_view literal) {
  absl::string_view base_describe;
  switch (base) {
    case 'b':
      base_describe = "Binary";
      break;
    case 'h':
      base_describe = "Hex";
      break;
    case 'o':
      base_describe = "Octal";
      break;
  }
  return absl::StrCat(base_describe, " literal ", width, base_text, literal,
                      " has less digits than expected for ", width, " bits.");
}

absl::Status UndersizedBinaryLiteralRule::Configure(
    absl::string_view configuration) {
  using verible::config::SetBool;
  return verible::ParseNameValues(configuration,
                                  {{"bin", SetBool(&check_bin_numbers_)},
                                   {"hex", SetBool(&check_hex_numbers_)},
                                   {"oct", SetBool(&check_oct_numbers_)},
                                   {"lint_zero", SetBool(&lint_zero_)},
                                   {"autofix", SetBool(&autofix_)}});
}

LintRuleStatus UndersizedBinaryLiteralRule::Report() const {
  return LintRuleStatus(violations_, GetDescriptor());
}

}  // namespace analysis
}  // namespace verilog
