// Copyright 2021 The Verible Authors.
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
//

#include "verilog/tools/ls/verible-lsp-adapter.h"

#include "common/lsp/lsp-protocol-operators.h"
#include "common/lsp/lsp-protocol.h"
#include "nlohmann/json.hpp"
#include "verilog/analysis/verilog_analyzer.h"
#include "verilog/analysis/verilog_linter.h"
#include "verilog/tools/ls/lsp-parse-buffer.h"

namespace verilog {
// Convert our representation of a linter violation to a LSP-Diagnostic
static verible::lsp::Diagnostic ViolationToDiagnostic(
    const verilog::LintViolationWithStatus &v, absl::string_view base,
    const verible::LineColumnMap &lc_map) {
  const verible::LintViolation &violation = *v.violation;
  verible::LineColumn start = lc_map(violation.token.left(base));
  verible::LineColumn end = lc_map(violation.token.right(base));
  const char *fix_msg = violation.autofixes.empty() ? "" : " (fix available)";
  return verible::lsp::Diagnostic{
      .range =
          {
              .start = {.line = start.line, .character = start.column},
              .end = {.line = end.line, .character = end.column},
          },
      .message = absl::StrCat(violation.reason, " ", v.status->url, "[",
                              v.status->lint_rule_name, "]", fix_msg),
  };
}

std::vector<verible::lsp::Diagnostic> CreateDiagnostics(
    const BufferTracker &tracker) {
  // Diagnostics should come from the latest state, including all the
  // syntax errors.
  const ParsedBuffer *const current = tracker.current();
  if (!current) return {};
  // TODO: files that generate a lot of messages will create a huge
  // output. So we limit the messages here.
  // However, we should work towards emitting them around the last known
  // edit point in the document as this is what the user sees.
  static constexpr int kMaxMessages = 100;
  const auto &rejected_tokens = current->parser().GetRejectedTokens();
  auto const &lint_violations =
      verilog::GetSortedViolations(current->lint_result());
  std::vector<verible::lsp::Diagnostic> result;
  int remaining = rejected_tokens.size() + lint_violations.size();
  if (remaining > kMaxMessages) remaining = kMaxMessages;
  result.reserve(remaining);
  for (const auto &rejected_token : rejected_tokens) {
    current->parser().ExtractLinterTokenErrorDetail(
        rejected_token,
        [&result](const std::string &filename, verible::LineColumnRange range,
                  verible::AnalysisPhase phase, absl::string_view token_text,
                  absl::string_view context_line, const std::string &msg) {
          // Note: msg is currently empty and not useful.
          const auto message = (phase == verible::AnalysisPhase::kLexPhase)
                                   ? "token error"
                                   : "syntax error";
          result.emplace_back(verible::lsp::Diagnostic{
              .range{.start{.line = range.start.line,
                            .character = range.start.column},
                     .end{.line = range.end.line,  //
                          .character = range.end.column}},
              .message = message,
          });
        });
    if (--remaining <= 0) break;
  }

  const absl::string_view base = current->parser().Data().Contents();
  verible::LineColumnMap line_column_map(base);
  for (const auto &v : lint_violations) {
    result.emplace_back(ViolationToDiagnostic(v, base, line_column_map));
    if (--remaining <= 0) break;
  }
  return result;
}

}  // namespace verilog
