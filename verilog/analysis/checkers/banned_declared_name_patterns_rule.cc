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

#include "verilog/analysis/checkers/banned_declared_name_patterns_rule.h"

#include <set>
#include <string>

#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "common/analysis/citation.h"
#include "common/analysis/lint_rule_status.h"
#include "common/analysis/matcher/bound_symbol_manager.h"
#include "common/strings/naming_utils.h"
#include "common/text/symbol.h"
#include "common/text/syntax_tree_context.h"
#include "verilog/CST/functions.h"
#include "verilog/CST/module.h"
#include "verilog/CST/package.h"
#include "verilog/analysis/lint_rule_registry.h"

namespace verilog {
namespace analysis {

VERILOG_REGISTER_LINT_RULE(BannedDeclaredNamePatternsRule);

using verible::GetStyleGuideCitation;
using verible::LintRuleStatus;
using verible::LintViolation;
using verible::SyntaxTreeContext;

absl::string_view BannedDeclaredNamePatternsRule::Name() {
  return "banned-declared-name-patterns";
}
const char BannedDeclaredNamePatternsRule::kTopic[] = "identifiers";
const char BannedDeclaredNamePatternsRule::kMessage[] =
    "Check banned declared name patterns";

std::string BannedDeclaredNamePatternsRule::GetDescription(
    DescriptionType description_type) {
  return absl::StrCat(
      "Checks for"
      " banned declared name against set of unwanted patterns."
      " See your project's style guidance regarding naming.");
}

void BannedDeclaredNamePatternsRule::HandleNode(
    const verible::SyntaxTreeNode& node,
    const verible::SyntaxTreeContext& context) {
  const auto tag = static_cast<verilog::NodeEnum>(node.Tag().tag);
  switch (tag) {
    case NodeEnum::kModuleDeclaration: {
      const auto& module_match = GetModuleName(node);
      absl::string_view module_id = module_match.get().text();

      if (absl::EqualsIgnoreCase(module_id, "ILLEGALNAME")) {
        violations_.insert(LintViolation(module_match.get(), kMessage));
      }
      break;
    }
    case NodeEnum::kPackageDeclaration: {
      const auto& pack_match = GetPackageNameToken(node);
      absl::string_view pack_id = pack_match.text();

      if (absl::EqualsIgnoreCase(pack_id, "ILLEGALNAME")) {
        violations_.insert(LintViolation(pack_match, kMessage));
      }
      break;
    }
    default:
      break;
  }
}

LintRuleStatus BannedDeclaredNamePatternsRule::Report() const {
  return LintRuleStatus(violations_, Name(), GetStyleGuideCitation(kTopic));
}

}  // namespace analysis
}  // namespace verilog
