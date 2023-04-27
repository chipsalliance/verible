// Copyright 2017-2023 The Verible Authors.
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

#ifndef VERIBLE_VERILOG_ANALYSIS_CHECKERS_FORBID_NEGATIVE_ARRAY_DIM_RULE_H_
#define VERIBLE_VERILOG_ANALYSIS_CHECKERS_FORBID_NEGATIVE_ARRAY_DIM_RULE_H_

#include <set>
#include <string>

#include "common/analysis/lint-rule-status.h"
#include "common/analysis/syntax-tree-lint-rule.h"
#include "common/text/symbol.h"
#include "common/text/syntax_tree_context.h"
#include "verilog/analysis/descriptions.h"

namespace verilog {
namespace analysis {
/*
 * Check for negative sizes inside array dimensions. It only
 * detects constant literals at the moment.
 *
 * Examples:
 *   logic l [-1:0];
 *   logic [-1:0] l;
 *
 * See forbid_negative_array_dim_test.cc for more examples.
 */
class ForbidNegativeArrayDim : public verible::SyntaxTreeLintRule {
 public:
  using rule_type = verible::SyntaxTreeLintRule;

  static const LintRuleDescriptor& GetDescriptor();

  void HandleSymbol(const verible::Symbol& symbol,
                    const verible::SyntaxTreeContext& context) final;

  verible::LintRuleStatus Report() const final;

 private:
  std::set<verible::LintViolation> violations_;
};

}  // namespace analysis
}  // namespace verilog

#endif  // VERIBLE_VERILOG_ANALYSIS_CHECKERS_FORBID_NEGATIVE_ARRAY_DIM_RULE_H_
