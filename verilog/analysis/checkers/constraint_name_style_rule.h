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

#ifndef VERIBLE_VERILOG_ANALYSIS_CHECKERS_CONSTRAINT_NAME_STYLE_RULE_H_
#define VERIBLE_VERILOG_ANALYSIS_CHECKERS_CONSTRAINT_NAME_STYLE_RULE_H_

#include <memory>
#include <set>
#include <string>

#include "common/analysis/lint_rule_status.h"
#include "common/analysis/syntax_tree_lint_rule.h"
#include "common/text/symbol.h"
#include "common/text/syntax_tree_context.h"
#include "re2/re2.h"
#include "verilog/analysis/descriptions.h"

namespace verilog {
namespace analysis {

// Lower snake case, ends with `_c`
#define kSuffix "([a-z0-9]+_)+c"

// Lower snake case, starts with `c_`
#define kPrefix "c+(_[a-z0-9]+)+"

// ConstraintNameStyleRule checks that each constraint name follows the
// specified naming convention.
//
// This convention is set by providing a regular expression to be matched
// against.
//
// The default, `kSuffix` checks that the name is written in lower_snake_case
// and ends with `_c`
class ConstraintNameStyleRule : public verible::SyntaxTreeLintRule {
 public:
  using rule_type = verible::SyntaxTreeLintRule;

  static const LintRuleDescriptor &GetDescriptor();

  absl::Status Configure(absl::string_view configuration) final;
  void HandleSymbol(const verible::Symbol &symbol,
                    const verible::SyntaxTreeContext &context) final;

  verible::LintRuleStatus Report() const final;

  std::string Pattern() const { return regex->pattern(); }

 private:
  std::set<verible::LintViolation> violations_;
  inline static std::unique_ptr<re2::RE2> regex =
      std::make_unique<re2::RE2>(kSuffix);

  std::string FormatReason() const;
};

}  // namespace analysis
}  // namespace verilog

#endif  // VERIBLE_VERILOG_ANALYSIS_CHECKERS_CONSTRAINT_NAME_STYLE_RULE_H_
