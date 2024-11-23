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

#ifndef VERIBLE_VERILOG_ANALYSIS_CHECKERS_SUSPICIOUS_SEMICOLON_H_
#define VERIBLE_VERILOG_ANALYSIS_CHECKERS_SUSPICIOUS_SEMICOLON_H_

#include <set>

#include "verible/common/analysis/lint-rule-status.h"
#include "verible/common/analysis/syntax-tree-lint-rule.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/syntax-tree-context.h"
#include "verible/verilog/analysis/descriptions.h"

namespace verilog {
namespace analysis {

/*
 * Detect suspicious semicolons. Inspired by clang-tidy's
 * bugprone-suspicious-semicolon check.
 *
 * This rule detects extra semicolons that modify code behaviour
 * while having a good chance to escape quick visual inspection.
 *
 * A couple of examples:
 *
 * if (condition);
 *   `uvm_fatal(...);
 *
 * while (condition); begin
 *   doSomething();
 * end
 *
 * Reference:
 * https://clang.llvm.org/extra/clang-tidy/checks/bugprone/suspicious-semicolon.html#bugprone-suspicious-semicolon
 */
class SuspiciousSemicolon : public verible::SyntaxTreeLintRule {
 public:
  using rule_type = verible::SyntaxTreeLintRule;

  static const LintRuleDescriptor &GetDescriptor();

  void HandleNode(const verible::SyntaxTreeNode &node,
                  const verible::SyntaxTreeContext &context) final;

  verible::LintRuleStatus Report() const final;

 private:
  std::set<verible::LintViolation> violations_;
};

}  // namespace analysis
}  // namespace verilog

#endif  // VERIBLE_VERILOG_ANALYSIS_CHECKERS_SUSPICIOUS_SEMICOLON_H_
