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

#include "verible/verilog/analysis/checkers/forbidden-symbol-rule.h"

#include <map>
#include <set>
#include <string>

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

using verible::container::FindWithDefault;
using verible::matcher::Matcher;

// Register ForbiddenSystemTaskFunctionRule
VERILOG_REGISTER_LINT_RULE(ForbiddenSystemTaskFunctionRule);

const LintRuleDescriptor &ForbiddenSystemTaskFunctionRule::GetDescriptor() {
  static const LintRuleDescriptor d{
      .name = "invalid-system-task-function",
      .topic = "forbidden-system-functions",
      .desc =
          "Checks that no forbidden system tasks or functions are used. These "
          "consist of the following functions: `$psprintf`, `$random`, and "
          "`$dist_*`. As well as non-LRM function `$srandom`.",
  };
  return d;
}

static const Matcher &IdMatcher() {
  static const Matcher matcher(SystemTFIdentifierLeaf().Bind("name"));
  return matcher;
}

// Set of invalid functions and suggested replacements
const std::map<std::string, std::string> &
ForbiddenSystemTaskFunctionRule::InvalidSymbolsMap() {
  static const auto *invalid_symbols = new std::map<std::string, std::string>({
      {"$psprintf", "$sformatf"},
      {"$random", "$urandom"},
      {"$srandom", "process::self().srandom()"},
      // $dist_* functions (LRM 20.15.2)
      {"$dist_chi_square", "$urandom"},
      {"$dist_erlang", "$urandom"},
      {"$dist_exponential", "$urandom"},
      {"$dist_normal", "$urandom"},
      {"$dist_poisson", "$urandom"},
      {"$dist_t", "$urandom"},
      {"$dist_uniform", "$urandom"},
  });
  return *invalid_symbols;
}

void ForbiddenSystemTaskFunctionRule::HandleSymbol(
    const verible::Symbol &symbol, const verible::SyntaxTreeContext &context) {
  verible::matcher::BoundSymbolManager manager;
  if (IdMatcher().Matches(symbol, &manager)) {
    if (const verible::SyntaxTreeLeaf *leaf = manager.GetAsLeaf("name")) {
      const auto &ism = InvalidSymbolsMap();
      if (ism.find(std::string(leaf->get().text())) != ism.end()) {
        violations_.insert(
            verible::LintViolation(leaf->get(), FormatReason(*leaf), context));
      }
    }
  }
}

verible::LintRuleStatus ForbiddenSystemTaskFunctionRule::Report() const {
  return verible::LintRuleStatus(violations_, GetDescriptor());
}

/* static */ std::string ForbiddenSystemTaskFunctionRule::FormatReason(
    const verible::SyntaxTreeLeaf &leaf) {
  const auto function_name = std::string(leaf.get().text());
  const auto replacement =
      FindWithDefault(InvalidSymbolsMap(), function_name, "");
  auto message = function_name + " is a forbidden system function or task";
  if (!replacement.empty()) {
    message += ", please use " + replacement + " instead";
  }
  return message;
}
}  // namespace analysis
}  // namespace verilog
