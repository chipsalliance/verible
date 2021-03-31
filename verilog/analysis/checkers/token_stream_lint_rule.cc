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

#include "verilog/analysis/checkers/token_stream_lint_rule.h"

#include <algorithm>
#include <set>
#include <string>

#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "common/analysis/citation.h"
#include "common/analysis/lint_rule_status.h"
#include "common/analysis/matcher/bound_symbol_manager.h"
#include "common/analysis/matcher/matcher.h"
#include "common/text/symbol.h"
#include "common/text/syntax_tree_context.h"
#include "verilog/CST/verilog_matchers.h"  // IWYU pragma: keep
#include "verilog/analysis/descriptions.h"
#include "verilog/analysis/lint_rule_registry.h"

namespace verilog {
namespace analysis {

using verible::GetStyleGuideCitation;
using verible::LintRuleStatus;
using verible::LintViolation;
using verible::SyntaxTreeContext;
using verible::matcher::Matcher;

// Register TokenStreamLintRule
VERILOG_REGISTER_LINT_RULE(TokenStreamLintRule);

absl::string_view TokenStreamLintRule::Name() {
  return "forbid-line-continuations";
}
const char TokenStreamLintRule::kTopic[] = "forbid-line-continuations";
const char TokenStreamLintRule::kMessage[] =
    "The lines can't be continued with \'\\\', use concatenation operator with "
    "braces";

std::string TokenStreamLintRule::GetDescription(
    DescriptionType description_type) {
  return absl::StrCat("Checks that there are no occurrences of ",
                      Codify("\'\\\'", description_type),
                      " when breaking the string literal line. ",
                      "Use concatenation operator with braces instead. See ",
                      GetStyleGuideCitation(kTopic), ".");
}

static const Matcher& StringLiteralMatcher() {
  static const Matcher matcher(StringLiteralKeyword());
  return matcher;
}

void TokenStreamLintRule::HandleSymbol(const verible::Symbol& symbol,
                                       const SyntaxTreeContext& context) {
  verible::matcher::BoundSymbolManager manager;
  if (!StringLiteralMatcher().Matches(symbol, &manager)) {
    return;
  }
  const auto& string_node = SymbolCastToNode(symbol);
  const auto& node_children = string_node.children();
  const auto& literal = std::find_if(node_children.begin(), node_children.end(),
                                     [](const verible::SymbolPtr& p) {
                                       return p->Tag().tag == TK_StringLiteral;
                                     });
  const auto& string_literal = SymbolCastToLeaf(**literal);
  if (absl::StrContains(string_literal.get().text(), "\\\n")) {
    violations_.insert(LintViolation(string_literal, kMessage, context));
  }
}

LintRuleStatus TokenStreamLintRule::Report() const {
  return LintRuleStatus(violations_, Name(), GetStyleGuideCitation(kTopic));
}

}  // namespace analysis
}  // namespace verilog
