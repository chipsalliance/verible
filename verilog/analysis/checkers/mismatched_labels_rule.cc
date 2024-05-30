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

#include "verilog/analysis/checkers/mismatched_labels_rule.h"

#include <set>

#include "absl/strings/string_view.h"
#include "common/analysis/lint_rule_status.h"
#include "common/analysis/matcher/bound_symbol_manager.h"
#include "common/analysis/matcher/matcher.h"
#include "common/text/concrete_syntax_tree.h"
#include "common/text/symbol.h"
#include "common/text/syntax_tree_context.h"
#include "verilog/CST/seq_block.h"
#include "verilog/CST/verilog_matchers.h"
#include "verilog/analysis/descriptions.h"
#include "verilog/analysis/lint_rule_registry.h"

namespace verilog {
namespace analysis {

using verible::matcher::Matcher;

// Register the lint rule
VERILOG_REGISTER_LINT_RULE(MismatchedLabelsRule);

static constexpr absl::string_view kMessageMismatch =
    "Begin/end block labels must match.";
static constexpr absl::string_view kMessageMissing =
    "Matching begin label is missing.";

const LintRuleDescriptor &MismatchedLabelsRule::GetDescriptor() {
  static const LintRuleDescriptor d{
      .name = "mismatched-labels",
      .topic = "mismatched-labels",
      .desc = "Check for matching begin/end labels.",
  };
  return d;
}

// Matches the begin node.
static const Matcher &BeginMatcher() {
  static const Matcher matcher(NodekBegin());
  return matcher;
}

void MismatchedLabelsRule::HandleSymbol(
    const verible::Symbol &symbol, const verible::SyntaxTreeContext &context) {
  verible::matcher::BoundSymbolManager manager;

  if (BeginMatcher().Matches(symbol, &manager)) {
    const auto &matchingEnd = GetMatchingEnd(symbol, context);

    const auto *begin_label = GetBeginLabelTokenInfo(symbol);
    const auto *end_label = GetEndLabelTokenInfo(*matchingEnd);

    // Don't check anything if there is no end label
    if (!end_label) {
      return;
    }

    // Error if there is no begin label
    if (!begin_label) {
      violations_.insert(
          verible::LintViolation(symbol, kMessageMissing, context));

      return;
    }

    // Finally compare the two labels
    if (begin_label->text() != end_label->text()) {
      violations_.insert(
          verible::LintViolation(*end_label, kMessageMismatch, context));
    }
  }
}

verible::LintRuleStatus MismatchedLabelsRule::Report() const {
  return verible::LintRuleStatus(violations_, GetDescriptor());
}

}  // namespace analysis
}  // namespace verilog
