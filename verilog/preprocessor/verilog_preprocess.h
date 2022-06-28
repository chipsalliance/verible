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
#include <stack>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "common/text/macro_definition.h"
#include "common/text/token_info.h"
#include "common/text/token_stream_view.h"

namespace verilog {

// TODO(fangism): configuration policy enums.

// VerilogPreprocessError contains preprocessor error information.
// TODO(hzeller): should we just use verible::RejectedToken here ?
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
  using TokenSequence = std::vector<verible::TokenInfo>;

  // Resulting token stream after preprocessing
  verible::TokenStreamView preprocessed_token_stream;
  std::vector<TokenSequence> lexed_macros_backup;

  // Map of defined macros.
  MacroDefinitionRegistry macro_definitions;

  // Sequence of tokens rejected by preprocessing.
  // TODO(hzeller): Until we have a severity in VerilogPreprocessError, these
  // are two separate vectors.
  std::vector<VerilogPreprocessError> errors;
  std::vector<VerilogPreprocessError> warnings;
};

// VerilogPreprocess transforms a TokenStreamView.
// The input stream view is expected to have been stripped of whitespace.
class VerilogPreprocess {
  using TokenStreamView = verible::TokenStreamView;
  using MacroDefinition = verible::MacroDefinition;
  using MacroParameterInfo = verible::MacroParameterInfo;

 public:
  struct Config {
    // Filter out non-matching `ifdef and `ifndef branches depending on
    // which defines are set.
    //
    // This option is useful for syntax check and possibly linting in
    // case parsing fails with all consecutive branches emitted.
    //
    // Note, for formatting, we do _not_ want to filter the output as we
    // want to emit all tokens.
    bool filter_branches = false;

    // Expand macro definition bodies, this will relexes the macro body.
    bool expand_macros = false;
    // TODO(hzeller): Provide a map of command-line provided +define+'s
  };

  explicit VerilogPreprocess(const Config& config);

  // Initialize preprocessing with safe default options
  // TODO(hzeller): remove this constructor once all places using the
  // preprocessor have been updated to pass a config.
  VerilogPreprocess() : VerilogPreprocess(Config()){};

  // ScanStream reads in a stream of tokens returns the result as a move
  // of preprocessor_data_.  preprocessor_data_ should not be accessed
  // after this returns.
  VerilogPreprocessData ScanStream(const TokenStreamView& token_stream);

  // TODO(fangism): ExpandMacro, ExpandMacroCall
  // TODO(b/111544845): ExpandEvalStringLiteral

 private:
  using StreamIteratorGenerator =
      std::function<TokenStreamView::const_iterator()>;

  // Extract macro name after `define, `ifdef, `elsif ... and returns
  // iterator of macro name or a failure status.
  // Updates error messages on failure.
  absl::StatusOr<TokenStreamView::const_iterator> ExtractMacroName(
      const StreamIteratorGenerator&);

  absl::Status HandleTokenIterator(TokenStreamView::const_iterator,
                                   const StreamIteratorGenerator&);
  absl::Status HandleMacroIdentifier(TokenStreamView::const_iterator,
                                     const StreamIteratorGenerator&,
                                     bool forward);

  absl::Status HandleDefine(TokenStreamView::const_iterator,
                            const StreamIteratorGenerator&);
  absl::Status HandleUndef(TokenStreamView::const_iterator,
                           const StreamIteratorGenerator&);

  absl::Status HandleIf(TokenStreamView::const_iterator ifpos,
                        const StreamIteratorGenerator&);
  absl::Status HandleElse(TokenStreamView::const_iterator else_pos);
  absl::Status HandleEndif(TokenStreamView::const_iterator endif_pos);

  static absl::Status ConsumeAndParseMacroCall(TokenStreamView::const_iterator,
                                               const StreamIteratorGenerator&,
                                               verible::MacroCall*,
                                               const verible::MacroDefinition&);

  // The following functions return nullptr when there is no error:
  absl::Status ConsumeMacroDefinition(const StreamIteratorGenerator&,
                                      TokenStreamView*);

  static std::unique_ptr<VerilogPreprocessError> ParseMacroDefinition(
      const TokenStreamView&, MacroDefinition*);

  static std::unique_ptr<VerilogPreprocessError> ParseMacroParameter(
      TokenStreamView::const_iterator*, MacroParameterInfo*);

  void RegisterMacroDefinition(const MacroDefinition&);
  absl::Status ExpandText(const absl::string_view&);
  absl::Status ExpandMacro(const verible::MacroCall&,
                           const verible::MacroDefinition*);

  const Config config_;

  // State of a block that can have a sequence of sub-blocks with conditions
  // (ifdef/elsif/else/endif). Only the first of the subblock whose condition
  // matches will be selected - or the else block if none before matched.
  class BranchBlock {
   public:
    BranchBlock(bool is_enabled, bool condition,
                const verible::TokenInfo& token)
        : outer_scope_enabled_(is_enabled), branch_token_(token) {
      UpdateCondition(token, condition);
    }

    // Is the current block selected, i.e. should tokens pass through.
    bool InSelectedBranch() const {
      return outer_scope_enabled_ && current_branch_condition_met_;
    }

    // Update condition e.g. in `elsif. Return 'false' if already in an
    // else clause.
    bool UpdateCondition(const verible::TokenInfo& token, bool condition) {
      if (in_else_) return false;
      branch_token_ = token;
      // Only the first in a row of matching conditions will select block.
      current_branch_condition_met_ = !any_branch_matched_ && condition;
      any_branch_matched_ |= condition;
      return true;
    }

    // Start an `else block. Uses its internal state to determine if
    // this will put is InSelectedBranch().
    // Returns 'false' if already in an else block.
    bool StartElse(const verible::TokenInfo& token) {
      if (in_else_) return false;
      in_else_ = true;
      branch_token_ = token;
      current_branch_condition_met_ = !any_branch_matched_;
      return true;
    }

    const verible::TokenInfo& token() const { return branch_token_; }

   private:
    const bool outer_scope_enabled_;
    verible::TokenInfo branch_token_;  // FYI for error reporting.
    bool any_branch_matched_ = false;  // only if no branch, `else will
    bool in_else_ = false;
    bool current_branch_condition_met_;
  };

  // State of nested conditional blocks. For code simplicity, this always has
  // a toplevel branch that is selected.
  std::stack<BranchBlock> conditional_block_;

  // Results of preprocessing
  VerilogPreprocessData preprocess_data_;
};

}  // namespace verilog

#endif  // VERIBLE_VERILOG_PREPROCESSOR_VERILOG_PREPROCESS_H_
