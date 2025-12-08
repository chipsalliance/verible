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

#ifndef VERIBLE_VERILOG_ANALYSIS_CHECKERS_MACRO_NAME_STYLE_RULE_H_
#define VERIBLE_VERILOG_ANALYSIS_CHECKERS_MACRO_NAME_STYLE_RULE_H_

#include <memory>
#include <set>
#include <string>
#include <string_view>

#include "absl/status/status.h"
#include "re2/re2.h"
#include "verible/common/analysis/lint-rule-status.h"
#include "verible/common/analysis/token-stream-lint-rule.h"
#include "verible/common/text/token-info.h"
#include "verible/verilog/analysis/descriptions.h"

namespace verilog {
namespace analysis {

// MacroNameStyleRule checks that macro names follow
// a naming convention matching a regex pattern. Exceptions
// are made for uvm_* and UVM_* named macros.
class MacroNameStyleRule : public verible::TokenStreamLintRule {
 public:
  using rule_type = verible::TokenStreamLintRule;

  MacroNameStyleRule();

  static const LintRuleDescriptor &GetDescriptor();

  std::string CreateViolationMessage();

  void HandleToken(const verible::TokenInfo &token) final;

  verible::LintRuleStatus Report() const final;

  absl::Status Configure(std::string_view configuration) final;

 private:
  // States of the internal token-based analysis.
  enum class State {
    kNormal,
    kExpectPPIdentifier,
  };

  // Internal lexical analysis state.
  State state_ = State::kNormal;

  std::set<verible::LintViolation> violations_;

  // A regex to check the style against
  std::unique_ptr<re2::RE2> style_regex_;
  std::unique_ptr<re2::RE2> style_lower_snake_case_regex_;
  std::unique_ptr<re2::RE2> style_upper_snake_case_regex_;
};

}  // namespace analysis
}  // namespace verilog

#endif  // VERIBLE_VERILOG_ANALYSIS_CHECKERS_MACRO_NAME_STYLE_RULE_H_
