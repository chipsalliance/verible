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

#include "verilog/analysis/checkers/posix_eof_rule.h"

#include <set>
#include <string>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "common/analysis/citation.h"
#include "common/analysis/lint_rule_status.h"
#include "common/text/text_structure.h"
#include "common/text/token_info.h"
#include "verilog/analysis/descriptions.h"
#include "verilog/analysis/lint_rule_registry.h"
#include "verilog/parser/verilog_token_enum.h"

namespace verilog {
namespace analysis {

using verible::GetStyleGuideCitation;
using verible::LintRuleStatus;
using verible::LintViolation;
using verible::TextStructureView;
using verible::TokenInfo;

// Register the lint rule
VERILOG_REGISTER_LINT_RULE(PosixEOFRule);

absl::string_view PosixEOFRule::Name() { return "posix-eof"; }
const char PosixEOFRule::kTopic[] = "posix-file-endings";
const char PosixEOFRule::kMessage[] = "File must end with a newline.";

std::string PosixEOFRule::GetDescription(DescriptionType description_type) {
  return absl::StrCat("Checks that the file ends with a newline. See ",
                      GetStyleGuideCitation(kTopic), ".");
}

void PosixEOFRule::Lint(const TextStructureView& text_structure,
                        absl::string_view) {
  if (!text_structure.Contents().empty()) {
    const auto& last_line = text_structure.Lines().back();
    if (!last_line.empty()) {
      // Point to the end of the line (also EOF).
      const TokenInfo token(TK_OTHER, last_line.substr(last_line.length(), 0));
      violations_.insert(LintViolation(token, kMessage));
    }
  }
}

LintRuleStatus PosixEOFRule::Report() const {
  return LintRuleStatus(violations_, Name(), GetStyleGuideCitation(kTopic));
}

}  // namespace analysis
}  // namespace verilog
