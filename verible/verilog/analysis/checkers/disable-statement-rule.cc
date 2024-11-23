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

#include "verible/verilog/analysis/checkers/disable-statement-rule.h"

#include <iterator>
#include <set>

#include "absl/strings/string_view.h"
#include "verible/common/analysis/lint-rule-status.h"
#include "verible/common/analysis/matcher/bound-symbol-manager.h"
#include "verible/common/analysis/matcher/matcher.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/syntax-tree-context.h"
#include "verible/common/text/tree-utils.h"
#include "verible/common/util/iterator-adaptors.h"
#include "verible/verilog/CST/identifier.h"
#include "verible/verilog/CST/verilog-matchers.h"  // IWYU pragma: keep
#include "verible/verilog/CST/verilog-nonterminals.h"
#include "verible/verilog/analysis/descriptions.h"
#include "verible/verilog/analysis/lint-rule-registry.h"

namespace verilog {
namespace analysis {

using verible::LintRuleStatus;
using verible::LintViolation;
using verible::SyntaxTreeContext;
using verible::matcher::Matcher;

VERILOG_REGISTER_LINT_RULE(DisableStatementNoLabelsRule);

static constexpr absl::string_view kMessage =
    "Invalid usage of disable statement. Preferred construction is: disable "
    "fork;";
static constexpr absl::string_view kMessageSeqBlock =
    "Invalid usage of disable statement. Preferred construction is: disable "
    "label_of_seq_block;";

const LintRuleDescriptor &DisableStatementNoLabelsRule::GetDescriptor() {
  static const LintRuleDescriptor d{
      .name = "disable-statement",
      .topic = "disable-invalid-in-non-sequential",
      .desc =
          "Checks that there are no occurrences of `disable some_label` "
          "if label is referring to a fork or other none sequential block "
          "label. Use `disable fork` instead.",
  };
  return d;
}

static const Matcher &DisableMatcher() {
  static const Matcher matcher(
      NodekDisableStatement(DisableStatementHasLabel()));
  return matcher;
}

void DisableStatementNoLabelsRule::HandleSymbol(
    const verible::Symbol &symbol, const SyntaxTreeContext &context) {
  verible::matcher::BoundSymbolManager manager;
  if (!DisableMatcher().Matches(symbol, &manager)) {
    return;
  }
  absl::string_view message_final = kMessage;
  // if no kDisable label, return, nothing to be checked
  const auto &disableLabels = FindAllSymbolIdentifierLeafs(symbol);
  if (disableLabels.empty()) {
    return;
  }
  // look for every kBegin node starting from kDisableLabel token
  // the kDisableLabel can be nested in some kBegin nodes
  // so we're looking for the kBegin node that direct parent
  // is not one of the initial/final/always statements since such blocks are
  // considered to be invalid. If the label for disable statements is not
  // found, it means that there is no appropriate label or the label
  // points to the illegal node such as forked label
  const auto &rcontext = reversed_view(context);
  for (auto rc = rcontext.begin(); rc != rcontext.end(); rc++) {
    const auto &node = *rc;
    if (node->Tag().tag != static_cast<int>(NodeEnum::kSeqBlock)) {
      continue;
    }
    for (const auto &ch : node->children()) {
      if (ch->Tag().tag != static_cast<int>(NodeEnum::kBegin)) {
        continue;
      }
      const auto &beginLabels = FindAllSymbolIdentifierLeafs(*ch);
      if (beginLabels.empty()) {
        continue;
      }
      const auto &pnode = *std::next(rc);
      const auto &ptag = pnode->Tag().tag;
      if (ptag == static_cast<int>(NodeEnum::kInitialStatement) ||
          ptag == static_cast<int>(NodeEnum::kFinalStatement) ||
          ptag == static_cast<int>(NodeEnum::kAlwaysStatement)) {
        message_final = kMessageSeqBlock;
        break;
      }
      const auto &beginLabel = SymbolCastToLeaf(*beginLabels[0].match);
      const auto &disableLabel = SymbolCastToLeaf(*disableLabels[0].match);
      if (beginLabel.get().text() == disableLabel.get().text()) {
        return;
      }
    }
  }
  violations_.insert(LintViolation(symbol, message_final, context));
}

LintRuleStatus DisableStatementNoLabelsRule::Report() const {
  return LintRuleStatus(violations_, GetDescriptor());
}
}  // namespace analysis
}  // namespace verilog
