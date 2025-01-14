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

#include "verible/verilog/analysis/checkers/truncated-numeric-literal-rule.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <set>
#include <string>
#include <string_view>

#include "absl/numeric/int128.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "verible/common/analysis/lint-rule-status.h"
#include "verible/common/analysis/matcher/bound-symbol-manager.h"
#include "verible/common/analysis/matcher/matcher.h"
#include "verible/common/text/concrete-syntax-leaf.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/syntax-tree-context.h"
#include "verible/common/text/token-info.h"
#include "verible/verilog/CST/numbers.h"
#include "verible/verilog/CST/verilog-matchers.h"
#include "verible/verilog/analysis/descriptions.h"
#include "verible/verilog/analysis/lint-rule-registry.h"

namespace verilog {
namespace analysis {

using verible::down_cast;
using verible::LintRuleStatus;
using verible::LintViolation;
using verible::SyntaxTreeContext;
using verible::SyntaxTreeLeaf;
using verible::SyntaxTreeNode;
using verible::matcher::Matcher;

VERILOG_REGISTER_LINT_RULE(TruncatedNumericLiteralRule);

const LintRuleDescriptor &TruncatedNumericLiteralRule::GetDescriptor() {
  static const LintRuleDescriptor d{
      .name = "truncated-numeric-literal",
      .topic = "number-literals",
      .desc =
          "Checks that numeric literals are not longer than their stated "
          "bit-width to avoid undesired accidental truncation.",
  };
  return d;
}

static const Matcher &NumberMatcher() {
  static const Matcher matcher(
      NodekNumber(NumberHasConstantWidth().Bind("width"),
                  NumberHasBasedLiteral().Bind("literal")));
  return matcher;
}

// Given a binary/oct/hex digit, return how many bits it occupies
static int digitBits(char digit, bool *is_lower_bound) {
  if (digit == 'z' || digit == 'x' || digit == '?') {
    *is_lower_bound = true;
    return 1;  // Minimum number of bits assumed
  }
  *is_lower_bound = false;
  if (digit > '7') return 4;
  if (digit > '3') return 3;
  if (digit > '1') return 2;
  return 1;
}

static std::string_view StripLeadingZeroes(std::string_view str) {
  const std::string_view::const_iterator it =
      std::find_if_not(str.begin(), str.end(), [](char c) { return c == '0'; });
  return str.substr(it - str.begin());
}

// Return count of bits the given number occupies. Sometims we can only make
// a lower bound estimate, return that in "is_lower_bound".
static size_t GetBitWidthOfNumber(const BasedNumber &n, bool *is_lower_bound) {
  const std::string_view literal = StripLeadingZeroes(n.literal);

  *is_lower_bound = true;           // Can only estimate for the following two
  if (literal.empty()) return 1;    // all zeroes
  if (literal[0] == '`') return 1;  // Not dealing with macros.

  *is_lower_bound = false;  // Now, we strive for exact bits
  switch (n.base) {
    case 'h':
      return digitBits(literal[0], is_lower_bound) + 4 * (literal.length() - 1);
    case 'o':
      return digitBits(literal[0], is_lower_bound) + 3 * (literal.length() - 1);
    case 'b':
      return literal.length();
    case 'd': {
      if (!isdigit(literal[0])) {
        *is_lower_bound = true;
        return 1;  // Not dealing with ? or z
      }

      // Let's first try if we can parse it with regular means. Luckily,
      // absl provides compiler-independent abstraction of 128 bit numbers,
      // so we can parse most commonly used values accurately.
      absl::uint128 number;
      if (absl::SimpleAtoi(literal, &number)) {
        int bits;
        for (bits = 0; bits < 128 && number; ++bits) {
          number >>= 1;
        }
        return bits;
      }

      *is_lower_bound = true;  // Heuristic below only gives us a lower bound

      // More than 128 bits. Best effort to establish at least a lower bound.

      // We parse the number as double, so that parsing can keep track of
      // pretty long numbers and we get log2 for 15-ish significant dec digits.

      // This will create false negatives, i.e. undercounting required bits,

      // TODO(hzeller): is there a cheap way to accurately determine bits used
      // without fully parsing the decimal number ?
      double v;
      if (absl::SimpleAtod(literal, &v) && !std::isinf(v)) {
        return std::max(129, static_cast<int>(ceil(log(v) / log(2))));
      }

      // Uh, more than 300-ish decimal digits ? ... rough estimation it is.
      return ceil((literal.length() - 1) * log(10) / log(2));
    } break;
    default:
      break;  // unexpected base
  }
  return 0;  // not reached.
}

void TruncatedNumericLiteralRule::HandleSymbol(
    const verible::Symbol &symbol, const SyntaxTreeContext &context) {
  verible::matcher::BoundSymbolManager manager;
  if (!NumberMatcher().Matches(symbol, &manager)) return;
  const auto *width_leaf = manager.GetAs<SyntaxTreeLeaf>("width");
  const auto *literal_node = manager.GetAs<SyntaxTreeNode>("literal");
  if (!width_leaf || !literal_node) return;

  const auto width_text = width_leaf->get().text();
  size_t width;
  if (!absl::SimpleAtoi(width_text, &width)) return;

  const auto *base_leaf =
      down_cast<const SyntaxTreeLeaf *>((*literal_node)[0].get());
  const auto *digits_leaf =
      down_cast<const SyntaxTreeLeaf *>((*literal_node)[1].get());

  const auto base_text = base_leaf->get().text();
  const auto digits_text = digits_leaf->get().text();

  const BasedNumber number(base_text, digits_text);

  bool is_lower_bound = false;
  const size_t actual_width = GetBitWidthOfNumber(number, &is_lower_bound);

  if (actual_width > width) {
    violations_.insert(LintViolation(
        digits_leaf->get(),
        absl::StrCat("Number ", width_text, base_text, digits_text,
                     " occupies ", is_lower_bound ? "at least " : "",
                     actual_width, " bits, truncated to ", width, " bits."),
        context));
    // No autofix yet. In particular signed numbers might be hairy, and
    // numbers for which we only have a lower bound.
  }
}

LintRuleStatus TruncatedNumericLiteralRule::Report() const {
  return LintRuleStatus(violations_, GetDescriptor());
}

}  // namespace analysis
}  // namespace verilog
