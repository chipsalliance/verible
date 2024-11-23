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

#include "verible/verilog/analysis/checkers/forbid-line-continuations-rule.h"

#include <algorithm>
#include <set>

#include "absl/strings/match.h"
#include "absl/strings/string_view.h"
#include "verible/common/analysis/lint-rule-status.h"
#include "verible/common/analysis/matcher/bound-symbol-manager.h"
#include "verible/common/analysis/matcher/matcher.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/syntax-tree-context.h"
#include "verible/common/text/tree-utils.h"
#include "verible/verilog/CST/verilog-matchers.h"  // IWYU pragma: keep
#include "verible/verilog/analysis/descriptions.h"
#include "verible/verilog/analysis/lint-rule-registry.h"
#include "verible/verilog/parser/verilog-token-enum.h"

namespace verilog {
namespace analysis {

using verible::LintRuleStatus;
using verible::LintViolation;
using verible::SyntaxTreeContext;
using verible::matcher::Matcher;

// Register ForbidLineContinuationsRule
VERILOG_REGISTER_LINT_RULE(ForbidLineContinuationsRule);

static constexpr absl::string_view kMessage =
    "The lines can't be continued with \'\\\', use concatenation operator with "
    "braces";

const LintRuleDescriptor &ForbidLineContinuationsRule::GetDescriptor() {
  static const LintRuleDescriptor d{
      .name = "forbid-line-continuations",
      .topic = "forbid-line-continuations",
      .desc =
          "Checks that there are no occurrences of `\\` when breaking the "
          "string literal line. Use concatenation operator with braces "
          "instead.",
  };
  return d;
}

static const Matcher &StringLiteralMatcher() {
  static const Matcher matcher(StringLiteralKeyword());
  return matcher;
}

void ForbidLineContinuationsRule::HandleSymbol(
    const verible::Symbol &symbol, const SyntaxTreeContext &context) {
  verible::matcher::BoundSymbolManager manager;
  if (!StringLiteralMatcher().Matches(symbol, &manager)) {
    return;
  }
  const auto &string_node = SymbolCastToNode(symbol);
  const auto &node_children = string_node.children();
  const auto &literal = std::find_if(node_children.begin(), node_children.end(),
                                     [](const verible::SymbolPtr &p) {
                                       return p->Tag().tag == TK_StringLiteral;
                                     });
  const auto &string_literal = SymbolCastToLeaf(**literal);
  if (absl::StrContains(string_literal.get().text(), "\\\n") ||
      absl::StrContains(string_literal.get().text(), "\\\r")) {
    violations_.insert(LintViolation(string_literal, kMessage, context));
  }
}

LintRuleStatus ForbidLineContinuationsRule::Report() const {
  return LintRuleStatus(violations_, GetDescriptor());
}

}  // namespace analysis
}  // namespace verilog
