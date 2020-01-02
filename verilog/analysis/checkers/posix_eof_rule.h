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

#ifndef VERIBLE_VERILOG_ANALYSIS_CHECKERS_POSIX_EOF_RULE_H_
#define VERIBLE_VERILOG_ANALYSIS_CHECKERS_POSIX_EOF_RULE_H_

#include <set>
#include <string>

#include "absl/strings/string_view.h"
#include "common/analysis/lint_rule_status.h"
#include "common/analysis/text_structure_lint_rule.h"
#include "common/text/text_structure.h"
#include "verilog/analysis/descriptions.h"

namespace verilog {
namespace analysis {

// Detects whether last line ends with newline.
// See
// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap03.html#tag_03_206
// for the formal definition of a line.
// Rather than implement as a LineLintRule which would traverse the entire
// file unnecessarily, implementing as a TextStructureLintRule efficiently
// examines only the last line of the file, because all previous lines have
// already been split as such line newline characters (by definition).
class PosixEOFRule : public verible::TextStructureLintRule {
 public:
  using rule_type = verible::TextStructureLintRule;
  static absl::string_view Name();

  // Returns the description of the rule implemented formatted for either the
  // helper flag or markdown depending on the parameter type.
  static std::string GetDescription(DescriptionType);

  PosixEOFRule() {}

  void Lint(const verible::TextStructureView&, absl::string_view) override;

  verible::LintRuleStatus Report() const override;

 private:
  // Link to style guide rule.
  static const char kTopic[];

  // Diagnostic message.
  static const char kMessage[];

  // Collection of found violations.
  std::set<verible::LintViolation> violations_;
};

}  // namespace analysis
}  // namespace verilog

#endif  // VERIBLE_VERILOG_ANALYSIS_CHECKERS_POSIX_EOF_RULE_H_
