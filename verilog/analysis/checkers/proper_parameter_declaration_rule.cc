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

#include "verilog/analysis/checkers/proper_parameter_declaration_rule.h"

#include <set>
#include <string>

#include "absl/strings/str_cat.h"
#include "common/analysis/citation.h"
#include "common/analysis/lint_rule_status.h"
#include "common/analysis/matcher/bound_symbol_manager.h"
#include "common/analysis/matcher/matcher.h"
#include "common/text/symbol.h"
#include "common/text/syntax_tree_context.h"
#include "verilog/CST/context_functions.h"
#include "verilog/CST/parameters.h"
#include "verilog/CST/verilog_matchers.h"
#include "verilog/analysis/descriptions.h"
#include "verilog/analysis/lint_rule_registry.h"
#include "verilog/parser/verilog_token_enum.h"

namespace verilog {
namespace analysis {

using verible::GetStyleGuideCitation;
using verible::LintRuleStatus;
using verible::LintViolation;
using verible::SyntaxTreeContext;
using verible::matcher::Matcher;

// Register ProperParameterDeclarationRule
VERILOG_REGISTER_LINT_RULE(ProperParameterDeclarationRule);

absl::string_view ProperParameterDeclarationRule::Name() {
  return "proper-parameter-declaration";
}
const char ProperParameterDeclarationRule::kTopic[] = "constants";
const char ProperParameterDeclarationRule::kParameterMessage[] =
    "\'parameter\' declarations should only be within packages or in the "
    "formal parameter list of modules/classes.";
const char ProperParameterDeclarationRule::kLocalParamMessage[] =
    "\'localparam\' declarations should only be within modules\' or classes\' "
    "definition bodies.";

std::string ProperParameterDeclarationRule::GetDescription(
    DescriptionType description_type) {
  return absl::StrCat("Checks that every ",
                      Codify("parameter", description_type),
                      " declaration is inside a package or in the formal "
                      "parameter list of modules/classes and every ",
                      Codify("localparam", description_type),
                      " declaration is inside a module or class. See ",
                      GetStyleGuideCitation(kTopic), ".");
}

static const Matcher& ParamDeclMatcher() {
  static const Matcher matcher(NodekParamDeclaration());
  return matcher;
}

// TODO(kathuriac): Also check the 'interface' and 'program' constructs.
void ProperParameterDeclarationRule::HandleSymbol(
    const verible::Symbol& symbol, const SyntaxTreeContext& context) {
  verible::matcher::BoundSymbolManager manager;
  if (ParamDeclMatcher().Matches(symbol, &manager)) {
    const auto param_decl_token = GetParamKeyword(symbol);
    if (param_decl_token == TK_parameter) {
      // Check if the context is inside a class or module, and a
      // kFormalParameterList.
      if (ContextIsInsideClass(context) &&
          !ContextIsInsideFormalParameterList(context)) {
        violations_.insert(LintViolation(symbol, kParameterMessage, context));
      } else if (ContextIsInsideModule(context) &&
                 !ContextIsInsideFormalParameterList(context)) {
        violations_.insert(LintViolation(symbol, kParameterMessage, context));
      }
    } else if (param_decl_token == TK_localparam) {
      // If the context is not inside a class or module, report violation.
      if (!ContextIsInsideClass(context) && !ContextIsInsideModule(context))
        violations_.insert(LintViolation(symbol, kLocalParamMessage, context));
    }
  }
}

LintRuleStatus ProperParameterDeclarationRule::Report() const {
  return LintRuleStatus(violations_, Name(), GetStyleGuideCitation(kTopic));
}

}  // namespace analysis
}  // namespace verilog
