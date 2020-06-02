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
#include "verilog/parser/verilog_parser.h"  // for verilog_symbol_name()
#include "verilog/parser/verilog_token_enum.h"

namespace verilog {

using verible::TokenGenerator;
using verible::TokenInfo;
using verible::TokenStreamView;
using verible::container::InsertOrUpdate;

// Copies `define token iterators into a temporary buffer.
// Assumes that the last token of a definition is the un-lexed definition body.
// Tokens are copied from the 'generator' into 'define_tokens'.
std::unique_ptr<VerilogPreprocessError>
VerilogPreprocess::ConsumeMacroDefinition(
    const StreamIteratorGenerator& generator, TokenStreamView* define_tokens) {
  // Next token to expect is macro definition name.
  TokenStreamView::const_iterator token_iter = generator();
  if ((*token_iter)->isEOF()) {
    return absl::make_unique<VerilogPreprocessError>(
        **token_iter, "unexpected EOF where expecting macro definition name");
  }
  const auto macro_name = *token_iter;
  if (macro_name->token_enum() != PP_Identifier) {
    return absl::make_unique<VerilogPreprocessError>(
        **token_iter,
        absl::StrCat("Expected identifier for macro name, but got \"",
                     macro_name->text(), "...\""));
  }
  define_tokens->push_back(*token_iter);

  // Everything else covers macro parameters and the definition body.
  do {
    token_iter = generator();
    if ((*token_iter)->isEOF()) {
      // Diagnose unexpected EOF downstream instead of erroring here.
      // Other subroutines can give better context about the parsing state.
      define_tokens->push_back(*token_iter);
      return nullptr;
    }
    define_tokens->push_back(*token_iter);
  } while ((*token_iter)->token_enum() != PP_define_body);
  return nullptr;
}

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

// Interprets preprocessor tokens as directives that act on this preprocessor
// object and possibly transform the input token stream.
absl::Status VerilogPreprocess::HandleTokenIterator(
    const TokenStreamView::const_iterator iter,
    const StreamIteratorGenerator& generator) {
  // For now, pass through all macro definition tokens to next consumer
  // (parser).
  switch ((*iter)->token_enum()) {
    case PP_define:
      return HandleDefine(iter, generator);
    default:
      // All other tokens are passed through unmodified.
      preprocess_data_.preprocessed_token_stream.push_back(*iter);
      return absl::OkStatus();
  }
}

// Stores a macro definition for later use.
void VerilogPreprocess::RegisterMacroDefinition(
    const MacroDefinition& definition) {
  // For now, unconditionally register the macro definition, keeping the last
  // definition if macro is re-defined.
  const bool inserted = InsertOrUpdate(&preprocess_data_.macro_definitions,
                                       definition.Name(), definition);
  if (!inserted) {
    LOG(INFO) << "Re-defining macro " << definition.Name();
  }
  // TODO(fangism): diagnose re-definitions
}

// Responds to `define directives.  Macro definitions are parsed and saved
// for use within the same file.
absl::Status VerilogPreprocess::HandleDefine(
    const TokenStreamView::const_iterator iter,  // points to `define token
    const StreamIteratorGenerator& generator) {
  TokenStreamView define_tokens;
  define_tokens.push_back(*iter);
  const auto consume_error_ptr =
      ConsumeMacroDefinition(generator, &define_tokens);
  if (consume_error_ptr) {
    preprocess_data_.errors.push_back(*consume_error_ptr);
    return absl::InvalidArgumentError("Error parsing macro definition.");
  }
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
  // For now, forward all definition tokens.
  RegisterMacroDefinition(macro_definition);
  for (const auto& token : define_tokens) {
    preprocess_data_.preprocessed_token_stream.push_back(token);
  }
  return absl::OkStatus();
}

VerilogPreprocessData VerilogPreprocess::ScanStream(
    const TokenStreamView& token_stream) {
  preprocess_data_.preprocessed_token_stream.reserve(token_stream.size());
  auto iter_generator = verible::MakeConstIteratorStreamer(token_stream);
  const auto end = token_stream.end();
  auto iter = iter_generator();
  // Token-pulling loop.
  while (iter != end) {
    const auto status = HandleTokenIterator(iter, iter_generator);
    if (!status.ok()) {
      // Detailed errors are already in preprocessor_data_.errors.
      break;  // For now, stop after first error.
    }
    iter = iter_generator();
  }
  return std::move(preprocess_data_);
}

}  // namespace verilog
