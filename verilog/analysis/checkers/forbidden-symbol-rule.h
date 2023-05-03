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

#ifndef VERIBLE_VERILOG_ANALYSIS_CHECKERS_FORBIDDEN_SYMBOL_RULE_H_
#define VERIBLE_VERILOG_ANALYSIS_CHECKERS_FORBIDDEN_SYMBOL_RULE_H_

#include <map>
#include <set>
#include <string>

#include "common/analysis/lint_rule_status.h"
#include "common/analysis/syntax-tree-lint-rule.h"
#include "common/text/concrete-syntax-leaf.h"
#include "common/text/symbol.h"
#include "common/text/syntax-tree-context.h"
#include "verilog/analysis/descriptions.h"

namespace verilog {
namespace analysis {

// ForbiddenSystemTaskFunctionRule checks for forbidden system tasks or
// functions.
//
// Example violation:
// class Bad;
//   function Foo;
//     $psprintf("bar");
//   endfunction
// endclass
class ForbiddenSystemTaskFunctionRule : public verible::SyntaxTreeLintRule {
 public:
  using rule_type = verible::SyntaxTreeLintRule;

  static const LintRuleDescriptor& GetDescriptor();

  void HandleSymbol(const verible::Symbol& symbol,
                    const verible::SyntaxTreeContext& context) final;

  verible::LintRuleStatus Report() const final;

 private:
  static std::string FormatReason(const verible::SyntaxTreeLeaf& leaf);

  // Set of invalid functions and suggested replacements
  static const std::map<std::string, std::string>& InvalidSymbolsMap();

 private:
  std::set<verible::LintViolation> violations_;
};

}  // namespace analysis
}  // namespace verilog

#endif  // VERIBLE_VERILOG_ANALYSIS_CHECKERS_FORBIDDEN_SYMBOL_RULE_H_
