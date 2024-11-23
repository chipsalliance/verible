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

#ifndef VERIBLE_VERILOG_ANALYSIS_CHECKERS_ALWAYS_FF_NON_BLOCKING_RULE_H_
#define VERIBLE_VERILOG_ANALYSIS_CHECKERS_ALWAYS_FF_NON_BLOCKING_RULE_H_

#include <cstddef>
#include <set>
#include <stack>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "verible/common/analysis/lint-rule-status.h"
#include "verible/common/analysis/syntax-tree-lint-rule.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/syntax-tree-context.h"
#include "verible/verilog/analysis/descriptions.h"

namespace verilog {
namespace analysis {

class AlwaysFFNonBlockingRule : public verible::SyntaxTreeLintRule {
 public:
  using rule_type = verible::SyntaxTreeLintRule;

  static const LintRuleDescriptor &GetDescriptor();

  absl::Status Configure(absl::string_view configuration) final;
  void HandleSymbol(const verible::Symbol &symbol,
                    const verible::SyntaxTreeContext &context) final;

  verible::LintRuleStatus Report() const final;

 private:
  // Detects entering and leaving relevant code inside always_ff
  bool InsideBlock(const verible::Symbol &symbol, int depth);
  // Processes local declarations
  bool LocalDeclaration(const verible::Symbol &symbol);

 private:
  // Collected violations.
  std::set<verible::LintViolation> violations_;

  //- Configuration ---------------------
  bool catch_modifying_assignments_ = false;
  bool waive_for_locals_ = false;

  //- Processing State ------------------
  // Inside an always_ff block
  int inside_ = 0;

  // Stack of inner begin-end scopes
  struct Scope {
    int syntax_tree_depth;
    size_t inherited_local_count;
  };
  std::stack<Scope, std::vector<Scope>> scopes_{
      {{-1, 0}}  // bottom element -> the stack is never empty
  };

  // In-order stack of local variable names
  std::vector<absl::string_view> locals_;
};

}  // namespace analysis
}  // namespace verilog

#endif  // VERIBLE_VERILOG_ANALYSIS_CHECKERS_ALWAYS_FF_NON_BLOCKING_RULE_H_
