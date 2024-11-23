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

#include "verible/verilog/analysis/checkers/signal-name-style-rule.h"

#include <memory>
#include <set>
#include <string>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "re2/re2.h"
#include "verible/common/analysis/lint-rule-status.h"
#include "verible/common/analysis/matcher/bound-symbol-manager.h"
#include "verible/common/analysis/matcher/matcher.h"
#include "verible/common/text/concrete-syntax-leaf.h"
#include "verible/common/text/config-utils.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/syntax-tree-context.h"
#include "verible/common/text/token-info.h"
#include "verible/common/util/logging.h"
#include "verible/verilog/CST/data.h"
#include "verible/verilog/CST/net.h"
#include "verible/verilog/CST/port.h"
#include "verible/verilog/CST/verilog-matchers.h"
#include "verible/verilog/analysis/descriptions.h"
#include "verible/verilog/analysis/lint-rule-registry.h"

namespace verilog {
namespace analysis {

VERILOG_REGISTER_LINT_RULE(SignalNameStyleRule);

using verible::LintRuleStatus;
using verible::LintViolation;
using verible::SyntaxTreeContext;
using verible::matcher::Matcher;

static constexpr absl::string_view kDefaultStyleRegex = "[a-z_0-9]+";

SignalNameStyleRule::SignalNameStyleRule()
    : style_regex_(
          std::make_unique<re2::RE2>(kDefaultStyleRegex, re2::RE2::Quiet)) {}

const LintRuleDescriptor &SignalNameStyleRule::GetDescriptor() {
  static const LintRuleDescriptor d{
      .name = "signal-name-style",
      .topic = "signal-conventions",
      .desc =
          "Checks that signal names conform to a naming convention defined by "
          "a RE2 regular expression. Signals are defined as \"a net, variable, "
          "or port within a SystemVerilog design\". The default regex pattern "
          "expects \"lower_snake_case\". Refer to "
          "https://github.com/chipsalliance/verible/tree/master/verilog/tools/"
          "lint#readme for more detail on verible regex patterns.",
      .param = {{"style_regex", std::string(kDefaultStyleRegex),
                 "A regex used to check signal names style."}},
  };
  return d;
}

static const Matcher &PortMatcher() {
  static const Matcher matcher(NodekPortDeclaration());
  return matcher;
}

static const Matcher &NetMatcher() {
  static const Matcher matcher(NodekNetDeclaration());
  return matcher;
}

static const Matcher &DataMatcher() {
  static const Matcher matcher(NodekDataDeclaration());
  return matcher;
}

std::string SignalNameStyleRule::CreateViolationMessage() {
  return absl::StrCat("Signal name does not match the naming convention ",
                      "defined by regex pattern: ", style_regex_->pattern());
}

void SignalNameStyleRule::HandleSymbol(const verible::Symbol &symbol,
                                       const SyntaxTreeContext &context) {
  verible::matcher::BoundSymbolManager manager;
  if (PortMatcher().Matches(symbol, &manager)) {
    const auto *identifier_leaf = GetIdentifierFromPortDeclaration(symbol);
    const auto name = ABSL_DIE_IF_NULL(identifier_leaf)->get().text();
    if (!RE2::FullMatch(name, *style_regex_)) {
      violations_.insert(LintViolation(identifier_leaf->get(),
                                       CreateViolationMessage(), context));
    }
  } else if (NetMatcher().Matches(symbol, &manager)) {
    const auto identifier_leaves = GetIdentifiersFromNetDeclaration(symbol);
    for (const auto *leaf : identifier_leaves) {
      const auto name = leaf->text();
      if (!RE2::FullMatch(name, *style_regex_)) {
        violations_.insert(
            LintViolation(*leaf, CreateViolationMessage(), context));
      }
    }
  } else if (DataMatcher().Matches(symbol, &manager)) {
    const auto identifier_leaves = GetIdentifiersFromDataDeclaration(symbol);
    for (const auto *leaf : identifier_leaves) {
      const auto name = leaf->text();
      if (!RE2::FullMatch(name, *style_regex_)) {
        violations_.insert(
            LintViolation(*leaf, CreateViolationMessage(), context));
      }
    }
  }
}

absl::Status SignalNameStyleRule::Configure(absl::string_view configuration) {
  using verible::config::SetRegex;
  absl::Status s = verible::ParseNameValues(
      configuration, {{"style_regex", SetRegex(&style_regex_)}});
  return s;
}

LintRuleStatus SignalNameStyleRule::Report() const {
  return LintRuleStatus(violations_, GetDescriptor());
}

}  // namespace analysis
}  // namespace verilog
