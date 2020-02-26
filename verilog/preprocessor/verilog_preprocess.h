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

// VerilogPreprocess is a *pseudo*-preprocessor for Verilog.
// Unlike a conventional preprocessor, this pseudo-preprocessor does not open
// included files, nor does it evaluate preprocessor expressions.
// Instead, it does a best-effort handling of preprocessor directives
// locally within one file, and no additional context.
// For example, it may expand a macro call if its definition happens to be
// available, but it is not required to do so.
// The pseudo-preprocessor is free to evaluate any/all/no conditional
// branches.
// Each analysis tool may configure the pseudo-preprocessor differently.

// TODO(fangism): expand macros if locally defined, and feed un-lexed
//   body text to lexer.  This approach works if the definition text
//   does not depend on the start-condition state at the macro call site.
// TODO(fangism): implement conditional evaluation policy (`ifdef, `else, ...)
// TODO(fangism): token concatenation, e.g. a``b
//   This will produce tokens that are not in the original source text.
// TODO(fangism): token string-ification (turning symbol names into strings)
// TODO(fangism): evaluate `defines inside `defines at expansion time.

#ifndef VERIBLE_VERILOG_PREPROCESSOR_VERILOG_PREPROCESS_H_
#define VERIBLE_VERILOG_PREPROCESSOR_VERILOG_PREPROCESS_H_

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "common/text/macro_definition.h"
#include "common/text/token_info.h"
#include "common/text/token_stream_view.h"

namespace verilog {

// TODO(fangism): configuration policy enums.

// VerilogPreprocessError contains preprocessor error information.
struct VerilogPreprocessError {
  verible::TokenInfo token_info;  // offending token
  std::string error_message;

  VerilogPreprocessError(const verible::TokenInfo& token,
                         const std::string& message)
      : token_info(token), error_message(message) {}
};

// Information that results from preprocessing.
struct VerilogPreprocessData {
  using MacroDefinition = verible::MacroDefinition;
  using MacroDefinitionRegistry = std::map<absl::string_view, MacroDefinition>;

  // Resulting token stream after preprocessing
  verible::TokenStreamView preprocessed_token_stream;

  // Map of defined macros.
  MacroDefinitionRegistry macro_definitions;

  // Sequence of tokens rejected by preprocessing.
  std::vector<VerilogPreprocessError> errors;
};

// VerilogPreprocess transforms a TokenStreamView.
// The input stream view is expected to have been stripped of whitespace.
class VerilogPreprocess {
  using TokenStreamView = verible::TokenStreamView;
  using MacroDefinition = verible::MacroDefinition;
  using MacroParameterInfo = verible::MacroParameterInfo;

 public:
  VerilogPreprocess() : preprocess_data_() {}

  // ScanStream reads in a stream of tokens returns the result as a move
  // of preprocessor_data_.  preprocessor_data_ should not be accessed
  // after this returns.
  VerilogPreprocessData ScanStream(const TokenStreamView& token_stream);

  // TODO(fangism): ExpandMacro, ExpandMacroCall
  // TODO(b/111544845): ExpandEvalStringLiteral

 private:
  using StreamIteratorGenerator =
      std::function<TokenStreamView::const_iterator()>;

  absl::Status HandleTokenIterator(const TokenStreamView::const_iterator,
                                   const StreamIteratorGenerator&);

  absl::Status HandleDefine(const TokenStreamView::const_iterator,
                            const StreamIteratorGenerator&);

  // The following functions return nullptr when there is no error:
  static std::unique_ptr<VerilogPreprocessError> ConsumeMacroDefinition(
      const StreamIteratorGenerator&, TokenStreamView*);

  static std::unique_ptr<VerilogPreprocessError> ParseMacroDefinition(
      const TokenStreamView&, MacroDefinition*);

  static std::unique_ptr<VerilogPreprocessError> ParseMacroParameter(
      TokenStreamView::const_iterator*, MacroParameterInfo*);

  void RegisterMacroDefinition(const MacroDefinition&);

  // Results of preprocessing
  VerilogPreprocessData preprocess_data_;
};

}  // namespace verilog

#endif  // VERIBLE_VERILOG_PREPROCESSOR_VERILOG_PREPROCESS_H_
