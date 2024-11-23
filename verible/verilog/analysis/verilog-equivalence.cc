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

// VerilogAnalyzer implementation (an example)
// Other related analyzers can follow the same structure.

#include "verible/verilog/analysis/verilog-equivalence.h"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <iostream>
#include <iterator>

#include "absl/strings/string_view.h"
#include "verible/common/lexer/token-stream-adapter.h"
#include "verible/common/text/token-info.h"
#include "verible/common/text/token-stream-view.h"
#include "verible/common/util/enum-flags.h"
#include "verible/common/util/logging.h"
#include "verible/verilog/parser/verilog-lexer.h"
#include "verible/verilog/parser/verilog-parser.h"  // for verilog_symbol_name()
#include "verible/verilog/parser/verilog-token-classifications.h"
#include "verible/verilog/parser/verilog-token-enum.h"

namespace verilog {

using verible::TokenInfo;
using verible::TokenSequence;

// TODO(fangism): majority of this code is not Verilog-specific and could
// be factored into a common/analysis library.

static const verible::EnumNameMap<DiffStatus> &DiffStatusStringMap() {
  static const verible::EnumNameMap<DiffStatus> kDiffStatusStringMap({
      {"equivalent", DiffStatus::kEquivalent},
      {"different", DiffStatus::kDifferent},
      {"left-error", DiffStatus::kLeftError},
      {"right-error", DiffStatus::kRightError},
  });
  return kDiffStatusStringMap;
}

std::ostream &operator<<(std::ostream &stream, DiffStatus status) {
  return DiffStatusStringMap().Unparse(status, stream);
}

// Lex a token into smaller substrings/subtokens.
// Lexical errors are reported to errstream.
// Returns true if lexing succeeded, false on error.
static bool LexText(absl::string_view text, TokenSequence *subtokens,
                    std::ostream *errstream) {
  VLOG(1) << __FUNCTION__;
  VerilogLexer lexer(text);
  // Reservation slot to capture first error token, if any.
  auto err_tok = TokenInfo::EOFToken(text);
  // Lex token's text into subtokens.
  const auto status = MakeTokenSequence(
      &lexer, text, subtokens, [&](const TokenInfo &err) { err_tok = err; });
  if (!status.ok()) {
    VLOG(1) << "lex error on: " << err_tok;
    if (errstream != nullptr) {
      (*errstream) << "Error lexing text: " << text << std::endl
                   << "subtoken: " << err_tok << std::endl
                   << status.message() << std::endl;
      // TODO(fangism): print relative offsets
    }
    return false;
  }
  return true;
}

static void VerilogTokenPrinter(const TokenInfo &token, std::ostream &stream) {
  stream << '(' << verilog_symbol_name(token.token_enum()) << ") " << token;
}

static bool ShouldRecursivelyAnalyzeToken(const TokenInfo &token) {
  return IsUnlexed(verilog_tokentype(token.token_enum()));
}

DiffStatus VerilogLexicallyEquivalent(
    absl::string_view left, absl::string_view right,
    const std::function<bool(const verible::TokenInfo &)> &remove_predicate,
    const std::function<bool(const verible::TokenInfo &,
                             const verible::TokenInfo &)> &equal_comparator,
    std::ostream *errstream) {
  // Bind some Verilog-specific parameters.
  return LexicallyEquivalent(
      left, right,
      [=](absl::string_view text, TokenSequence *tokens) {
        return LexText(text, tokens, errstream);
      },
      ShouldRecursivelyAnalyzeToken,  //
      remove_predicate,               //
      equal_comparator,               //
      VerilogTokenPrinter,            //
      errstream);
}

DiffStatus LexicallyEquivalent(
    absl::string_view left_text, absl::string_view right_text,
    const std::function<bool(absl::string_view, TokenSequence *)> &lexer,
    const std::function<bool(const verible::TokenInfo &)> &recursion_predicate,
    const std::function<bool(const verible::TokenInfo &)> &remove_predicate,
    const std::function<bool(const verible::TokenInfo &,
                             const verible::TokenInfo &)> &equal_comparator,
    const std::function<void(const verible::TokenInfo &, std::ostream &)>
        &token_printer,
    std::ostream *errstream) {
  VLOG(2) << __FUNCTION__;
  // Lex texts into token sequences.
  verible::TokenSequence left_tokens, right_tokens;
  {
    const bool left_success = lexer(left_text, &left_tokens);
    if (!left_success) {
      if (errstream != nullptr) {
        *errstream << "Lexical error from left input text." << std::endl;
      }
      return DiffStatus::kLeftError;
    }
    const bool right_success = lexer(right_text, &right_tokens);
    if (!right_success) {
      if (errstream != nullptr) {
        *errstream << "Lexical error from right input text." << std::endl;
      }
      return DiffStatus::kRightError;
    }
  }

  // Filter out ignored tokens from both token sequences.
  verible::TokenStreamView left_filtered, right_filtered;
  verible::InitTokenStreamView(left_tokens, &left_filtered);
  verible::InitTokenStreamView(right_tokens, &right_filtered);
  verible::TokenFilterPredicate keep_predicate = [&](const TokenInfo &t) {
    return !remove_predicate(t);
  };
  verible::FilterTokenStreamViewInPlace(keep_predicate, &left_filtered);
  verible::FilterTokenStreamViewInPlace(keep_predicate, &right_filtered);

  // Compare filtered views, starting with sizes.
  const size_t l_size = left_filtered.size();
  const size_t r_size = right_filtered.size();
  const bool size_match = l_size == r_size;
  if (!size_match) {
    if (errstream != nullptr) {
      *errstream << "Mismatch in token sequence lengths: " << l_size << " vs. "
                 << r_size << std::endl;
    }
  }

  // This comparator is a composition of the non-recursive equal_comparator
  // and self-recursion, depending on recursion_predicate.
  // This comparator must return a bool for use in the <algorithm> library
  // functions, but to surface lexical errors, we must capture the DiffStatus
  // to a local variable.
  DiffStatus diff_status = DiffStatus::kEquivalent;
  auto recursive_comparator = [&](const TokenSequence::const_iterator l,
                                  const TokenSequence::const_iterator r) {
    if (l->token_enum() != r->token_enum() &&
        !((l->token_enum() == verilog_tokentype::MacroCallCloseToEndLine &&
           r->text() == ")") ||
          (r->token_enum() == verilog_tokentype::MacroCallCloseToEndLine &&
           l->text() == ")"))) {
      if (errstream != nullptr) {
        *errstream << "Mismatched token enums.  got: ";
        token_printer(*l, *errstream);
        *errstream << " vs. ";
        token_printer(*r, *errstream);
        *errstream << std::endl;
      }
      return false;
    }
    if (recursion_predicate(*l)) {
      // Recursively lex and compare.
      VLOG(1) << "recursively lex-ing and comparing";
      diff_status = LexicallyEquivalent(
          l->text(), r->text(), lexer, recursion_predicate, remove_predicate,
          equal_comparator, token_printer, errstream);
      return diff_status == DiffStatus::kEquivalent;
      // Note: Any lexical errors in either stream will make this
      // predicate return false.
    }
    return equal_comparator(*l, *r);  // non-recursive comparator
  };

  // Compare element-by-element up to the common length.
  const size_t min_size = std::min(l_size, r_size);
  auto left_filtered_stop_iter = left_filtered.begin() + min_size;

  auto mismatch_pair =
      std::mismatch(left_filtered.begin(), left_filtered_stop_iter,
                    right_filtered.begin(), recursive_comparator);

  // Report lexical errors as higher precedence.
  switch (diff_status) {
    case DiffStatus::kLeftError:
    case DiffStatus::kRightError:
      return diff_status;
    default:
      break;
  }

  // Report differences.
  if (mismatch_pair.first == left_filtered_stop_iter) {
    if (size_match) {
      // Lengths match, and end of both sequences reached without mismatch.
      return DiffStatus::kEquivalent;
    }
    if (l_size < r_size) {
      *errstream << "First excess token in right sequence: "
                 << *right_filtered[min_size] << std::endl;
    } else {  // r_size < l_size
      *errstream << "First excess token in left sequence: "
                 << *left_filtered[min_size] << std::endl;
    }
    return DiffStatus::kDifferent;
  }
  // else there was a mismatch
  if (errstream != nullptr) {
    const size_t mismatch_index =
        std::distance(left_filtered.begin(), mismatch_pair.first);
    const auto &left_token = **mismatch_pair.first;
    const auto &right_token = **mismatch_pair.second;
    *errstream << "First mismatched token [" << mismatch_index << "]: ";
    token_printer(left_token, *errstream);
    *errstream << " vs. ";
    token_printer(right_token, *errstream);
    *errstream << std::endl;
    // TODO(fangism): print human-digestable location information.
  }
  return DiffStatus::kDifferent;
}

DiffStatus FormatEquivalent(absl::string_view left, absl::string_view right,
                            std::ostream *errstream) {
  return VerilogLexicallyEquivalent(
      left, right,
      [](const TokenInfo &t) {
        return IsWhitespace(verilog_tokentype(t.token_enum()));
      },
      [=](const TokenInfo &l, const TokenInfo &r) {
        // MacroCallCloseToEndLine should be considered equivalent to ')', as
        // they are whitespace dependant
        if (((r.token_enum() == verilog_tokentype::MacroCallCloseToEndLine) &&
             (l.text() == ")")) ||
            ((l.token_enum() == verilog_tokentype::MacroCallCloseToEndLine) &&
             (r.text() == ")"))) {
          return true;
        }
        return l.EquivalentWithoutLocation(r);
      },
      errstream);
}

static bool ObfuscationEquivalentTokens(const TokenInfo &l,
                                        const TokenInfo &r) {
  const auto l_vtoken_enum = verilog_tokentype(l.token_enum());
  if (IsIdentifierLike(l_vtoken_enum)) {
    return l.EquivalentBySpace(r);
  }
  return l.EquivalentWithoutLocation(r);
}

DiffStatus ObfuscationEquivalent(absl::string_view left,
                                 absl::string_view right,
                                 std::ostream *errstream) {
  return VerilogLexicallyEquivalent(
      left, right,
      [](const TokenInfo &) {
        // Whitespaces are required to match exactly.
        return false;
      },
      ObfuscationEquivalentTokens, errstream);
}

}  // namespace verilog
