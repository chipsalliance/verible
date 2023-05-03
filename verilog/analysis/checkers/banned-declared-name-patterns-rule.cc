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

#include "verilog/analysis/checkers/banned-declared-name-patterns-rule.h"

#include <set>
#include <string>

#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "common/analysis/lint_rule_status.h"
#include "common/analysis/matcher/bound-symbol-manager.h"
#include "common/strings/naming-utils.h"
#include "common/text/symbol.h"
#include "common/text/syntax-tree-context.h"
#include "verilog/CST/functions.h"
#include "verilog/CST/module.h"
#include "verilog/CST/package.h"
#include "verilog/analysis/lint_rule_registry.h"

namespace verilog {
namespace analysis {

VERILOG_REGISTER_LINT_RULE(BannedDeclaredNamePatternsRule);

using verible::LintRuleStatus;
using verible::LintViolation;

static constexpr absl::string_view kMessage =
    "Check banned declared name patterns";

const LintRuleDescriptor& BannedDeclaredNamePatternsRule::GetDescriptor() {
  static const LintRuleDescriptor d{
      .name = "banned-declared-name-patterns",
      .topic = "identifiers",
      .desc =
          "Checks for banned declared name against set of unwanted "
          "patterns.",
  };
  return d;
}

void BannedDeclaredNamePatternsRule::HandleNode(
    const verible::SyntaxTreeNode& node,
    const verible::SyntaxTreeContext& context) {
  const auto tag = static_cast<verilog::NodeEnum>(node.Tag().tag);
  switch (tag) {
    case NodeEnum::kModuleDeclaration: {
      const auto* module_match = GetModuleName(node);
      if (module_match) {
        const absl::string_view module_id = module_match->get().text();

        if (absl::EqualsIgnoreCase(module_id, "ILLEGALNAME")) {
          violations_.insert(LintViolation(module_match->get(), kMessage));
        }
      }
      break;
    }
    case NodeEnum::kPackageDeclaration: {
      const verible::TokenInfo* pack_match = GetPackageNameToken(node);
      if (pack_match) {
        absl::string_view pack_id = pack_match->text();
        if (absl::EqualsIgnoreCase(pack_id, "ILLEGALNAME")) {
          violations_.insert(LintViolation(*pack_match, kMessage));
        }
      }
      break;
    }
    default:
      break;
  }
}

LintRuleStatus BannedDeclaredNamePatternsRule::Report() const {
  return LintRuleStatus(violations_, GetDescriptor());
}

}  // namespace analysis
}  // namespace verilog
