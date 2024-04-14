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

#include "verilog/analysis/checkers/explicit_parameter_storage_type_rule.h"

#include <set>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "common/analysis/lint_rule_status.h"
#include "common/analysis/matcher/bound_symbol_manager.h"
#include "common/analysis/matcher/matcher.h"
#include "common/text/config_utils.h"
#include "common/text/symbol.h"
#include "common/text/syntax_tree_context.h"
#include "common/text/token_info.h"
#include "common/text/tree_utils.h"
#include "common/util/logging.h"
#include "verilog/CST/parameters.h"
#include "verilog/CST/verilog_matchers.h"
#include "verilog/analysis/descriptions.h"
#include "verilog/analysis/lint_rule_registry.h"
#include "verilog/parser/verilog_token_enum.h"

namespace verilog {
namespace analysis {

using verible::LintRuleStatus;
using verible::LintViolation;
using verible::SyntaxTreeContext;
using Matcher = verible::matcher::Matcher;

// Register ExplicitParameterStorageTypeRule
VERILOG_REGISTER_LINT_RULE(ExplicitParameterStorageTypeRule);

static constexpr absl::string_view kMessage =
    "Explicitly define a storage type for every parameter and localparam, ";

const LintRuleDescriptor &ExplicitParameterStorageTypeRule::GetDescriptor() {
  static const LintRuleDescriptor d{
      .name = "explicit-parameter-storage-type",
      .topic = "constants",
      .desc =
          "Checks that every `parameter` and `localparam` "
          "is declared with an explicit storage type.",
      .param = {{"exempt_type", "", "Set to `string` to exempt string types"}},
  };
  return d;
}

static const Matcher &ParamMatcher() {
  static const Matcher matcher(NodekParamDeclaration());
  return matcher;
}

static bool HasStringAssignment(const verible::Symbol &param_decl) {
  const auto *s = GetParamAssignExpression(param_decl);
  if (s == nullptr) return false;
  // We can't really do expression evaluation and determine the type of the
  // RHS, so here we focus on the simple case in which the RHS is a
  // string literal.
  if (s->Kind() != verible::SymbolKind::kLeaf) return false;
  return verible::SymbolCastToLeaf(*s).get().token_enum() ==
         verilog_tokentype::TK_StringLiteral;
}

void ExplicitParameterStorageTypeRule::HandleSymbol(
    const verible::Symbol &symbol, const SyntaxTreeContext &context) {
  verible::matcher::BoundSymbolManager manager;
  if (ParamMatcher().Matches(symbol, &manager)) {
    // 'parameter type' declarations have a storage type declared.
    if (IsParamTypeDeclaration(symbol)) return;

    const auto *type_info_symbol = GetParamTypeInfoSymbol(symbol);
    if (IsTypeInfoEmpty(*ABSL_DIE_IF_NULL(type_info_symbol))) {
      if (exempt_string_ && HasStringAssignment(symbol)) return;
      const verible::TokenInfo *param_name = GetParameterNameToken(symbol);
      violations_.insert(LintViolation(
          *param_name, absl::StrCat(kMessage, "(", param_name->text(), ")."),
          context));
    }
  }
}

// The only allowed exemption right now is 'string', as this is
// a common type that can't be handled well in some old tools.
absl::Status ExplicitParameterStorageTypeRule::Configure(
    absl::string_view configuration) {
  static const std::vector<absl::string_view> allowed = {"", "string"};
  using verible::config::SetStringOneOf;
  std::string value;
  auto s = verible::ParseNameValues(
      configuration, {{"exempt_type", SetStringOneOf(&value, allowed)}});
  if (!s.ok()) return s;
  exempt_string_ = (value == "string");
  return absl::OkStatus();
}

LintRuleStatus ExplicitParameterStorageTypeRule::Report() const {
  return LintRuleStatus(violations_, GetDescriptor());
}

}  // namespace analysis
}  // namespace verilog
