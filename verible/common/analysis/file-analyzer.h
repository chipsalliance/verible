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

// FileAnalyzer holds the results of lexing and parsing.
// Internally, it owns a copy of the source text as a string,
// and scanned Tokens pointing to substrings as string_views.
// Subclasses are expected to call Tokenize(), and possibly perform
// other actions and refinements on the TokenStreamView, before
// calling Parse().
//
// usage:
// class MyLangFileAnalyzer : public FileAnalyzer {
//  public:
//   absl::Status Analyze(void) {
//     MyLangLexer lexer{data_.Contents()};
//     absl::Status lex_status = Tokenize(&lexer);
//     // diagnostics
//     // optional: filter or modify tokens_view_
//     MyLangParser parser;
//     absl::Status parse_status = Parse(&parser);
//     // diagnostics
//   }
//
//  private:
//   // language-specific helpers
// };

#ifndef VERIBLE_COMMON_ANALYSIS_FILE_ANALYZER_H_
#define VERIBLE_COMMON_ANALYSIS_FILE_ANALYZER_H_

#include <functional>
#include <iosfwd>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "verible/common/lexer/lexer.h"
#include "verible/common/parser/parse.h"
#include "verible/common/strings/line-column-map.h"
#include "verible/common/strings/mem-block.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/text-structure.h"
#include "verible/common/text/token-info.h"
#include "verible/common/util/logging.h"

namespace verible {

// AnalysisPhase enumerates various analysis phases.
enum class AnalysisPhase {
  kLexPhase,         // for lexical diagnostics
  kPreprocessPhase,  // for diagnostics during preprocessing
  kParsePhase,       // for syntax diagnostics
  // Lint phase handles its own diagnostics.
};

// String representation of phase (needed for CHECK).
const char *AnalysisPhaseName(const AnalysisPhase &phase);
std::ostream &operator<<(std::ostream &, const AnalysisPhase &);

enum class ErrorSeverity {
  kError,
  kWarning,
};
const char *ErrorSeverityDescription(const ErrorSeverity &severity);
std::ostream &operator<<(std::ostream &, const ErrorSeverity &);

// RejectedToken is a categorized warning/error token.
// TODO(hzeller): In the presence of warnings, this probably needs to be
// renamed, as a token is not rejected per-se.
struct RejectedToken {
  TokenInfo token_info;
  AnalysisPhase phase;
  std::string explanation;
  ErrorSeverity severity = ErrorSeverity::kError;
};

std::ostream &operator<<(std::ostream &, const RejectedToken &);

// FileAnalyzer holds the results of lexing and parsing.
class FileAnalyzer {
 public:
  FileAnalyzer(std::shared_ptr<MemBlock> contents, std::string_view filename)
      : text_structure_(new TextStructure(std::move(contents))),
        filename_(filename) {}

  // Legacy constructor.
  FileAnalyzer(std::string_view contents, std::string_view filename)
      : text_structure_(new TextStructure(contents)), filename_(filename) {}

  virtual ~FileAnalyzer() = default;

  virtual absl::Status Tokenize() = 0;

  // Break file contents (string) into tokens.
  absl::Status Tokenize(Lexer *lexer);

  // Construct ConcreteSyntaxTree from TokenStreamView.
  absl::Status Parse(Parser *parser);

  // Diagnostic message for one rejected token.
  std::string TokenErrorMessage(const TokenInfo &) const;

  // Collect diagnostic messages for rejected tokens.
  std::vector<std::string> TokenErrorMessages() const;

  // TODO(hzeller): these are not really 'linter' messages but lexing
  // and parsing issues. So ExtractParseErrorDetails() would be more
  // appropriate.

  // Function to receive break-down of an issue with a rejected token.
  // "filename" is the filename the error occured, "phase" is the phase
  // such as lexing/parsing. "range" is the range of the reported region
  // with lines/columns.
  // "token_text" is the exact text of the token.
  // The "context_line" is the line in which the corresponding error happened.
  // The "message" finally is a human-readable error message
  // TODO(hzeller): these are a lot of parameters, maybe a struct would be good.
  using ReportLinterErrorFunction = std::function<void(
      const std::string &filename, LineColumnRange range,
      ErrorSeverity severity, AnalysisPhase phase, std::string_view token_text,
      std::string_view context_line, const std::string &message)>;

  // Extract detailed diagnostic information for rejected token.
  void ExtractLinterTokenErrorDetail(
      const RejectedToken &error_token,
      const ReportLinterErrorFunction &error_report) const;

  // -- convenience functions using the above

  // Diagnostic message for rejected tokens for linter.
  // Second argument is the show_context option. When enabled
  // additional diagnostic line is concatenated to an error message
  // with marker that points to vulnerable token
  std::string LinterTokenErrorMessage(const RejectedToken &, bool) const;

  // First argument is the show_context option. When enabled
  // additional diagnostic line is concatenated to an error message
  // with marker that points to vulnerable token
  std::vector<std::string> LinterTokenErrorMessages(bool) const;

  const std::vector<RejectedToken> &GetRejectedTokens() const {
    return rejected_tokens_;
  }

  // Convenience methods to access text structure view.
  const ConcreteSyntaxTree &SyntaxTree() const {
    return ABSL_DIE_IF_NULL(text_structure_)->SyntaxTree();
  }
  const TextStructureView &Data() const {
    return ABSL_DIE_IF_NULL(text_structure_)->Data();
  }
  TextStructureView &MutableData() {
    return ABSL_DIE_IF_NULL(text_structure_)->MutableData();
  }

  // Return text structure used in this analysis.
  // This FileAnalyzer object must be considered invalid afterwards.
  // TODO(hzeller): drive decomposition further so that we don't need this wart.
  std::unique_ptr<TextStructure> ReleaseTextStructure() {
    return std::move(text_structure_);
  }

 protected:
  std::unique_ptr<TextStructure> text_structure_;

  // Name of file being analyzed (optional).
  const std::string filename_;

  // Locations of syntax-rejected tokens.
  std::vector<RejectedToken> rejected_tokens_;
};

}  // namespace verible

#endif  // VERIBLE_COMMON_ANALYSIS_FILE_ANALYZER_H_
