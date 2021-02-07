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

#include "verilog/analysis/json_diagnostics.h"

#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "common/analysis/file_analyzer.h"
#include "common/strings/line_column_map.h"
#include "json/value.h"
#include "verilog/analysis/verilog_analyzer.h"

namespace verilog {

// Returns AnalysisPhase as JSON value.
// Try not to change. External tools may use these values as a constant phase
// IDs.
static Json::Value analysis_phase_to_json(const verible::AnalysisPhase& phase) {
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

Json::Value GetLinterTokenErrorsAsJson(
    const verilog::VerilogAnalyzer* analyzer) {
  Json::Value syntax_errors = Json::arrayValue;

  const std::vector<verible::RejectedToken>& rejected_tokens =
      analyzer->GetRejectedTokens();
  for (const auto& rejected_token : rejected_tokens) {
    Json::Value& error = syntax_errors.append(Json::objectValue);

    const absl::string_view base_text = analyzer->Data().Contents();
    const verible::LineColumnMap& line_column_map =
        analyzer->Data().GetLineColumnMap();
    if (!rejected_token.token_info.isEOF()) {
      const auto pos =
          line_column_map(rejected_token.token_info.left(base_text));
      error["line"] = pos.line;
      error["column"] = pos.column;
      error["text"] = std::string(rejected_token.token_info.text());
    } else {
      const int file_size = base_text.length();
      const auto pos = line_column_map(file_size);
      error["line"] = pos.line;
      error["column"] = pos.column;
      error["text"] = "<EOF>";
    }
    error["phase"] = analysis_phase_to_json(rejected_token.phase);

    if (!rejected_token.explanation.empty()) {
      error["message"] = rejected_token.explanation;
    }
  }

  return syntax_errors;
}

}  // namespace verilog
