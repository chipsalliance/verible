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
#include "common/analysis/lint_rule_status.h"
#include "common/analysis/matcher/bound_symbol_manager.h"
#include "common/analysis/matcher/matcher.h"
#include "common/text/config_utils.h"
#include "common/text/symbol.h"
#include "common/text/syntax_tree_context.h"
#include "verilog/CST/verilog_matchers.h"
#include "verilog/analysis/descriptions.h"
#include "verilog/analysis/lint_rule_registry.h"

namespace verilog {
namespace analysis {

using verible::LintRuleStatus;
using verible::LintViolation;
using verible::matcher::Matcher;

// Register the lint rule
VERILOG_REGISTER_LINT_RULE(ForbiddenAnonymousStructsUnionsRule);

static const char kMessageStruct[] =
    "struct definitions always should be named using typedef.";
static const char kMessageUnion[] =
    "union definitions always should be named using typedef.";

const LintRuleDescriptor& ForbiddenAnonymousStructsUnionsRule::GetDescriptor() {
  static const LintRuleDescriptor d{
      .name = "typedef-structs-unions",
      .topic = "typedef-structs-unions",
      .desc =
          "Checks that a Verilog `struct` or `union` declaration is "
          "named using `typedef`.",
      .param = {{"allow_anonymous_nested", "false",
                 "Allow nested structs/unions to be anonymous."}},
  };
  return d;
}

absl::Status ForbiddenAnonymousStructsUnionsRule::Configure(
    absl::string_view configuration) {
  using verible::config::SetBool;
  return verible::ParseNameValues(
      configuration,
      {{"allow_anonymous_nested", SetBool(&allow_anonymous_nested_type_)}});
}

static const Matcher& StructMatcher() {
  static const Matcher matcher(NodekStructType());
  return matcher;
}

static const Matcher& UnionMatcher() {
  static const Matcher matcher(NodekUnionType());
  return matcher;
}

static bool IsPreceededByTypedef(const verible::SyntaxTreeContext& context) {
  return context.DirectParentsAre({NodeEnum::kDataTypePrimitive,
                                   NodeEnum::kDataType,
                                   NodeEnum::kTypeDeclaration});
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
  if (StructMatcher().Matches(symbol, &manager) && !IsRuleMet(context)) {
    violations_.insert(LintViolation(symbol, kMessageStruct, context));
  } else if (UnionMatcher().Matches(symbol, &manager) && !IsRuleMet(context)) {
    violations_.insert(LintViolation(symbol, kMessageUnion, context));
  }
}

LintRuleStatus ForbiddenAnonymousStructsUnionsRule::Report() const {
  return LintRuleStatus(violations_, GetDescriptor());
}

}  // namespace analysis
}  // namespace verilog
