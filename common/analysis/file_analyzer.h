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
//   util::Status Analyze(void) {
//     MyLangLexer lexer{data_.Contents()};
//     util::Status lex_status = Tokenize(&lexer);
//     // diagnostics
//     // optional: filter or modify tokens_view_
//     MyLangParser parser;
//     util::Status parse_status = Parse(&parser);
//     // diagnostics
//   }
//
//  private:
//   // language-specific helpers
// };

#ifndef VERIBLE_COMMON_ANALYSIS_FILE_ANALYZER_H_
#define VERIBLE_COMMON_ANALYSIS_FILE_ANALYZER_H_

#include <iosfwd>
#include <string>
#include <vector>

#include "common/lexer/lexer.h"
#include "common/parser/parse.h"
#include "common/text/text_structure.h"
#include "common/text/token_info.h"
#include "common/util/status.h"

namespace verible {

// AnalysisPhase enumerates various analysis phases.
enum class AnalysisPhase {
  kLexPhase,         // for lexical diagnostics
  kPreprocessPhase,  // for diagnostics during preprocessing
  kParsePhase,       // for syntax diagnostics
  // Lint phase handles its own diagnostics.
};

// String representation of phase (needed for CHECK).
std::ostream& operator<<(std::ostream&, const AnalysisPhase&);

// RejectedToken is a categorized error token.
struct RejectedToken {
  TokenInfo token_info;
  AnalysisPhase phase;
  std::string explanation;
};

// FileAnalyzer holds the results of lexing and parsing.
class FileAnalyzer : public TextStructure {
 public:
  explicit FileAnalyzer(absl::string_view contents, absl::string_view filename)
      : TextStructure(contents), filename_(filename), rejected_tokens_() {}

  virtual ~FileAnalyzer() {}

  virtual util::Status Tokenize() = 0;

  // Break file contents (string) into tokens.
  util::Status Tokenize(Lexer* lexer);

  // Construct ConcreteSyntaxTree from TokenStreamView.
  util::Status Parse(Parser* parser);

  // Diagnostic message for one rejected token.
  std::string TokenErrorMessage(const TokenInfo&) const;

  // Collect diagnostic messages for rejected tokens.
  std::vector<std::string> TokenErrorMessages() const;

  // Diagnostic message for rejected tokens for linter.
  std::string LinterTokenErrorMessage(const RejectedToken&) const;

  std::vector<std::string> LinterTokenErrorMessages() const;

  const std::vector<RejectedToken>& GetRejectedTokens() const {
    return rejected_tokens_;
  }

 protected:
  // Name of file being analyzed (optional).
  const std::string filename_;

  // Locations of syntax-rejected tokens.
  std::vector<RejectedToken> rejected_tokens_;
};

}  // namespace verible

#endif  // VERIBLE_COMMON_ANALYSIS_FILE_ANALYZER_H_
