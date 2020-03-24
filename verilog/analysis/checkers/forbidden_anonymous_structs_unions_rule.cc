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

#include "verilog/analysis/checkers/forbidden_anonymous_structs_unions_rule.h"

#include <set>
#include <string>

#include "absl/strings/match.h"
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

// Register the lint rule
VERILOG_REGISTER_LINT_RULE(ForbiddenAnonymousStructsUnionsRule);

absl::string_view ForbiddenAnonymousStructsUnionsRule::Name() {
  return "typedef-structs-unions";
}
const char ForbiddenAnonymousStructsUnionsRule::kTopic[] =
    "typedef-structs-unions";
const char ForbiddenAnonymousStructsUnionsRule::kMessageStruct[] =
    "struct definitions always should be named using typedef.";
const char ForbiddenAnonymousStructsUnionsRule::kMessageUnion[] =
    "union definitions always should be named using typedef.";

std::string ForbiddenAnonymousStructsUnionsRule::GetDescription(
    DescriptionType description_type) {
  static std::string basic_desc = absl::StrCat(
      "Checks that a Verilog ", Codify("struct", description_type), " or ",
      Codify("union", description_type), " declaration is named using ",
      Codify("typedef", description_type), ". See ",
      GetStyleGuideCitation(kTopic), ".\n");
  if (description_type == DescriptionType::kHelpRulesFlag) {
    return absl::StrCat(basic_desc, "Parameters: allow_anonymous_nested:false");
  } else {
    return absl::StrCat(basic_desc, "##### Parameters\n",
                        "  * allow_anonymous_nested Default: false");
  }
}

absl::Status ForbiddenAnonymousStructsUnionsRule::Configure(
    absl::string_view configuration) {
  if (configuration.empty()) return absl::OkStatus();
  // TODO(hzeller): make general parser of lint rule configuration parameters
  // to avoid ad-hoc parsings such as the following.
  constexpr char kParamName[] = "allow_anonymous_nested";
  if (absl::StartsWith(configuration, kParamName)) {
    const absl::string_view suffix = configuration.substr(strlen(kParamName));
    allow_anonymous_nested_type_ = suffix.empty() || suffix == ":true";
    if (!allow_anonymous_nested_type_ && suffix != ":false") {
      return absl::InvalidArgumentError(
          "allow_anonymous_nested allowed value: "
          "'true' or 'false'");
    }
    return absl::OkStatus();
  }
  return absl::InvalidArgumentError(
      absl::StrCat("Only supported parameter 'allow_anonymous_nested'; got '",
                   configuration, "'"));
}

static bool IsPreceededByTypedef(const verible::SyntaxTreeContext& context) {
  return context.DirectParentsAre(
      {NodeEnum::kDataTypePrimitive, NodeEnum::kTypeDeclaration});
}

static bool NestedInStructOrUnion(const verible::SyntaxTreeContext& context) {
  return context.IsInsideStartingFrom(NodeEnum::kDataTypePrimitive, 1);
}

bool ForbiddenAnonymousStructsUnionsRule::IsRuleMet(
    const verible::SyntaxTreeContext& context) const {
  return IsPreceededByTypedef(context) ||
         (allow_anonymous_nested_type_ && NestedInStructOrUnion(context));
}

void ForbiddenAnonymousStructsUnionsRule::HandleSymbol(
    const verible::Symbol& symbol, const verible::SyntaxTreeContext& context) {
  verible::matcher::BoundSymbolManager manager;
  if (matcher_struct_.Matches(symbol, &manager) && !IsRuleMet(context)) {
    violations_.insert(LintViolation(symbol, kMessageStruct, context));
  } else if (matcher_union_.Matches(symbol, &manager) && !IsRuleMet(context)) {
    violations_.insert(LintViolation(symbol, kMessageUnion, context));
  }
}

LintRuleStatus ForbiddenAnonymousStructsUnionsRule::Report() const {
  return LintRuleStatus(violations_, Name(), GetStyleGuideCitation(kTopic));
}

}  // namespace analysis
}  // namespace verilog
