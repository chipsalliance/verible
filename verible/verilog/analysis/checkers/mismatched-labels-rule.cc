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

#include "verible/verilog/analysis/checkers/mismatched-labels-rule.h"

#include <set>
#include <string_view>

#include "verible/common/analysis/lint-rule-status.h"
#include "verible/common/analysis/matcher/bound-symbol-manager.h"
#include "verible/common/analysis/matcher/matcher.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/syntax-tree-context.h"
#include "verible/verilog/CST/seq-block.h"
#include "verible/verilog/CST/verilog-matchers.h"
#include "verible/verilog/analysis/descriptions.h"
#include "verible/verilog/analysis/lint-rule-registry.h"

namespace verilog {
namespace analysis {

using verible::matcher::Matcher;

// Register the lint rule
VERILOG_REGISTER_LINT_RULE(MismatchedLabelsRule);

static constexpr std::string_view kMessageMismatch =
    "Begin/end block labels must match.";
static constexpr std::string_view kMessageMissing =
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
    if (end_label == nullptr) {
      return;
    }

    // Error if there is no begin label
    if (begin_label == nullptr) {
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
