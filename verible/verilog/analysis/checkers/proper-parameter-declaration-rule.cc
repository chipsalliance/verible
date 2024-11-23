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

#include "verible/verilog/analysis/checkers/proper-parameter-declaration-rule.h"

#include <set>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "verible/common/analysis/lint-rule-status.h"
#include "verible/common/analysis/matcher/bound-symbol-manager.h"
#include "verible/common/analysis/matcher/matcher.h"
#include "verible/common/text/config-utils.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/syntax-tree-context.h"
#include "verible/common/text/token-info.h"
#include "verible/verilog/CST/context-functions.h"
#include "verible/verilog/CST/parameters.h"
#include "verible/verilog/CST/verilog-matchers.h"
#include "verible/verilog/analysis/descriptions.h"
#include "verible/verilog/analysis/lint-rule-registry.h"
#include "verible/verilog/parser/verilog-token-enum.h"

namespace verilog {
namespace analysis {

using verible::AutoFix;
using verible::LintRuleStatus;
using verible::LintViolation;
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

static constexpr absl::string_view kAutoFixReplaceParameterWithLocalparam =
    "Replace 'parameter' with 'localparam'";
static constexpr absl::string_view kAutoFixReplaceLocalparamWithParameter =
    "Replace 'localparam' with 'parameter'";

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
              .default_value = "false",
              .description = "Allow parameters in packages (treated as a "
                             "synonym for localparam).",
          },
          {
              .name = "package_allow_localparam",
              .default_value = "true",
              .description = "Allow localparams in packages.",
          },
      }};
  return d;
}

ProperParameterDeclarationRule::ProperParameterDeclarationRule() {
  ChooseMessagesForConfiguration();
}

void ProperParameterDeclarationRule::ChooseMessagesForConfiguration() {
  // Message is slightly different depending on configuration
  parameter_message_ = package_allow_parameter_ ? kParameterAllowPackageMessage
                                                : kParameterNotInPackageMessage;
  local_parameter_message_ = package_allow_localparam_
                                 ? kLocalParamAllowPackageMessage
                                 : kLocalParamNotInPackageMessage;
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

  ChooseMessagesForConfiguration();
  return status;
}

static const Matcher &ParamDeclMatcher() {
  static const Matcher matcher(NodekParamDeclaration());
  return matcher;
}

void ProperParameterDeclarationRule::AddParameterViolation(
    const verible::Symbol &symbol, const verible::SyntaxTreeContext &context) {
  const verible::TokenInfo *token = GetParameterToken(symbol);

  if (token == nullptr) {
    return;
  }

  AutoFix autofix =
      AutoFix(kAutoFixReplaceParameterWithLocalparam, {*token, "localparam"});
  violations_.insert(
      LintViolation(*token, parameter_message_, context, {autofix}));
}

void ProperParameterDeclarationRule::AddLocalparamViolation(
    const verible::Symbol &symbol, const verible::SyntaxTreeContext &context) {
  const verible::TokenInfo *token = GetParameterToken(symbol);

  if (token == nullptr) {
    return;
  }

  AutoFix autofix =
      AutoFix(kAutoFixReplaceLocalparamWithParameter, {*token, "parameter"});
  violations_.insert(
      LintViolation(*token, local_parameter_message_, context, {autofix}));
}

// TODO(kathuriac): Also check the 'interface' and 'program' constructs.
void ProperParameterDeclarationRule::HandleSymbol(
    const verible::Symbol &symbol, const verible::SyntaxTreeContext &context) {
  verible::matcher::BoundSymbolManager manager;
  if (ParamDeclMatcher().Matches(symbol, &manager)) {
    const auto param_decl_token = GetParamKeyword(symbol);
    if (param_decl_token == TK_parameter) {
      // Check if the context is inside a class or module, and a
      // kFormalParameterList.
      if (ContextIsInsideClass(context) &&
          !ContextIsInsideFormalParameterList(context)) {
        AddParameterViolation(symbol, context);
      } else if (ContextIsInsideModule(context) &&
                 !ContextIsInsideFormalParameterList(context)) {
        AddParameterViolation(symbol, context);
      } else if (ContextIsInsidePackage(context)) {
        if (!package_allow_parameter_) {
          AddParameterViolation(symbol, context);
        }
      }
    } else if (param_decl_token == TK_localparam) {
      // Raise violation if the context is not inside a class, package or
      // module, report violation.
      if (!ContextIsInsideClass(context) && !ContextIsInsideModule(context)) {
        if (!ContextIsInsidePackage(context)) {
          AddLocalparamViolation(symbol, context);
        } else {
          if (!package_allow_localparam_) {
            AddLocalparamViolation(symbol, context);
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
