// Copyright 2017-2023 The Verible Authors.
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

#include "verible/verilog/transform/obfuscate.h"

#include <iostream>
#include <sstream>
#include <string>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "verible/common/strings/obfuscator.h"
#include "verible/common/strings/random.h"
#include "verible/common/text/token-info.h"
#include "verible/common/util/logging.h"
#include "verible/common/util/status-macros.h"
#include "verible/verilog/analysis/verilog-equivalence.h"
#include "verible/verilog/parser/verilog-lexer.h"
#include "verible/verilog/parser/verilog-token-enum.h"

namespace verilog {

using verible::IdentifierObfuscator;

std::string RandomEqualLengthSymbolIdentifier(absl::string_view in) {
  verilog::VerilogLexer lexer("");  // Oracle to check identifier-ness.
  // In rare case we accidentally generate a keyword, try again.
  for (;;) {
    std::string candidate = verible::RandomEqualLengthIdentifier(in);
    lexer.Restart(candidate);
    if (lexer.DoNextToken().token_enum() ==
        verilog_tokentype::SymbolIdentifier) {
      return candidate;
    }
  }
}

// TODO(fangism): single-char identifiers don't need to be obfuscated.
// or use a shuffle/permutation to guarantee collision-free reversibility.

static void ObfuscateVerilogCodeInternal(absl::string_view content,
                                         std::ostream *output,
                                         IdentifierObfuscator *subst) {
  VLOG(1) << __FUNCTION__;
  verilog::VerilogLexer lexer(content);
  for (;;) {
    const verible::TokenInfo &token(lexer.DoNextToken());
    if (token.isEOF()) break;
    switch (token.token_enum()) {
      case verilog_tokentype::SymbolIdentifier:
      case verilog_tokentype::PP_Identifier:
        *output << (*subst)(token.text());
        break;
        // Preserve all $ID calls, including system task/function calls, and VPI
        // calls
      case verilog_tokentype::SystemTFIdentifier:
        *output << token.text();
        break;
        // The following identifier types start with a special character that
        // needs to be preserved.
      case verilog_tokentype::MacroIdentifier:
      case verilog_tokentype::MacroCallId:
      case verilog_tokentype::MacroIdItem:
        // TODO(fangism): verilog_tokentype::EscapedIdentifier
        *output << token.text()[0] << (*subst)(token.text().substr(1));
        break;
      // The following tokens are un-lexed, so they need to be lexed
      // recursively.
      case verilog_tokentype::MacroArg:
      case verilog_tokentype::PP_define_body:
        ObfuscateVerilogCodeInternal(token.text(), output, subst);
        break;
      default:
        // This also covers lexical error tokens.
        *output << token.text();
    }
  }
  VLOG(1) << "end of " << __FUNCTION__;
}

static absl::Status ObfuscationError(absl::string_view message,
                                     absl::string_view original,
                                     absl::string_view encoded) {
  return absl::InternalError(absl::StrCat(message, "\nORIGINAL:\n", original,
                                          "\nENCODED:\n", encoded,
                                          "\n*** Please file a bug. ***\n"));
}

static absl::Status ReversibilityError(absl::string_view original,
                                       absl::string_view encoded,
                                       absl::string_view decoded) {
  return absl::InternalError(absl::StrCat(
      "Internal error: decode(encode) != original\nORIGINAL:\n", original,
      "\nENCODED:\n", encoded, "\nDECODED:\n", decoded,
      // FIXME(fangism): use a diff library to highlight the differences
      "\n*** Please file a bug. ***\n"));
}

// Internal consistency check that decoding restores original text.
static absl::Status VerifyDecoding(absl::string_view original,
                                   absl::string_view encoded,
                                   const verible::Obfuscator &subst) {
  VLOG(1) << __FUNCTION__;
  // Skip if original transformation was already decoding.
  if (subst.is_decoding()) return absl::OkStatus();

  IdentifierObfuscator reverse_subst(RandomEqualLengthSymbolIdentifier);
  reverse_subst.set_decode_mode(true);

  // Copy over mappings.  Verify map reconstruction.
  const auto saved_map = subst.save();
  RETURN_IF_ERROR(reverse_subst.load(saved_map));

  // Decode and compare.
  std::ostringstream decoded_output;
  ObfuscateVerilogCodeInternal(encoded, &decoded_output, &reverse_subst);
  if (original != decoded_output.str()) {
    return ReversibilityError(original, encoded, decoded_output.str());
  }
  return absl::OkStatus();
}

// Verify that obfuscated output is lexically equivalent to original.
static absl::Status VerifyEquivalence(absl::string_view original,
                                      absl::string_view encoded) {
  VLOG(1) << __FUNCTION__;
  std::ostringstream errstream;
  const auto diff_status =
      verilog::ObfuscationEquivalent(original, encoded, &errstream);
  switch (diff_status) {
    case verilog::DiffStatus::kEquivalent:
      break;
    case verilog::DiffStatus::kDifferent:
      return ObfuscationError(
          absl::StrCat("output is not equivalent: ", errstream.str()), original,
          encoded);
    case verilog::DiffStatus::kLeftError:
      return absl::InvalidArgumentError(
          absl::StrCat("Input contains lexical errors:\n", errstream.str()));
    case verilog::DiffStatus::kRightError:
      return ObfuscationError(
          absl::StrCat("output contains lexical errors: ", errstream.str()),
          original, encoded);
  }
  return absl::OkStatus();
}

absl::Status ObfuscateVerilogCode(absl::string_view content,
                                  std::ostream *output,
                                  IdentifierObfuscator *subst) {
  VLOG(1) << __FUNCTION__;
  std::ostringstream buffer;
  ObfuscateVerilogCodeInternal(content, &buffer, subst);

  // Always verify equivalence.
  RETURN_IF_ERROR(VerifyEquivalence(content, buffer.str()));

  // Always verify decoding.
  RETURN_IF_ERROR(VerifyDecoding(content, buffer.str(), *subst));

  *output << buffer.str();
  return absl::OkStatus();
}

}  // namespace verilog
