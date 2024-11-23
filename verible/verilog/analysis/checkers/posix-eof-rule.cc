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

#include "verible/verilog/analysis/checkers/posix-eof-rule.h"

#include <set>
#include <string>

#include "absl/strings/string_view.h"
#include "verible/common/analysis/lint-rule-status.h"
#include "verible/common/text/text-structure.h"
#include "verible/common/text/token-info.h"
#include "verible/verilog/analysis/descriptions.h"
#include "verible/verilog/analysis/lint-rule-registry.h"
#include "verible/verilog/parser/verilog-token-enum.h"

namespace verilog {
namespace analysis {

using verible::AutoFix;
using verible::LintRuleStatus;
using verible::LintViolation;
using verible::TextStructureView;
using verible::TokenInfo;

// Register the lint rule
VERILOG_REGISTER_LINT_RULE(PosixEOFRule);

static constexpr absl::string_view kMessage = "File must end with a newline.";

const LintRuleDescriptor &PosixEOFRule::GetDescriptor() {
  static const LintRuleDescriptor d{
      .name = "posix-eof",
      .topic = "posix-file-endings",
      .desc = "Checks that the file ends with a newline.",
  };
  return d;
}

void PosixEOFRule::Lint(const TextStructureView &text_structure,
                        absl::string_view) {
  if (!text_structure.Contents().empty()) {
    const auto &last_line = text_structure.Lines().back();
    if (!last_line.empty()) {
      // Point to the end of the line (also EOF).
      const TokenInfo token(TK_OTHER, last_line.substr(last_line.length(), 0));
      violations_.insert(LintViolation(
          token, kMessage,
          {AutoFix("Add newline at end of file", {token, "\n"})}));
    }
  }
}

LintRuleStatus PosixEOFRule::Report() const {
  return LintRuleStatus(violations_, GetDescriptor());
}

}  // namespace analysis
}  // namespace verilog
