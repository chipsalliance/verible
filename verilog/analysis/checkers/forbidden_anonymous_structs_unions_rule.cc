// Copyright 2017-2019 The Verible Authors.
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

#include "verilog/analysis/checkers/forbidden_anonymous_structs_unions_rule.h"

#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "common/analysis/citation.h"
#include "common/analysis/lint_rule_status.h"
#include "common/analysis/matcher/bound_symbol_manager.h"
#include "common/text/symbol.h"
#include "common/text/syntax_tree_context.h"
#include "verilog/analysis/descriptions.h"
#include "verilog/analysis/lint_rule_registry.h"

namespace verilog {
namespace analysis {

using verible::GetStyleGuideCitation;
using verible::LintRuleStatus;
using verible::LintViolation;
using verible::TokenInfo;
using verible::TokenStreamLintRule;

// Register the lint rule
VERILOG_REGISTER_LINT_RULE(ForbiddenAnonymousStructsUnionsRule);

absl::string_view ForbiddenAnonymousStructsUnionsRule::Name() { return "typedef-structs-unions"; }
const char ForbiddenAnonymousStructsUnionsRule::kTopic[] = "typedef-structs-unions";
const char ForbiddenAnonymousStructsUnionsRule::kMessageStruct[] =
    "struct definitions always should be named using typedef.";
const char ForbiddenAnonymousStructsUnionsRule::kMessageUnion[] =
    "union definitions always should be named using typedef.";

std::string ForbiddenAnonymousStructsUnionsRule::GetDescription(DescriptionType description_type) {
  return absl::StrCat("Checks that a Verilog ",
                      Codify("struct", description_type),
                      " or ",
                      Codify("union", description_type),
                      " declaration is named using ",
                      Codify("typedef", description_type), ". See ",
                      GetStyleGuideCitation(kTopic), ".");
}

void ForbiddenAnonymousStructsUnionsRule::HandleSymbol(const verible::Symbol& symbol,
                  const verible::SyntaxTreeContext& context) {
  verible::matcher::BoundSymbolManager manager;
  if (matcher_struct_.Matches(symbol, &manager)) {
    // Check if it is preceded by a typedef
    if (!context.DirectParentsAre(
          {NodeEnum::kDataTypePrimitive,
           NodeEnum::kTypeDeclaration})) {
      violations_.push_back(LintViolation(
        symbol, kMessageStruct, context));
    }
  } else if (matcher_union_.Matches(symbol, &manager)) {
    // Check if it is preceded by a typedef
    if (!context.DirectParentsAre(
          {NodeEnum::kDataTypePrimitive,
           NodeEnum::kTypeDeclaration})) {
      violations_.push_back(LintViolation(
        symbol, kMessageUnion, context));
    }
  }
}

LintRuleStatus ForbiddenAnonymousStructsUnionsRule::Report() const {
  return LintRuleStatus(violations_, Name(), GetStyleGuideCitation(kTopic));
}

}  // namespace analysis
}  // namespace verilog
