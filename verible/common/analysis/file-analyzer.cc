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

// Implementation of FileAnalyzer methods.

#include "verible/common/analysis/file-analyzer.h"

#include <algorithm>
#include <ostream>
#include <sstream>  // IWYU pragma: keep  // for ostringstream
#include <string>
#include <string_view>
#include <vector>

#include "absl/status/status.h"
#include "verible/common/lexer/lexer.h"
#include "verible/common/lexer/token-stream-adapter.h"
#include "verible/common/parser/parse.h"
#include "verible/common/strings/line-column-map.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/text-structure.h"
#include "verible/common/text/token-info.h"
#include "verible/common/text/token-stream-view.h"
#include "verible/common/util/logging.h"
#include "verible/common/util/spacer.h"

namespace verible {

// Translates phase enum into string for diagnostic messages.
const char *AnalysisPhaseName(const AnalysisPhase &phase) {
  switch (phase) {
    case AnalysisPhase::kLexPhase:
      return "lexical";
    case AnalysisPhase::kPreprocessPhase:
      return "preprocessing";
    case AnalysisPhase::kParsePhase:
      return "syntax";
  }
  return "UNKNOWN";
}
std::ostream &operator<<(std::ostream &stream, const AnalysisPhase &phase) {
  return stream << AnalysisPhaseName(phase);
}

const char *ErrorSeverityDescription(const ErrorSeverity &severity) {
  switch (severity) {
    case ErrorSeverity::kError:
      return "error";
    case ErrorSeverity::kWarning:
      return "warning";
  }
  return "UNKNOWN";
}
std::ostream &operator<<(std::ostream &stream, const ErrorSeverity &severity) {
  return stream << ErrorSeverityDescription(severity);
}

std::ostream &operator<<(std::ostream &stream, const RejectedToken &r) {
  return stream << r.token_info << " (" << r.phase << " " << r.severity
                << "): " << r.explanation;
}

// Grab tokens until EOF, and initialize a stream view with all tokens.
absl::Status FileAnalyzer::Tokenize(Lexer *lexer) {
  const auto buffer = Data().Contents();
  TokenSequence &tokens = MutableData().MutableTokenStream();

  if (auto lex_status = MakeTokenSequence(
          lexer, buffer, &tokens,
          [&](const TokenInfo &error_token) {
            VLOG(1) << "Lexical error with token: " << error_token;
            // Save error details in rejected_tokens_.
            rejected_tokens_.push_back(
                RejectedToken{error_token, AnalysisPhase::kLexPhase,
                              "" /* no detailed explanation */});
          });
      !lex_status.ok()) {
    return lex_status;
  }

  // Partition token stream into line-by-line slices.
  MutableData().CalculateFirstTokensPerLine();

  // Initialize filtered view of token stream.
  InitTokenStreamView(tokens, &MutableData().MutableTokenStreamView());
  return absl::OkStatus();
}

// Runs the parser on the current TokenStreamView.
absl::Status FileAnalyzer::Parse(Parser *parser) {
  absl::Status status = parser->Parse();
  // Transfer syntax tree root, even if there were (recovered) syntax errors,
  // because the partial tree can still be useful to analyze.
  MutableData().MutableSyntaxTree() = parser->TakeRoot();
  if (status.ok()) {
    CHECK(Data().SyntaxTree().get()) << "Expected syntax tree from parsing \""
                                     << filename_ << "\", but got none.";
  } else {
    for (const auto &token : parser->RejectedTokens()) {
      rejected_tokens_.push_back(RejectedToken{
          token, AnalysisPhase::kParsePhase, "" /* no detailed explanation */});
    }
  }
  return status;
}

// Reports human-readable token error.
std::string FileAnalyzer::TokenErrorMessage(
    const TokenInfo &error_token) const {
  // TODO(fangism): accept a RejectedToken to get an explanation message.
  std::ostringstream output_stream;
  if (!error_token.isEOF()) {
    const LineColumnRange range = Data().GetRangeForToken(error_token);
    output_stream << "token: \"" << error_token.text() << "\" at " << range;
  } else {
    const auto end = Data().GetLineColAtOffset(Data().Contents().length());
    output_stream << "token: <<EOF>> at " << end;
  }
  return output_stream.str();
}

std::vector<std::string> FileAnalyzer::TokenErrorMessages() const {
  std::vector<std::string> messages;
  messages.reserve(rejected_tokens_.size());
  for (const auto &rejected_token : rejected_tokens_) {
    messages.push_back(TokenErrorMessage(rejected_token.token_info));
  }
  return messages;
}

void FileAnalyzer::ExtractLinterTokenErrorDetail(
    const RejectedToken &error_token,
    const ReportLinterErrorFunction &error_report) const {
  const LineColumnRange range = Data().GetRangeForToken(error_token.token_info);
  std::string_view context_line;
  const auto &lines = Data().Lines();
  if (range.start.line < static_cast<int>(lines.size())) {
    context_line = lines[range.start.line];
  }
  // TODO(b/63893567): Explain syntax errors by inspecting state stack.
  error_report(
      filename_, range, error_token.severity, error_token.phase,
      error_token.token_info.isEOF() ? "<EOF>" : error_token.token_info.text(),
      context_line, error_token.explanation);
}

std::string FileAnalyzer::LinterTokenErrorMessage(
    const RejectedToken &error_token, bool diagnostic_context) const {
  std::ostringstream out;
  ExtractLinterTokenErrorDetail(
      error_token,
      [&](const std::string &filename, LineColumnRange range,
          ErrorSeverity severity, AnalysisPhase phase,
          std::string_view token_text, std::string_view context_line,
          const std::string &message) {
        out << filename_ << ':' << range << " " << phase << " " << severity;
        if (error_token.token_info.isEOF()) {
          out << " (unexpected EOF)";
        } else {
          out << " at token \"" << token_text << "\"";
        }
        if (!message.empty()) {
          out << " : " << message;
        }
        if (diagnostic_context && !context_line.empty()) {
          // Need to get rid of all tabs so that spacing
          std::string no_tab_line(context_line.begin(), context_line.end());
          std::replace(no_tab_line.begin(), no_tab_line.end(), '\t', ' ');
          out << "\n" << no_tab_line << std::endl;
          out << verible::Spacer(range.start.column) << "^";
        }
      });
  return out.str();
}

std::vector<std::string> FileAnalyzer::LinterTokenErrorMessages(
    bool diagnostic_context) const {
  std::vector<std::string> messages;
  messages.reserve(rejected_tokens_.size());
  for (const auto &rejected_token : rejected_tokens_) {
    messages.push_back(
        LinterTokenErrorMessage(rejected_token, diagnostic_context));
  }
  return messages;
}

}  // namespace verible
