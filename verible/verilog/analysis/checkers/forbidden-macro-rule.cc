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

#include "verible/verilog/analysis/checkers/forbidden-macro-rule.h"

#include <map>
#include <set>
#include <string>

#include "verible/common/analysis/citation.h"
#include "verible/common/analysis/lint-rule-status.h"
#include "verible/common/analysis/matcher/bound-symbol-manager.h"
#include "verible/common/analysis/matcher/matcher.h"
#include "verible/common/text/concrete-syntax-leaf.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/syntax-tree-context.h"
#include "verible/common/text/token-info.h"
#include "verible/common/util/container-util.h"
#include "verible/verilog/CST/verilog-matchers.h"
#include "verible/verilog/analysis/descriptions.h"
#include "verible/verilog/analysis/lint-rule-registry.h"

namespace verilog {
namespace analysis {

using verible::GetStyleGuideCitation;
using verible::container::FindWithDefault;
using verible::matcher::Matcher;

// Register ForbiddenMacroRule
VERILOG_REGISTER_LINT_RULE(ForbiddenMacroRule);

// TODO(fangism): Generate table of URLs from InvalidMacrosMap().
const LintRuleDescriptor &ForbiddenMacroRule::GetDescriptor() {
  static const LintRuleDescriptor d{
      .name = "forbidden-macro",
      .topic = "uvm-logging",
      .desc = "Checks that no forbidden macro calls are used.",
  };
  return d;
}

// Matches all macro call ids, like `foo.
static const Matcher &MacroCallMatcher() {
  static const Matcher matcher(MacroCallIdLeaf().Bind("name"));
  return matcher;
}

// Set of invalid macros and URLs
const std::map<std::string, std::string> &
ForbiddenMacroRule::InvalidMacrosMap() {
  // TODO(hzeller): don't use GetStyleGuideCitation here, more downstream.
  static const auto *invalid_symbols = new std::map<std::string, std::string>({
      {"`uvm_warning", GetStyleGuideCitation("uvm-logging")},
  });
  return *invalid_symbols;
}

void ForbiddenMacroRule::HandleSymbol(
    const verible::Symbol &symbol, const verible::SyntaxTreeContext &context) {
  verible::matcher::BoundSymbolManager manager;
  if (MacroCallMatcher().Matches(symbol, &manager)) {
    if (const auto *leaf = manager.GetAs<verible::SyntaxTreeLeaf>("name")) {
      const auto &imm = InvalidMacrosMap();
      if (imm.find(std::string(leaf->get().text())) != imm.end()) {
        violations_.insert(
            verible::LintViolation(leaf->get(), FormatReason(*leaf), context));
      }
    }
  }
}

verible::LintRuleStatus ForbiddenMacroRule::Report() const {
  // TODO(b/68104316): restructure LintRuleStatus to not requires a single URL
  // for every LintRuleStatus.
  return verible::LintRuleStatus(violations_, GetDescriptor());
}

/* static */ std::string ForbiddenMacroRule::FormatReason(
    const verible::SyntaxTreeLeaf &leaf) {
  const std::string function_name(leaf.get().text());
  const auto url = FindWithDefault(InvalidMacrosMap(), function_name, "");
  auto message = function_name + " is a forbidden macro";
  if (!url.empty()) {
    message += ", see " + url;
  }
  return message + ".";
}

}  // namespace analysis
}  // namespace verilog
