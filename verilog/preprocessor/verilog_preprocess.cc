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

#include "verilog/preprocessor/verilog_preprocess.h"

#include <functional>
#include <iterator>
#include <memory>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "common/lexer/token_generator.h"
#include "common/lexer/token_stream_adapter.h"
#include "common/text/macro_definition.h"
#include "common/text/token_info.h"
#include "common/text/token_stream_view.h"
#include "common/util/container_util.h"
#include "common/util/logging.h"
#include "verilog/parser/verilog_lexer.h"
#include "verilog/parser/verilog_parser.h"  // for verilog_symbol_name()
#include "verilog/parser/verilog_token_enum.h"

namespace verilog {

using verible::TokenGenerator;
using verible::TokenStreamView;
using verible::container::FindOrNull;
using verible::container::InsertOrUpdate;

VerilogPreprocess::VerilogPreprocess(const Config& config) : config_(config) {
  // To avoid having to check at every place if the stack is empty, we always
  // place a toplevel 'conditional' that is always selected.
  // Thus we only need to test in `else and `endif to see if we underrun due
  // to unbalanced statements.
  conditional_block_.push(
      BranchBlock(true, true, verible::TokenInfo::EOFToken()));
}

absl::StatusOr<TokenStreamView::const_iterator>
VerilogPreprocess::ExtractMacroName(const StreamIteratorGenerator& generator) {
  // Next token to expect is macro definition name.
  TokenStreamView::const_iterator token_iter = generator();
  if ((*token_iter)->isEOF()) {
    preprocess_data_.errors.push_back(
        {**token_iter, "unexpected EOF where expecting macro name"});
    return absl::InvalidArgumentError("Unexpected EOF");
  }
  const auto& macro_name = *token_iter;
  if (macro_name->token_enum() != PP_Identifier) {
    preprocess_data_.errors.push_back(
        {**token_iter,
         absl::StrCat("Expected identifier for macro name, but got \"",
                      macro_name->text(), "...\"")});
    return absl::InvalidArgumentError("macro name expected");
  }
  return token_iter;
}

// Copies `define token iterators into a temporary buffer.
// Assumes that the last token of a definition is the un-lexed definition body.
// Tokens are copied from the 'generator' into 'define_tokens'.
absl::Status VerilogPreprocess::ConsumeMacroDefinition(
    const StreamIteratorGenerator& generator, TokenStreamView* define_tokens) {
  auto macro_name_extract = ExtractMacroName(generator);
  if (!macro_name_extract.ok()) {
    return macro_name_extract.status();
  }
  define_tokens->push_back(**macro_name_extract);

  // Everything else covers macro parameters and the definition body.
  TokenStreamView::const_iterator token_iter;
  do {
    token_iter = generator();
    if ((*token_iter)->isEOF()) {
      // Diagnose unexpected EOF downstream instead of erroring here.
      // Other subroutines can give better context about the parsing state.
      define_tokens->push_back(*token_iter);
      return absl::OkStatus();
    }
    define_tokens->push_back(*token_iter);
  } while ((*token_iter)->token_enum() != PP_define_body);
  return absl::OkStatus();
}

// TODO(hzeller): instead of returning a unique ptr to a
// VerilogPreprocessError, these functions should just be non-static,
// fill in the error directly into preprocess_data.errors and
// return an absl::Status,

// Interprets a single macro definition parameter.
// Tokens are scanned by advancing the token_scan iterator (by-reference).
std::unique_ptr<VerilogPreprocessError> VerilogPreprocess::ParseMacroParameter(
    TokenStreamView::const_iterator* token_scan,
    MacroParameterInfo* macro_parameter) {
  auto advance = [](TokenStreamView::const_iterator* scan) { return *++*scan; };
  auto token_iter = **token_scan;
  // Extract macro name.
  if (token_iter->token_enum() != PP_Identifier) {
    return absl::make_unique<VerilogPreprocessError>(
        *token_iter,
        absl::StrCat("expected identifier for macro parameter, but got: ",
                     token_iter->ToString()));
  }
  macro_parameter->name = *token_iter;

  // Check for separator or default text.
  token_iter = advance(token_scan);
  if (token_iter->isEOF()) {
    return absl::make_unique<VerilogPreprocessError>(
        *token_iter, "unexpected EOF while parsing macro parameter");
  }
  if (token_iter->token_enum() == '=') {
    token_iter = advance(token_scan);
    if (token_iter->isEOF()) {
      return absl::make_unique<VerilogPreprocessError>(
          *token_iter,
          "unexpected EOF where macro parameter default text is expected");
    }
    if (token_iter->token_enum() != PP_default_text) {
      return absl::make_unique<VerilogPreprocessError>(
          *token_iter,
          absl::StrCat("expected macro parameter default text, but got: ",
                       token_iter->ToString()));
    }
    // Note: the default parameter text is allowed to be empty.
    macro_parameter->default_value = *token_iter;
    token_iter = advance(token_scan);
  }
  if (token_iter->isEOF()) {
    return absl::make_unique<VerilogPreprocessError>(
        *token_iter,
        "unexpected EOF where expecting macro parameter separator");
  }
  if (token_iter->token_enum() == ',') {
    advance(token_scan);  // Advance to next parameter identifier.
  } else if (token_iter->token_enum() == ')') {
    // Do not advance.
  } else {
    // This case covers an unexpected EOF token.
    return absl::make_unique<VerilogPreprocessError>(
        *token_iter,
        absl::StrCat(
            "expecting macro parameter separator ',', or terminator ')', "
            "but got: ",
            verilog_symbol_name(token_iter->token_enum())));
  }
  return nullptr;
}

// Parses an entire macro definition from header through body text.
// The span of tokens that covers a macro definition is expected to
// be in define_tokens.
std::unique_ptr<VerilogPreprocessError> VerilogPreprocess::ParseMacroDefinition(
    const TokenStreamView& define_tokens, MacroDefinition* macro_definition) {
  auto token_scan = define_tokens.begin() + 2;  // skip `define and the name
  auto token_iter = *token_scan;
  if (token_iter->token_enum() == '(') {
    token_iter = *++token_scan;
    // Scan for macro parameters.
    while (token_iter->token_enum() != ')') {
      MacroParameterInfo macro_parameter;
      auto error_ptr = ParseMacroParameter(&token_scan, &macro_parameter);
      if (error_ptr) return error_ptr;
      macro_definition->AppendParameter(macro_parameter);
      token_iter = *token_scan;
    }  // while there are macro parameters
    // Advance past final ')'.
    token_iter = *++token_scan;
  }
  // The macro definition body follows.
  if (token_iter->token_enum() != PP_define_body) {
    return absl::make_unique<VerilogPreprocessError>(
        *token_iter,
        absl::StrCat("expected macro definition body text, but got: ",
                     token_iter->ToString()));
  }
  macro_definition->SetDefinitionText(*token_iter);
  ++token_scan;
  if (token_scan != define_tokens.end()) {
    token_iter = *token_scan;
    return absl::make_unique<VerilogPreprocessError>(
        *token_iter,
        absl::StrCat("expected no more tokens from macro definition, but got: ",
                     token_iter->ToString()));
  }
  return nullptr;
}

// Responds to `define directives.  Macro definitions are parsed and saved
// for use within the same file.
absl::Status VerilogPreprocess::HandleMacroIdentifier(
    const TokenStreamView::const_iterator
        iter  // points to `MACROIDENTIFIER token
) {
  const absl::string_view sv = (*iter)->text();
  const auto* found =
      FindOrNull(preprocess_data_.macro_definitions, sv.substr(1));
  if (!found) {
    preprocess_data_.errors.push_back(VerilogPreprocessError(
        **iter,
        "Error expanding macro identifier, might not be defined before."));
    return absl::InvalidArgumentError(
        "Error expanding macro identifier, might not be defined before.");
  }
  auto& lexed = found->GetLexedDefinitionText();
  auto iter_generator = verible::MakeConstIteratorStreamer(lexed);
  const auto it_end = lexed.end();
  for (auto it = iter_generator(); it != it_end; it++) {
    preprocess_data_.preprocessed_token_stream.push_back(it);
  }
  return absl::OkStatus();
}

// Stores a macro definition for later use.
void VerilogPreprocess::RegisterMacroDefinition(
    const MacroDefinition& definition) {
  // For now, unconditionally register the macro definition, keeping the last
  // definition if macro is re-defined.
  const bool inserted = InsertOrUpdate(&preprocess_data_.macro_definitions,
                                       definition.Name(), definition);
  if (inserted) return;
  preprocess_data_.warnings.emplace_back(
      VerilogPreprocessError(definition.NameToken(), "Re-defining macro"));
  // TODO(hzeller): multiline warning with 'previously defined here' location
}

absl::Status VerilogPreprocess::ExpandMacro(MacroDefinition* definition) {
  VerilogLexer lexer(definition->DefinitionText().text());
  verible::TokenSequence myseq;
  for (lexer.DoNextToken(); !lexer.GetLastToken().isEOF();
       lexer.DoNextToken()) {
    // handle lexical error
    const verible::TokenInfo& subtoken(lexer.GetLastToken());
    myseq.push_back(subtoken);
  }
  definition->SetLexedDefinitionTokens(myseq);
  return absl::OkStatus();
}

// Responds to `define directives.  Macro definitions are parsed and saved
// for use within the same file.
absl::Status VerilogPreprocess::HandleDefine(
    const TokenStreamView::const_iterator iter,  // points to `define token
    const StreamIteratorGenerator& generator) {
  TokenStreamView define_tokens;
  define_tokens.push_back(*iter);
  if (auto status = ConsumeMacroDefinition(generator, &define_tokens);
      !status.ok())
    return status;
  CHECK_GE(define_tokens.size(), 3)
      << "Macro definition should span at least 3 tokens, but only got "
      << define_tokens.size();
  const verible::TokenSequence::const_iterator macro_name = define_tokens[1];
  verible::MacroDefinition macro_definition(*define_tokens[0], *macro_name);
  const auto parse_error_ptr =
      ParseMacroDefinition(define_tokens, &macro_definition);

  if (!macro_definition.IsCallable() && config_.expand_macros) {
    if (auto status = ExpandMacro(&macro_definition); !status.ok())
      return status;
  }

  if (parse_error_ptr) {
    preprocess_data_.errors.push_back(*parse_error_ptr);
    return absl::InvalidArgumentError("Error parsing macro definition.");
  }

  // Parsing showed that things are syntatically correct.
  // But let's only emit things if we're in an active preprocessing branch.
  if (conditional_block_.top().InSelectedBranch()) {
    RegisterMacroDefinition(macro_definition);

    // For now, forward all definition tokens.
    for (const auto& token : define_tokens) {
      preprocess_data_.preprocessed_token_stream.push_back(token);
    }
  }

  return absl::OkStatus();
}

absl::Status VerilogPreprocess::HandleUndef(
    TokenStreamView::const_iterator undef_it,
    const StreamIteratorGenerator& generator) {
  auto macro_name_extract = ExtractMacroName(generator);
  if (!macro_name_extract.ok()) {
    return macro_name_extract.status();
  }
  const auto& macro_name = *macro_name_extract.value();
  preprocess_data_.macro_definitions.erase(macro_name->text());

  // For now, forward all `undef tokens.
  if (conditional_block_.top().InSelectedBranch()) {
    preprocess_data_.preprocessed_token_stream.push_back(*undef_it);
    preprocess_data_.preprocessed_token_stream.push_back(macro_name);
  }
  return absl::OkStatus();
}

absl::Status VerilogPreprocess::HandleIf(
    const TokenStreamView::const_iterator ifpos,  // `ifdef, `ifndef, `elseif
    const StreamIteratorGenerator& generator) {
  if (!config_.filter_branches) {  // nothing to do.
    preprocess_data_.preprocessed_token_stream.push_back(*ifpos);
    return absl::OkStatus();
  }

  auto macro_name_extract = ExtractMacroName(generator);
  if (!macro_name_extract.ok()) {
    return macro_name_extract.status();
  }
  const auto& macro_name = *macro_name_extract.value();
  const bool negative_if = (*ifpos)->token_enum() == PP_ifndef;
  const auto& defs = preprocess_data_.macro_definitions;
  const bool name_is_defined = defs.find(macro_name->text()) != defs.end();
  const bool condition_met = (name_is_defined ^ negative_if);

  if ((*ifpos)->token_enum() == PP_elsif) {
    if (conditional_block_.size() <= 1) {
      preprocess_data_.errors.push_back({**ifpos, "Unmatched `elsif"});
      return absl::InvalidArgumentError("Unmatched `else");
    }
    if (!conditional_block_.top().UpdateCondition(**ifpos, condition_met)) {
      preprocess_data_.errors.push_back({**ifpos, "`elsif after `else"});
      preprocess_data_.errors.push_back(
          {conditional_block_.top().token(), "Previous `else started here."});
      return absl::InvalidArgumentError("Duplicate `else");
    }
  } else {
    // A new, nested if-branch.
    const bool scope_enabled = conditional_block_.top().InSelectedBranch();
    conditional_block_.push(BranchBlock(scope_enabled, condition_met, **ifpos));
  }
  return absl::OkStatus();
}

absl::Status VerilogPreprocess::HandleElse(
    TokenStreamView::const_iterator else_pos) {
  if (!config_.filter_branches) {  // nothing to do.
    preprocess_data_.preprocessed_token_stream.push_back(*else_pos);
    return absl::OkStatus();
  }

  if (conditional_block_.size() <= 1) {
    preprocess_data_.errors.push_back({**else_pos, "Unmatched `else"});
    return absl::InvalidArgumentError("Unmatched `else");
  }

  if (!conditional_block_.top().StartElse(**else_pos)) {
    preprocess_data_.errors.push_back({**else_pos, "Duplicate `else"});
    preprocess_data_.errors.push_back(
        {conditional_block_.top().token(), "Previous `else started here."});
    return absl::InvalidArgumentError("Duplicate `else");
  }
  return absl::OkStatus();
}

absl::Status VerilogPreprocess::HandleEndif(
    TokenStreamView::const_iterator endif_pos) {
  if (!config_.filter_branches) {  // nothing to do.
    preprocess_data_.preprocessed_token_stream.push_back(*endif_pos);
    return absl::OkStatus();
  }

  if (conditional_block_.size() <= 1) {
    preprocess_data_.errors.push_back({**endif_pos, "Unmatched `endif"});
    return absl::InvalidArgumentError("Unmatched `endif");
  }
  conditional_block_.pop();
  return absl::OkStatus();
}

// Interprets preprocessor tokens as directives that act on this preprocessor
// object and possibly transform the input token stream.
absl::Status VerilogPreprocess::HandleTokenIterator(
    TokenStreamView::const_iterator iter,
    const StreamIteratorGenerator& generator) {
  switch ((*iter)->token_enum()) {
    case PP_define:
      return HandleDefine(iter, generator);
    case PP_undef:
      return HandleUndef(iter, generator);

    case PP_ifdef:
    case PP_ifndef:
    case PP_elsif:
      return HandleIf(iter, generator);
    case PP_else:
      return HandleElse(iter);
    case PP_endif:
      return HandleEndif(iter);
  }
  if (config_.expand_macros && ((*iter)->token_enum() == MacroIdentifier ||
                                (*iter)->token_enum() == MacroIdItem)) {
    return HandleMacroIdentifier(iter);
  }

  // If not return'ed above, any other tokens are passed through unmodified
  // unless filtered by a branch.
  if (conditional_block_.top().InSelectedBranch()) {
    preprocess_data_.preprocessed_token_stream.push_back(*iter);
  }
  return absl::OkStatus();
}

VerilogPreprocessData VerilogPreprocess::ScanStream(
    const TokenStreamView& token_stream) {
  preprocess_data_.preprocessed_token_stream.reserve(token_stream.size());
  auto iter_generator = verible::MakeConstIteratorStreamer(token_stream);
  const auto end = token_stream.end();
  // Token-pulling loop.
  for (auto iter = iter_generator(); iter != end; iter = iter_generator()) {
    const auto status = HandleTokenIterator(iter, iter_generator);
    if (!status.ok()) {
      // Detailed errors are already in preprocessor_data_.errors.
      break;  // For now, stop after first error.
    }
  }

  if (conditional_block_.size() > 1 &&
      preprocess_data_.errors.empty()) {  // Only report if not followup-error
    preprocess_data_.errors.push_back(
        {conditional_block_.top().token(),
         "Unterminated preprocessing conditional here, but never completed at "
         "end of file."});
  }
  return std::move(preprocess_data_);
}

}  // namespace verilog
