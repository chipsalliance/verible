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

#include "verible/verilog/analysis/json-diagnostics.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "nlohmann/json.hpp"
#include "verible/common/analysis/file-analyzer.h"
#include "verible/common/strings/line-column-map.h"
#include "verible/verilog/analysis/verilog-analyzer.h"

using nlohmann::json;
using verible::AnalysisPhase;
using verible::ErrorSeverity;
using verible::LineColumnRange;

namespace verilog {

// Returns AnalysisPhase as JSON value.
// Try not to change. External tools may use these values as a constant phase
// IDs.
static json analysis_phase_to_json(const verible::AnalysisPhase &phase) {
  switch (phase) {
    case verible::AnalysisPhase::kLexPhase:
      return "lex";
    case verible::AnalysisPhase::kPreprocessPhase:
      return "preprocess";
    case verible::AnalysisPhase::kParsePhase:
      return "parse";
    default:
      return "unknown";
  }
}

json GetLinterTokenErrorsAsJson(const verilog::VerilogAnalyzer *analyzer,
                                size_t limit) {
  json syntax_errors = json::array();

  const std::vector<verible::RejectedToken> &rejected_tokens =
      analyzer->GetRejectedTokens();
  for (const auto &rejected_token : rejected_tokens) {
    json &error = syntax_errors.emplace_back(json::object());

    analyzer->ExtractLinterTokenErrorDetail(
        rejected_token,
        [&error](const std::string &filename, LineColumnRange range,
                 ErrorSeverity severity, AnalysisPhase phase,
                 std::string_view token_text, std::string_view context_line,
                 const std::string &message) {
          // TODO: should this do something different for severity = kWarning ?
          error["line"] = range.start.line;  // NB: zero based index
          error["column"] = range.start.column;
          error["text"] = std::string(token_text);
          error["phase"] = analysis_phase_to_json(phase);
          if (!message.empty()) error["message"] = message;
        });
    if (limit && --limit == 0) break;
  }

  return syntax_errors;
}

}  // namespace verilog
