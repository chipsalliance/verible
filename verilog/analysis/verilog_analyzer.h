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

#ifndef VERIBLE_VERILOG_ANALYSIS_VERILOG_ANALYZER_H_
#define VERIBLE_VERILOG_ANALYSIS_VERILOG_ANALYZER_H_

#include <cstddef>
#include <iosfwd>
#include <memory>
#include <string>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "common/analysis/file_analyzer.h"
#include "common/text/token_stream_view.h"
#include "verilog/preprocessor/verilog_preprocess.h"

namespace verilog {

// VerilogAnalyzer analyzes Verilog and SystemVerilog code syntax.
class VerilogAnalyzer : public verible::FileAnalyzer {
 public:
  VerilogAnalyzer(absl::string_view text, absl::string_view name,
                  bool use_parser_directive_comments = true)
      : verible::FileAnalyzer(text, name),
        max_used_stack_size_(0),
        use_parser_directive_comments_(use_parser_directive_comments) {}

  // Lex-es the input text into tokens.
  absl::Status Tokenize() override;

  // Create token stream view without comments and whitespace.
  // The retained tokens will become leaves of a concrete syntax tree.
  void FilterTokensForSyntaxTree();

  // Analyzes the syntax and structure of a source file (lex and parse).
  // Result of parsing is stored in syntax_tree_, which may contain gaps
  // if there are syntax errors.
  absl::Status Analyze();

  absl::Status LexStatus() const { return lex_status_; }

  absl::Status ParseStatus() const { return parse_status_; }

  size_t MaxUsedStackSize() const { return max_used_stack_size_; }

  // Automatically analyze with the correct parsing mode, as detected
  // by parser directive comments.
  static std::unique_ptr<VerilogAnalyzer> AnalyzeAutomaticMode(
      absl::string_view text, absl::string_view name);

  const VerilogPreprocessData& PreprocessorData() const {
    return preprocessor_data_;
  }

  // Maybe this belongs in a subclass like VerilogFileAnalyzer?
  // TODO(fangism): Retain a copy of the token stream transformer because it
  // may contain tokens backed by generated text.

 protected:
  // Apply context-based disambiguation of tokens.
  void ContextualizeTokens();

  // Scan comments for parsing mode directives.
  // Returns a string that is first argument of the directive, e.g.:
  //     // verilog_syntax: mode-x
  // results in "mode-x".
  static absl::string_view ScanParsingModeDirective(
      const verible::TokenSequence& raw_tokens);

  // Special string inside a comment that triggers setting parsing mode.
  static const char kParseDirectiveName[];

 private:
  // Attempt to parse all macro arguments as expressions.  Where parsing as an
  // expession succeeds, substitute the leaf with a node with the expression's
  // syntax tree.  If parsing fails, leave the MacroArg token unexpanded.
  void ExpandMacroCallArgExpressions();

  // Information about parser internals.

  // True if input text has already been lexed.
  bool tokenized_ = false;

  // Maximum symbol stack depth.
  size_t max_used_stack_size_;

  // Preprocessor.
  VerilogPreprocessData preprocessor_data_;

  // If true, let comments control the parsing mode.
  bool use_parser_directive_comments_ = true;

  // Status of lexing.
  absl::Status lex_status_;

  // Status of parsing.
  absl::Status parse_status_;
};

}  // namespace verilog

#endif  // VERIBLE_VERILOG_ANALYSIS_VERILOG_ANALYZER_H_
