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

#include "verilog/analysis/checkers/generate_label_prefix_rule.h"

#include <string>
#include <vector>

#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "common/analysis/citation.h"
#include "common/analysis/lint_rule_status.h"
#include "common/analysis/matcher/bound_symbol_manager.h"
#include "common/analysis/matcher/matcher.h"
#include "common/text/symbol.h"
#include "common/text/syntax_tree_context.h"
#include "verilog/CST/identifier.h"
#include "verilog/CST/seq_block.h"
#include "verilog/CST/verilog_matchers.h"
#include "verilog/analysis/descriptions.h"
#include "verilog/analysis/lint_rule_registry.h"

namespace verilog {
namespace analysis {

using verible::GetStyleGuideCitation;
using verible::matcher::Matcher;

// Register the lint rule
VERILOG_REGISTER_LINT_RULE(GenerateLabelPrefixRule);

absl::string_view GenerateLabelPrefixRule::Name() {
  return "generate-label-prefix";
}
const char GenerateLabelPrefixRule::kTopic[] = "generate-constructs";
const char GenerateLabelPrefixRule::kMessage[] =
    "All generate block labels must start with g_ or gen_";
// TODO(fangism): and be lower_snake_case?
// TODO(fangism): generalize to a configurable pattern and
// rename this class/rule to GenerateLabelNamingStyle?

std::string GenerateLabelPrefixRule::GetDescription(
    DescriptionType description_type) {
  return absl::StrCat(
      "Checks that every generate block label starts with g_ or gen_. See ",
      GetStyleGuideCitation(kTopic), ".");
}

// Matches begin statements
static const Matcher& BlockMatcher() {
  static const Matcher matcher(NodekGenerateBlock());
  return matcher;
}

void GenerateLabelPrefixRule::HandleSymbol(
    const verible::Symbol& symbol, const verible::SyntaxTreeContext& context) {
  verible::matcher::BoundSymbolManager manager;
  if (BlockMatcher().Matches(symbol, &manager)) {
    // Exclude case generate statements, as kGenerateBlock is generated for
    // each 'case' item too.
    if (context.IsInside(NodeEnum::kGenerateCaseItemList)) {
      return;
    }

    for (const auto& child : SymbolCastToNode(symbol).children()) {
      const verible::TokenInfo* label = nullptr;
      switch (NodeEnum(SymbolCastToNode(*child).Tag().tag)) {
        case NodeEnum::kBegin:
          label = GetBeginLabelTokenInfo(*child);
          break;
        case NodeEnum::kEnd:
          label = GetEndLabelTokenInfo(*child);
          break;
        default:
          continue;
      }

      if (label != nullptr) {
        if (!(absl::StartsWith(label->text(), "g_") ||
              absl::StartsWith(label->text(), "gen_"))) {
          violations_.insert(verible::LintViolation(*label, kMessage, context));
        }
      }
    }
  }
}

verible::LintRuleStatus GenerateLabelPrefixRule::Report() const {
  return verible::LintRuleStatus(violations_, Name(),
                                 GetStyleGuideCitation(kTopic));
}

}  // namespace analysis
}  // namespace verilog
