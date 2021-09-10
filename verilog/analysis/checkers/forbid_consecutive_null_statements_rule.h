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

#ifndef VERIBLE_VERILOG_ANALYSIS_CHECKERS_FORBID_CONSECUTIVE_NULL_STATEMENTS_RULE_H_
#define VERIBLE_VERILOG_ANALYSIS_CHECKERS_FORBID_CONSECUTIVE_NULL_STATEMENTS_RULE_H_

#include <set>
#include <string>

#include "common/analysis/lint_rule_status.h"
#include "common/analysis/matcher/matcher.h"
#include "common/analysis/matcher/matcher_builders.h"
#include "common/analysis/syntax_tree_lint_rule.h"
#include "common/text/symbol.h"
#include "common/text/syntax_tree_context.h"
#include "verilog/CST/verilog_matchers.h"  // IWYU pragma: keep
#include "verilog/analysis/descriptions.h"

namespace verilog {
namespace analysis {

// ForbidConsecutiveNullStatementsRule finds occurrences of consecutive
// null statements e.g. ';;'
class ForbidConsecutiveNullStatementsRule : public verible::SyntaxTreeLintRule {
 public:
  using rule_type = verible::SyntaxTreeLintRule;

  ForbidConsecutiveNullStatementsRule() : state_(State::kNormal) {}

  static const LintRuleDescriptor& GetDescriptor();

  void HandleLeaf(const verible::SyntaxTreeLeaf& leaf,
                  const verible::SyntaxTreeContext& context) final;

  verible::LintRuleStatus Report() const final;

 private:
  // States of the internal leaf-based analysis.
  enum class State {
    kNormal,
    kExpectNonSemicolon,
  };

  // Internal analysis state.
  State state_;

  std::set<verible::LintViolation> violations_;
};

}  // namespace analysis
}  // namespace verilog

#endif  // VERIBLE_VERILOG_ANALYSIS_CHECKERS_FORBID_CONSECUTIVE_NULL_STATEMENTS_RULE_H_
