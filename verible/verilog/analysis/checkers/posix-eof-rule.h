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

#include "absl/strings/string_view.h"
#include "verible/common/analysis/lint-rule-status.h"
#include "verible/common/analysis/text-structure-lint-rule.h"
#include "verible/common/text/text-structure.h"
#include "verible/verilog/analysis/descriptions.h"

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

  static const LintRuleDescriptor &GetDescriptor();

  PosixEOFRule() = default;

  void Lint(const verible::TextStructureView &, absl::string_view) final;

  verible::LintRuleStatus Report() const final;

 private:
  // Collection of found violations.
  std::set<verible::LintViolation> violations_;
};

}  // namespace analysis
}  // namespace verilog

#endif  // VERIBLE_VERILOG_ANALYSIS_CHECKERS_POSIX_EOF_RULE_H_
