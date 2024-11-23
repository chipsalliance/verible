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

#include "verible/verilog/preprocessor/verilog-preprocess.h"

#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "verible/common/lexer/token-generator.h"
#include "verible/common/lexer/token-stream-adapter.h"
#include "verible/common/text/macro-definition.h"
#include "verible/common/text/text-structure.h"
#include "verible/common/text/token-info.h"
#include "verible/common/text/token-stream-view.h"
#include "verible/common/util/container-util.h"
#include "verible/common/util/logging.h"
#include "verible/common/util/status-macros.h"
#include "verible/verilog/analysis/verilog-filelist.h"
#include "verible/verilog/parser/verilog-lexer.h"
#include "verible/verilog/parser/verilog-parser.h"  // for verilog_symbol_name()
#include "verible/verilog/parser/verilog-token-enum.h"

namespace verilog {

using verible::TokenGenerator;
using verible::TokenStreamView;
using verible::container::FindOrNull;
using verible::container::InsertOrUpdate;

VerilogPreprocess::VerilogPreprocess(const Config &config)
    : VerilogPreprocess(config, nullptr) {}

VerilogPreprocess::VerilogPreprocess(const Config &config, FileOpener opener)
    : config_(config), file_opener_(std::move(opener)) {
  // To avoid having to check at every place if the stack is empty, we always
  // place a toplevel 'conditional' that is always selected.
  // Thus we only need to test in `else and `endif to see if we underrun due
  // to unbalanced statements.
  conditional_block_.push(
      BranchBlock(true, true, verible::TokenInfo::EOFToken()));
}

TokenStreamView::const_iterator VerilogPreprocess::GenerateBypassWhiteSpaces(
    const StreamIteratorGenerator &generator) {
  auto iterator =
      generator();  // iterator should be pointing to a non-whitespace token;
  while (verilog::VerilogLexer::KeepSyntaxTreeTokens(**iterator) == 0) {
    iterator = generator();
  }
  return iterator;
}

absl::StatusOr<TokenStreamView::const_iterator>
VerilogPreprocess::ExtractMacroName(const StreamIteratorGenerator &generator) {
  // Next token to expect is macro definition name.
  TokenStreamView::const_iterator token_iter =
      GenerateBypassWhiteSpaces(generator);
  if ((*token_iter)->isEOF()) {
    preprocess_data_.errors.emplace_back(
        **token_iter, "unexpected EOF where expecting macro name");
    return absl::InvalidArgumentError("Unexpected EOF");
  }
  const auto &macro_name = *token_iter;
  if (macro_name->token_enum() != PP_Identifier) {
    preprocess_data_.errors.emplace_back(
        **token_iter,
        absl::StrCat("Expected identifier for macro name, but got \"",
                     macro_name->text(), "...\""));
    return absl::InvalidArgumentError("macro name expected");
  }
  return token_iter;
}

// Copies `define token iterators into a temporary buffer.
// Assumes that the last token of a definition is the un-lexed definition body.
// Tokens are copied from the 'generator' into 'define_tokens'.
absl::Status VerilogPreprocess::ConsumeMacroDefinition(
    const StreamIteratorGenerator &generator, TokenStreamView *define_tokens) {
  auto macro_name_extract = ExtractMacroName(generator);
  if (!macro_name_extract.ok()) {
    return macro_name_extract.status();
  }
  define_tokens->push_back(**macro_name_extract);

  // Everything else covers macro parameters and the definition body.
  TokenStreamView::const_iterator token_iter;
  do {
    token_iter = GenerateBypassWhiteSpaces(generator);
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
    TokenStreamView::const_iterator *token_scan,
    MacroParameterInfo *macro_parameter) {
  auto advance = [](TokenStreamView::const_iterator *scan) { return *++*scan; };
  auto token_iter = **token_scan;
  // Extract macro name.
  if (token_iter->token_enum() != PP_Identifier) {
    return std::make_unique<VerilogPreprocessError>(
        *token_iter,
        absl::StrCat("expected identifier for macro parameter, but got: ",
                     token_iter->ToString()));
  }
  macro_parameter->name = *token_iter;

  // Check for separator or default text.
  token_iter = advance(token_scan);
  if (token_iter->isEOF()) {
    return std::make_unique<VerilogPreprocessError>(
        *token_iter, "unexpected EOF while parsing macro parameter");
  }
  if (token_iter->token_enum() == '=') {
    token_iter = advance(token_scan);
    if (token_iter->isEOF()) {
      return std::make_unique<VerilogPreprocessError>(
          *token_iter,
          "unexpected EOF where macro parameter default text is expected");
    }
    if (token_iter->token_enum() != PP_default_text) {
      return std::make_unique<VerilogPreprocessError>(
          *token_iter,
          absl::StrCat("expected macro parameter default text, but got: ",
                       token_iter->ToString()));
    }
    // Note: the default parameter text is allowed to be empty.
    macro_parameter->default_value = *token_iter;
    token_iter = advance(token_scan);
  }
  if (token_iter->isEOF()) {
    return std::make_unique<VerilogPreprocessError>(
        *token_iter,
        "unexpected EOF where expecting macro parameter separator");
  }
  if (token_iter->token_enum() == ',') {
    advance(token_scan);  // Advance to next parameter identifier.
  } else if (token_iter->token_enum() == ')') {
    // Do not advance.
  } else {
    // This case covers an unexpected EOF token.
    return std::make_unique<VerilogPreprocessError>(
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
    const TokenStreamView &define_tokens, MacroDefinition *macro_definition) {
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
    return std::make_unique<VerilogPreprocessError>(
        *token_iter,
        absl::StrCat("expected macro definition body text, but got: ",
                     token_iter->ToString()));
  }
  macro_definition->SetDefinitionText(*token_iter);
  ++token_scan;
  if (token_scan != define_tokens.end()) {
    token_iter = *token_scan;
    return std::make_unique<VerilogPreprocessError>(
        *token_iter,
        absl::StrCat("expected no more tokens from macro definition, but got: ",
                     token_iter->ToString()));
  }
  return nullptr;
}

// Parses a callable macro actual parameters, and saves it into a MacroCall
absl::Status VerilogPreprocess::ConsumeAndParseMacroCall(
    TokenStreamView::const_iterator iter,
    const StreamIteratorGenerator &generator, verible::MacroCall *macro_call,
    const verible::MacroDefinition &macro_definition) {
  // Parsing the macro .
  const absl::string_view macro_name_str = (*iter)->text().substr(1);
  verible::TokenInfo macro_name_token(MacroCallId, macro_name_str);
  macro_call->macro_name = macro_name_token;

  // Checking if the macro has formal parameters.
  if (!macro_definition.IsCallable()) {
    macro_call->has_parameters = false;
    return absl::OkStatus();
  }
  macro_call->has_parameters = true;

  // Parsing parameters.
  TokenStreamView::const_iterator token_iter =
      GenerateBypassWhiteSpaces(generator);
  int parameters_size = macro_definition.Parameters().size();
  if ((*token_iter)->text() == "(") {
    token_iter = GenerateBypassWhiteSpaces(generator);  // skip the "("
  } else {
    return absl::InvalidArgumentError(
        "Error it is illegal to call a callable macro without ().");
  }

  while (parameters_size > 0) {
    if ((*token_iter)->token_enum() == MacroArg) {
      macro_call->positional_arguments.emplace_back(**token_iter);
      token_iter = GenerateBypassWhiteSpaces(generator);
      if ((*token_iter)->text() == ",") {
        token_iter = GenerateBypassWhiteSpaces(generator);
      }
      parameters_size--;
      continue;
    }
    if ((*token_iter)->text() == ",") {
      macro_call->positional_arguments.emplace_back();  // default token info
      token_iter = GenerateBypassWhiteSpaces(generator);
      parameters_size--;
      continue;
    }
    if ((*token_iter)->text() == ")") {
      break;
    }
  }
  if (parameters_size > 0) {
    while (parameters_size--) {
      macro_call->positional_arguments.emplace_back();  // default token info
    }
  }
  return absl::OkStatus();
}

// Responds to `define directives.  Macro definitions are parsed and saved
// for use within the same file.
absl::Status VerilogPreprocess::HandleMacroIdentifier(
    const TokenStreamView::const_iterator
        iter  // points to `MACROIDENTIFIER token
    ,
    const StreamIteratorGenerator &generator, bool forward = true) {
  // Note: since this function is called we know that config_.expand_macros is
  // true.

  // Finding the macro definition.
  const absl::string_view sv = (*iter)->text();
  const auto *found =
      FindOrNull(preprocess_data_.macro_definitions, sv.substr(1));
  if (!found) {
    preprocess_data_.errors.emplace_back(
        **iter,
        "Error expanding macro identifier, might not be defined before.");
    return absl::InvalidArgumentError(
        "Error expanding macro identifier, might not be defined before.");
  }

  if (config_.expand_macros) {
    verible::MacroCall macro_call;
    RETURN_IF_ERROR(
        ConsumeAndParseMacroCall(iter, generator, &macro_call, *found));
    RETURN_IF_ERROR(ExpandMacro(macro_call, found));
  }
  auto &lexed = preprocess_data_.lexed_macros_backup.back();
  if (!forward) return absl::OkStatus();
  auto iter_generator = verible::MakeConstIteratorStreamer(lexed);
  const auto it_end = lexed.end();
  for (auto it = iter_generator(); it != it_end; it++) {
    preprocess_data_.preprocessed_token_stream.push_back(it);
  }
  return absl::OkStatus();
}

// Stores a macro definition for later use.
void VerilogPreprocess::RegisterMacroDefinition(
    const MacroDefinition &definition) {
  // For now, unconditionally register the macro definition, keeping the last
  // definition if macro is re-defined.
  const bool inserted = InsertOrUpdate(&preprocess_data_.macro_definitions,
                                       definition.Name(), definition);
  if (inserted) return;
  preprocess_data_.warnings.emplace_back(definition.NameToken(),
                                         "Re-defining macro");
  // TODO(hzeller): multiline warning with 'previously defined here' location
}

// This function expands a text.
// The expanded tokens are saved as a TokenSequence, stored at
// preprocess_data_.lexed_macros_backup Can be accessed directly after expansion
// as: preprocess_data_.lexed_macros_backup.back()
absl::Status VerilogPreprocess::ExpandText(
    const absl::string_view &definition_text) {
  VerilogLexer lexer(definition_text);
  verible::TokenSequence lexed_sequence;
  verible::TokenSequence expanded_lexed_sequence;
  // Populating the lexed token sequence.
  for (lexer.DoNextToken(); !lexer.GetLastToken().isEOF();
       lexer.DoNextToken()) {
    lexed_sequence.push_back(lexer.GetLastToken());
  }
  verible::TokenStreamView lexed_streamview;
  // Initializing the lexed token stream view.
  InitTokenStreamView(lexed_sequence, &lexed_streamview);

  auto iter_generator = verible::MakeConstIteratorStreamer(lexed_streamview);
  const auto end = lexed_streamview.end();

  // Token-pulling loop.
  for (auto iter = iter_generator(); iter != end; iter = iter_generator()) {
    auto &last_token = **iter;
    // TODO: handle lexical error
    if (lexer.GetLastToken().token_enum() == TK_SPACE) {
      continue;  // don't forward spaces
    }
    // If the expanded token is another macro identifier that needs to be
    // expanded.
    // TODO: this needs to be something like HandleTokenIterator, to claim that
    // it fully covers all cases.
    if (last_token.token_enum() == MacroIdentifier ||
        last_token.token_enum() == MacroIdItem ||
        last_token.token_enum() == MacroCallId) {
      RETURN_IF_ERROR(HandleMacroIdentifier(iter, iter_generator, false));
      // merge the expanded macro tokens into 'expanded_lexed_sequence'
      auto &expanded_child = preprocess_data_.lexed_macros_backup.back();
      for (auto &u : expanded_child) expanded_lexed_sequence.push_back(u);
      continue;
    }
    expanded_lexed_sequence.push_back(last_token);
  }
  preprocess_data_.lexed_macros_backup.emplace_back(expanded_lexed_sequence);
  return absl::OkStatus();
}

// This method expands a callable macro call, that follows this form:
// `MACRO([param1],[param2],...)
absl::Status VerilogPreprocess::ExpandMacro(
    const verible::MacroCall &macro_call,
    const verible::MacroDefinition *macro_definition) {
  const auto &actual_parameters = macro_call.positional_arguments;

  std::map<absl::string_view, verible::DefaultTokenInfo> subs_map;
  if (macro_definition->IsCallable()) {
    RETURN_IF_ERROR(macro_definition->PopulateSubstitutionMap(actual_parameters,
                                                              &subs_map));
  }

  VerilogLexer lexer(macro_definition->DefinitionText().text());
  verible::TokenSequence lexed_sequence;
  verible::TokenSequence expanded_lexed_sequence;
  // Populating the lexed token sequence.
  for (lexer.DoNextToken(); !lexer.GetLastToken().isEOF();
       lexer.DoNextToken()) {
    lexed_sequence.push_back(lexer.GetLastToken());
  }
  verible::TokenStreamView lexed_streamview;
  // Initializing the lexed token stream view.
  InitTokenStreamView(lexed_sequence, &lexed_streamview);

  auto iter_generator = verible::MakeConstIteratorStreamer(lexed_streamview);
  const auto end = lexed_streamview.end();

  // Token-pulling loop.
  for (auto iter = iter_generator(); iter != end; iter = iter_generator()) {
    // TODO: handle lexical error
    auto &last_token = **iter;
    if (last_token.token_enum() == TK_SPACE) continue;  // don't forward spaces
    // If the expanded token is another macro identifier that needs to be
    // expanded.
    // TODO: this needs to be something like HandleTokenIterator, to claim that
    // it fully covers all cases.
    if (last_token.token_enum() == MacroIdentifier ||
        last_token.token_enum() == MacroIdItem ||
        last_token.token_enum() == MacroCallId) {
      RETURN_IF_ERROR(HandleMacroIdentifier(iter, iter_generator, false));
      // merge the expanded macro tokens into 'expanded_lexed_sequence'
      auto &expanded_child = preprocess_data_.lexed_macros_backup.back();
      for (auto &u : expanded_child) expanded_lexed_sequence.push_back(u);
      continue;
    }
    if (macro_definition->IsCallable()) {
      // Check if the last token is a formal parameter
      const auto *replacement = FindOrNull(subs_map, last_token.text());
      if (replacement) {
        RETURN_IF_ERROR(ExpandText(replacement->text()));
        // merge the expanded macro tokens into 'expanded_lexed_sequence'
        auto &expanded_child = preprocess_data_.lexed_macros_backup.back();
        for (auto &u : expanded_child) expanded_lexed_sequence.push_back(u);
        continue;
      }
    }
    expanded_lexed_sequence.push_back(last_token);
  }
  preprocess_data_.lexed_macros_backup.emplace_back(expanded_lexed_sequence);
  return absl::OkStatus();
}

// Responds to `define directives.  Macro definitions are parsed and saved
// for use within the same file.
absl::Status VerilogPreprocess::HandleDefine(
    const TokenStreamView::const_iterator iter,  // points to `define token
    const StreamIteratorGenerator &generator) {
  TokenStreamView define_tokens;
  define_tokens.push_back(*iter);
  RETURN_IF_ERROR(ConsumeMacroDefinition(generator, &define_tokens));
  CHECK_GE(define_tokens.size(), 3)
      << "Macro definition should span at least 3 tokens, but only got "
      << define_tokens.size();
  const verible::TokenSequence::const_iterator macro_name = define_tokens[1];
  verible::MacroDefinition macro_definition(*define_tokens[0], *macro_name);
  const auto parse_error_ptr =
      ParseMacroDefinition(define_tokens, &macro_definition);

  if (parse_error_ptr) {
    preprocess_data_.errors.push_back(*parse_error_ptr);
    return absl::InvalidArgumentError("Error parsing macro definition.");
  }

  // Parsing showed that things are syntatically correct.
  // But let's only emit things if we're in an active preprocessing branch.
  if (conditional_block_.top().InSelectedBranch()) {
    RegisterMacroDefinition(macro_definition);

    // For now, forward all definition tokens.
    for (const auto &token : define_tokens) {
      preprocess_data_.preprocessed_token_stream.push_back(token);
    }
  }

  return absl::OkStatus();
}

absl::Status VerilogPreprocess::HandleUndef(
    TokenStreamView::const_iterator undef_it,
    const StreamIteratorGenerator &generator) {
  auto macro_name_extract = ExtractMacroName(generator);
  if (!macro_name_extract.ok()) {
    return macro_name_extract.status();
  }
  const auto &macro_name = *macro_name_extract.value();
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
    const StreamIteratorGenerator &generator) {
  if (!config_.filter_branches) {  // nothing to do.
    preprocess_data_.preprocessed_token_stream.push_back(*ifpos);
    return absl::OkStatus();
  }

  auto macro_name_extract = ExtractMacroName(generator);
  if (!macro_name_extract.ok()) {
    return macro_name_extract.status();
  }
  const auto &macro_name = *macro_name_extract.value();
  const bool negative_if = (*ifpos)->token_enum() == PP_ifndef;
  const auto &defs = preprocess_data_.macro_definitions;
  const bool name_is_defined = defs.find(macro_name->text()) != defs.end();
  const bool condition_met = (name_is_defined ^ negative_if);

  if ((*ifpos)->token_enum() == PP_elsif) {
    if (conditional_block_.size() <= 1) {
      preprocess_data_.errors.emplace_back(**ifpos, "Unmatched `elsif");
      return absl::InvalidArgumentError("Unmatched `else");
    }
    if (!conditional_block_.top().UpdateCondition(**ifpos, condition_met)) {
      preprocess_data_.errors.emplace_back(**ifpos, "`elsif after `else");
      preprocess_data_.errors.emplace_back(conditional_block_.top().token(),
                                           "Previous `else started here.");
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
    preprocess_data_.errors.emplace_back(**else_pos, "Unmatched `else");
    return absl::InvalidArgumentError("Unmatched `else");
  }

  if (!conditional_block_.top().StartElse(**else_pos)) {
    preprocess_data_.errors.emplace_back(**else_pos, "Duplicate `else");
    preprocess_data_.errors.emplace_back(conditional_block_.top().token(),
                                         "Previous `else started here.");
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
    preprocess_data_.errors.emplace_back(**endif_pos, "Unmatched `endif");
    return absl::InvalidArgumentError("Unmatched `endif");
  }
  conditional_block_.pop();
  return absl::OkStatus();
}

// Handle `include directives.
// TODO(karimtera):  An important future work would be to utilize
// "VerilogProject::OpenIncludedFile()", which has more advantages over the way
// we open included files in "VerilogPreprocess::HandleInclude()", such as
// avoiding to open the same file multiple times, and have a more clear
// definition of a compilation unit. It could be done, but here are some changes
// that I think need to be done first:
//    1- Add a member "VerilogProject project_" to "VerilogPreprocess".
//    2- Add a constructor to "VerilogPreprocess" to construct "project_"
//    correctly (as a VerilogProject can't be assigned, copied, or moved).
//    3- Modify "VerilogPreprocess::ScanStream()" or replace it with
//    "VerilogPreprocess::ScanProject()", which should scan all
//    "project_.files_" files.

absl::Status VerilogPreprocess::HandleInclude(
    TokenStreamView::const_iterator iter,
    const StreamIteratorGenerator &generator) {
  if (!file_opener_) {
    return absl::FailedPreconditionError("file_opener_ is not defined");
  }
  // TODO(karimtera): Support inclduing <file>,
  // which should look for files defined by language standard in a compiler
  // dependent path.
  TokenStreamView::const_iterator token_iter =
      GenerateBypassWhiteSpaces(generator);
  auto file_token_iter = *token_iter;
  if (file_token_iter->token_enum() != TK_StringLiteral &&
      file_token_iter->token_enum() != TK_AngleBracketInclude) {
    preprocess_data_.errors.emplace_back(**token_iter,
                                         "Expected a path to a SV file.");
    return absl::InvalidArgumentError("Expected a path to a SV file.");
  }
  // Currently the file path looks like "path", we need to remove "" or <>
  const auto &token_text = file_token_iter->text();

  std::filesystem::path file_path =
      std::string(token_text.substr(1, token_text.size() - 2));

  // Use the provided FileOpener to open the included file.
  const auto status_or_file = file_opener_(file_path.string());
  if (!status_or_file.ok()) {
    preprocess_data_.errors.emplace_back(
        **token_iter, std::string(status_or_file.status().message()));
    return status_or_file.status();
  }
  const absl::string_view source_contents = *status_or_file;

  // Creating a new "VerilogPreprocess" object for the included file,
  // With the same configuration and preprocessing info (defines, incdirs) as
  // the main one.
  // TODO(karimtera): Ideally modify the FileOpener to return
  // absl::StatusOr<MemBlock> to avoid doing a second copy inside TextStructure.
  verilog::VerilogPreprocess child_preprocessor(config_, file_opener_);
  child_preprocessor.setPreprocessingInfo(preprocess_info_);

  // TODO(karimtera): limit number of nested includes, detect cycles? maybe.
  preprocess_data_.included_text_structure.emplace_back(
      new verible::TextStructure(source_contents));
  verible::TextStructure &included_structure =
      *preprocess_data_.included_text_structure.back();

  // "included_sequence" should contain the lexed token sequence.
  verible::TokenSequence &included_sequence =
      included_structure.MutableData().MutableTokenStream();

  // Lexing the included file content, and storing it in "included_sequence".
  verilog::VerilogLexer lexer(included_structure.Data().Contents());
  for (lexer.DoNextToken(); !lexer.GetLastToken().isEOF();
       lexer.DoNextToken()) {
    included_sequence.push_back(lexer.GetLastToken());
  }

  // Preprocessing the included file tokens.
  verible::TokenStreamView lexed_streamview;
  InitTokenStreamView(included_sequence, &lexed_streamview);
  verilog::VerilogPreprocessData child_preprocessed_data =
      child_preprocessor.ScanStream(lexed_streamview);

  // Check for errors while preprocessing the included file.
  if (!child_preprocessed_data.errors.empty()) {
    preprocess_data_.errors.insert(preprocess_data_.errors.end(),
                                   child_preprocessed_data.errors.begin(),
                                   child_preprocessed_data.errors.end());
    return absl::InvalidArgumentError(
        "Error: the included file preprocessing has failed.");
  }

  // Need to move the text structures of the child preprocessor to avoid
  // destruction.
  for (auto &u : child_preprocessed_data.included_text_structure) {
    preprocess_data_.included_text_structure.push_back(std::move(u));
  }

  // Forwarding the included preprocessed view.
  for (const auto &u : child_preprocessed_data.preprocessed_token_stream) {
    preprocess_data_.preprocessed_token_stream.push_back(u);
  }

  return absl::OkStatus();
}

// Interprets preprocessor tokens as directives that act on this preprocessor
// object and possibly transform the input token stream.
absl::Status VerilogPreprocess::HandleTokenIterator(
    TokenStreamView::const_iterator iter,
    const StreamIteratorGenerator &generator) {
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
    default:
      break;  // not interested in anything else
  }
  if (config_.expand_macros && ((*iter)->token_enum() == MacroIdentifier ||
                                (*iter)->token_enum() == MacroIdItem ||
                                (*iter)->token_enum() == MacroCallId)) {
    return HandleMacroIdentifier(iter, generator);
  }

  if (config_.include_files && (*iter)->token_enum() == PP_include) {
    return HandleInclude(iter, generator);
  }

  // If not return'ed above, any other tokens are passed through unmodified
  // unless filtered by a branch.
  if (conditional_block_.top().InSelectedBranch()) {
    preprocess_data_.preprocessed_token_stream.push_back(*iter);
  }
  return absl::OkStatus();
}

void VerilogPreprocess::setPreprocessingInfo(
    const verilog::FileList::PreprocessingInfo &preprocess_info) {
  preprocess_info_ = preprocess_info;

  // Adding defines.
  for (const auto &define : preprocess_info_.defines) {
    // manually create the tokens to save them into a MacroDefinition.
    verible::TokenInfo macro_directive(PP_define, "`define");
    verible::TokenInfo macro_name(PP_Identifier, define.name);
    verible::TokenInfo macro_body(PP_define_body, define.value);
    verible::MacroDefinition macro_definition(macro_directive, macro_name);
    macro_definition.SetDefinitionText(macro_body);

    // Registers the macro definition to memeory.
    RegisterMacroDefinition(macro_definition);
  }

  // We can directly access "preprocess_info_.include_dirs" whenever needed.
}

VerilogPreprocessData VerilogPreprocess::ScanStream(
    const TokenStreamView &token_stream) {
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
    preprocess_data_.errors.emplace_back(
        conditional_block_.top().token(),
        "Unterminated preprocessing conditional here, but never completed at "
        "end of file.");
  }
  return std::move(preprocess_data_);
}

}  // namespace verilog
