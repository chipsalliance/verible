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

#include "verible/verilog/analysis/checkers/forbidden-anonymous-enums-rule.h"

#include <set>

#include "absl/strings/string_view.h"
#include "verible/common/analysis/lint-rule-status.h"
#include "verible/common/analysis/matcher/bound-symbol-manager.h"
#include "verible/common/analysis/matcher/matcher.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/syntax-tree-context.h"
#include "verible/verilog/CST/verilog-matchers.h"
#include "verible/verilog/CST/verilog-nonterminals.h"
#include "verible/verilog/analysis/descriptions.h"
#include "verible/verilog/analysis/lint-rule-registry.h"

namespace verilog {
namespace analysis {

using verible::LintRuleStatus;
using verible::LintViolation;
using Matcher = verible::matcher::Matcher;

// Register the lint rule
VERILOG_REGISTER_LINT_RULE(ForbiddenAnonymousEnumsRule);

static constexpr absl::string_view kMessage =
    "enum types always should be named using typedef.";

const LintRuleDescriptor &ForbiddenAnonymousEnumsRule::GetDescriptor() {
  static const LintRuleDescriptor d{
      .name = "typedef-enums",
      .topic = "typedef-enums",
      .desc =
          "Checks that a Verilog `enum` declaration is named using "
          "`typedef`.",
  };
  return d;
}

static const Matcher &EnumMatcher() {
  static const Matcher matcher(NodekEnumType());
  return matcher;
}

void ForbiddenAnonymousEnumsRule::HandleSymbol(
    const verible::Symbol &symbol, const verible::SyntaxTreeContext &context) {
  verible::matcher::BoundSymbolManager manager;
  if (EnumMatcher().Matches(symbol, &manager)) {
    // Check if it is preceded by a typedef
    if (!context.DirectParentsAre({NodeEnum::kDataTypePrimitive,
                                   NodeEnum::kDataType,
                                   NodeEnum::kTypeDeclaration})) {
      violations_.insert(LintViolation(symbol, kMessage, context));
    }
  }
}

LintRuleStatus ForbiddenAnonymousEnumsRule::Report() const {
  return LintRuleStatus(violations_, GetDescriptor());
}

}  // namespace analysis
}  // namespace verilog
