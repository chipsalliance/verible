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

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "common/analysis/lint_rule_status.h"
#include "common/analysis/matcher/bound_symbol_manager.h"
#include "common/analysis/matcher/matcher.h"
#include "common/text/config_utils.h"
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

using verible::LintRuleStatus;
using verible::LintViolation;
using verible::SyntaxTreeContext;
using verible::matcher::Matcher;

// Register ProperParameterDeclarationRule
VERILOG_REGISTER_LINT_RULE(ProperParameterDeclarationRule);

static constexpr absl::string_view kParameterNotInPackageMessage =
    "\'parameter\' declarations should only be in the formal parameter list of "
    "modules/classes.";
static constexpr absl::string_view kParameterAllowPackageMessage =
    "\'parameter\' declarations should only be in the formal parameter list of "
    "modules and classes or in package definition bodies.";
static constexpr absl::string_view kLocalParamNotInPackageMessage =
    "\'localparam\' declarations should only be within modules or class "
    "definition bodies.";
static constexpr absl::string_view kLocalParamAllowPackageMessage =
    "\'localparam\' declarations should only be within modules, packages or "
    "class definition bodies.";

static absl::string_view kParameterMessage = kParameterNotInPackageMessage;
static absl::string_view kLocalParamMessage = kLocalParamAllowPackageMessage;

const LintRuleDescriptor &ProperParameterDeclarationRule::GetDescriptor() {
  static const LintRuleDescriptor d{
      .name = "proper-parameter-declaration",
      .topic = "constants",
      .desc =
          "Checks that every `parameter` declaration is inside a "
          "formal parameter list of modules/classes and "
          "every `localparam` declaration is inside a module, class or "
          "package.",
      .param = {
          {
              .name = "package_allow_parameter",
              .default_value = "true",
              .description = "Allow parameters in packages (treated as a "
                             "synonym for localparam).",
          },
          {
              .name = "package_allow_localparam",
              .default_value = "false",
              .description = "Allow localparams in packages.",
          },
      }};
  return d;
}

absl::Status ProperParameterDeclarationRule::Configure(
    absl::string_view configuration) {
  using verible::config::SetBool;
  auto status = verible::ParseNameValues(
      configuration,
      {
          {"package_allow_parameter", SetBool(&package_allow_parameter_)},
          {"package_allow_localparam", SetBool(&package_allow_localparam_)},
      });

  // Change the message slightly
  kParameterMessage = package_allow_parameter_ ? kParameterAllowPackageMessage
                                               : kParameterNotInPackageMessage;
  kLocalParamMessage = package_allow_localparam_
                           ? kLocalParamAllowPackageMessage
                           : kLocalParamNotInPackageMessage;
  return status;
}

static const Matcher &ParamDeclMatcher() {
  static const Matcher matcher(NodekParamDeclaration());
  return matcher;
}

// TODO(kathuriac): Also check the 'interface' and 'program' constructs.
void ProperParameterDeclarationRule::HandleSymbol(
    const verible::Symbol &symbol, const SyntaxTreeContext &context) {
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
      } else if (ContextIsInsidePackage(context)) {
        if (!package_allow_parameter_) {
          violations_.insert(LintViolation(symbol, kParameterMessage, context));
        }
      }
    } else if (param_decl_token == TK_localparam) {
      // Raise violation if the context is not inside a class, package or
      // module, report violation.
      if (!ContextIsInsideClass(context) && !ContextIsInsideModule(context)) {
        if (!ContextIsInsidePackage(context)) {
          violations_.insert(
              LintViolation(symbol, kLocalParamMessage, context));
        } else {
          if (!package_allow_localparam_) {
            violations_.insert(
                LintViolation(symbol, kLocalParamMessage, context));
          }
        }
      }
    }
  }
}

LintRuleStatus ProperParameterDeclarationRule::Report() const {
  return LintRuleStatus(violations_, GetDescriptor());
}

}  // namespace analysis
}  // namespace verilog
