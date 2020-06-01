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

#ifndef VERIBLE_VERILOG_ANALYSIS_CHECKERS_UVM_MACRO_SEMICOLON_RULE_H_
#define VERIBLE_VERILOG_ANALYSIS_CHECKERS_UVM_MACRO_SEMICOLON_RULE_H_

#include <string>

#include "common/analysis/lint_rule_status.h"
#include "common/analysis/syntax_tree_lint_rule.h"
#include "common/text/concrete_syntax_leaf.h"
#include "common/text/symbol.h"
#include "common/text/syntax_tree_context.h"
#include "verilog/CST/verilog_matchers.h"
#include "verilog/analysis/descriptions.h"

namespace verilog {
namespace analysis {

// UvmMacroSemicolonRule checks for `uvm_* macro calls that end with ';'
//
// Example violations:
// class Bad;
//   function foo();
//     `uvm_error("id","message");
//     `uvm_error("id",
//                "message");
//    endfunction
// endclass

class UvmMacroSemicolonRule : public verible::SyntaxTreeLintRule {
 public:
  using rule_type = verible::SyntaxTreeLintRule;
  static absl::string_view Name();

  UvmMacroSemicolonRule() : state_(State::kNormal) {}

  // Returns the description of the rule implemented
  static std::string GetDescription(DescriptionType);

  void HandleLeaf(const verible::SyntaxTreeLeaf& leaf,
                  const verible::SyntaxTreeContext& context) override;

  verible::LintRuleStatus Report() const override;

 private:
  // States of the internal leaf-based analysis.
  enum class State {
    kNormal,
    kCheckMacro,
  };

  // Internal analysis state
  State state_;

  // Save the matching macro and include it in diagnostic message
  verible::TokenInfo macro_id_ = verible::TokenInfo::EOFToken();

  // Link to style-guide rule.
  static const char kTopic[];

  std::set<verible::LintViolation> violations_;
};

}  // namespace analysis
}  // namespace verilog

#endif  // VERIBLE_VERILOG_ANALYSIS_CHECKERS_UVM_MACRO_SEMICOLON_RULE_H_
