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

#include "verible/verilog/analysis/checkers/generate-label-prefix-rule.h"

#include <string>
#include <string_view>

#include "absl/status/status.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "verible/common/analysis/lint-rule-status.h"
#include "verible/common/analysis/matcher/bound-symbol-manager.h"
#include "verible/common/analysis/matcher/matcher.h"
#include "verible/common/text/config-utils.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/syntax-tree-context.h"
#include "verible/common/text/token-info.h"
#include "verible/common/text/tree-utils.h"
#include "verible/verilog/CST/seq-block.h"
#include "verible/verilog/CST/verilog-matchers.h"
#include "verible/verilog/CST/verilog-nonterminals.h"
#include "verible/verilog/analysis/descriptions.h"
#include "verible/verilog/analysis/lint-rule-registry.h"

namespace verilog {
namespace analysis {

using verible::matcher::Matcher;

// Register the lint rule
VERILOG_REGISTER_LINT_RULE(GenerateLabelPrefixRule);

static constexpr std::string_view kDefaultPrefix = "g_";

GenerateLabelPrefixRule::GenerateLabelPrefixRule() : prefix_(kDefaultPrefix) {}

const LintRuleDescriptor &GenerateLabelPrefixRule::GetDescriptor() {
  static const LintRuleDescriptor d{
      .name = "generate-label-prefix",
      .topic = "generate-constructs",
      .desc = "Checks that every generate block label starts with the specified prefix.",
      .param = {{"prefix", std::string(kDefaultPrefix),
                 "The required prefix for generate block labels."}},
  };
  return d;
}

// Matches begin statements
static const Matcher &BlockMatcher() {
  static const Matcher matcher(NodekGenerateBlock());
  return matcher;
}

void GenerateLabelPrefixRule::HandleSymbol(
    const verible::Symbol &symbol, const verible::SyntaxTreeContext &context) {
  verible::matcher::BoundSymbolManager manager;
  if (BlockMatcher().Matches(symbol, &manager)) {
    // Exclude case generate statements, as kGenerateBlock is generated for
    // each 'case' item too.
    if (context.IsInside(NodeEnum::kGenerateCaseItemList)) {
      return;
    }

    for (const auto &child : SymbolCastToNode(symbol).children()) {
      const verible::TokenInfo *label = nullptr;
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
        // Check if label starts with the configured prefix (case-insensitive)
        if (!absl::StartsWithIgnoreCase(label->text(), prefix_)) {
          std::string message = absl::StrCat(
              "Generate block label must start with \"", prefix_, "\"");
          violations_.insert(verible::LintViolation(*label, message, context));
        }
      }
    }
  }
}

absl::Status GenerateLabelPrefixRule::Configure(std::string_view configuration) {
  using verible::config::SetString;
  absl::Status s = verible::ParseNameValues(
      configuration, {{"prefix", SetString(&prefix_)}});
  return s;
}

verible::LintRuleStatus GenerateLabelPrefixRule::Report() const {
  return verible::LintRuleStatus(violations_, GetDescriptor());
}

}  // namespace analysis
}  // namespace verilog
