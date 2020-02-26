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

#include "verilog/analysis/verilog_equivalence.h"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "common/lexer/token_stream_adapter.h"
#include "common/text/token_info.h"
#include "common/text/token_stream_view.h"
#include "common/util/logging.h"
#include "verilog/parser/verilog_lexer.h"
#include "verilog/parser/verilog_parser.h"  // for verilog_symbol_name()
#include "verilog/parser/verilog_token_classifications.h"
#include "verilog/parser/verilog_token_enum.h"

namespace verilog {

using verible::TokenInfo;
using verible::TokenSequence;

// TODO(fangism): majority of this code is not Verilog-specific and could
// be factored into a common/analysis library.

// Lex a token into smaller substrings/subtokens.
// Lexical errors are reported to errstream.
static TokenSequence LexText(absl::string_view text, const char* label,
                             std::ostream* errstream) {
  TokenSequence subtokens;
  VerilogLexer lexer(text);
  // Reservation slot to capture first error token, if any.
  auto err_tok = TokenInfo::EOFToken(text);
  // Lex token's text into subtokens.
  const auto status = MakeTokenSequence(
      &lexer, text, &subtokens, [&](const TokenInfo& err) { err_tok = err; });
  if (!status.ok() && (errstream != nullptr)) {
    (*errstream) << "Error lexing " << label << " text: " << text << std::endl
                 << "subtoken: " << err_tok << std::endl
                 << status.message();
  }
  return subtokens;
}

// Recursively lex a token into smaller substrings/subtokens.
// The comparator function itself can invoke another lex-and-compare-like
// function.
static bool RecursivelyLexAndCompare(
    absl::string_view left, absl::string_view right, std::ostream* errstream,
    std::function<bool(absl::string_view, absl::string_view, std::ostream*)>
        comp) {
  return comp(left, right, errstream);
}

// Compares two (already lexed) token sequences one token at a time,
// stopping an report the first mismatch.
static bool LexicallyEquivalentTokens(
    const TokenSequence& left, const TokenSequence& right,
    std::function<bool(const verible::TokenInfo&)> remove_predicate,
    std::function<bool(const verible::TokenInfo&, const verible::TokenInfo&)>
        equal_comparator,
    std::ostream* errstream) {
  // Filter out whitespaces from both token sequences.
  verible::TokenStreamView left_filtered, right_filtered;
  verible::InitTokenStreamView(left, &left_filtered);
  verible::InitTokenStreamView(right, &right_filtered);
  verible::TokenFilterPredicate keep_predicate = [&](const TokenInfo& t) {
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

  // Compare element-by-element up to the common length.
  const size_t min_size = std::min(l_size, r_size);
  auto left_filtered_stop_iter = left_filtered.begin() + min_size;
  auto mismatch_pair = std::mismatch(
      left_filtered.begin(), left_filtered_stop_iter, right_filtered.begin(),
      [&](const TokenSequence::const_iterator l,
          const TokenSequence::const_iterator r) {
        // Ignore location/offset differences.
        return equal_comparator(*l, *r);
      });
  if (mismatch_pair.first == left_filtered_stop_iter) {
    if (size_match) {
      // Lengths match, and end of both sequences reached without mismatch.
      return true;
    } else if (l_size < r_size) {
      *errstream << "First excess token in right sequence: "
                 << *right_filtered[min_size] << std::endl;
    } else {  // r_size < l_size
      *errstream << "First excess token in left sequence: "
                 << *left_filtered[min_size] << std::endl;
    }
    return false;
  }
  // else there was a mismatch
  if (errstream != nullptr) {
    const size_t mismatch_index =
        std::distance(left_filtered.begin(), mismatch_pair.first);
    const auto& left_token = **mismatch_pair.first;
    const auto& right_token = **mismatch_pair.second;
    *errstream << "First mismatched token [" << mismatch_index << "]: ("
               << verilog_symbol_name(left_token.token_enum) << ") "
               << left_token << " vs. ("
               << verilog_symbol_name(right_token.token_enum) << ") "
               << right_token << std::endl;
  }
  return false;
}

// This variant expects two strings and lexes both into corresponding
// token streams to do the comparison.
bool LexicallyEquivalent(
    absl::string_view left, absl::string_view right,
    std::function<bool(const verible::TokenInfo&)> remove_predicate,
    std::function<bool(const verible::TokenInfo&, const verible::TokenInfo&)>
        equal_comparator,
    std::ostream* errstream) {
  const TokenSequence left_subtokens = LexText(left, "left", errstream);
  const TokenSequence right_subtokens = LexText(right, "right", errstream);
  return LexicallyEquivalentTokens(left_subtokens, right_subtokens,
                                   remove_predicate, equal_comparator,
                                   errstream);
}

// Token comparator for format-equivalence.
static bool FormatEquivalentTokens(const TokenInfo& l, const TokenInfo& r,
                                   std::ostream* errstream) {
  if (IsUnlexed(verilog_tokentype(l.token_enum))) {
    return RecursivelyLexAndCompare(l.text, r.text, errstream,
                                    FormatEquivalent);
  }
  return l.EquivalentWithoutLocation(r);
}

bool FormatEquivalent(absl::string_view left, absl::string_view right,
                      std::ostream* errstream) {
  return LexicallyEquivalent(
      left, right,
      [](const TokenInfo& t) {
        return IsWhitespace(verilog_tokentype(t.token_enum));
      },
      [=](const TokenInfo& l, const TokenInfo& r) {
        return FormatEquivalentTokens(l, r, errstream);
      },
      errstream);
}

// Token comparator for obfuscation-equivalence.
static bool ObfuscationEquivalentTokens(const TokenInfo& l, const TokenInfo& r,
                                        std::ostream* errstream) {
  const auto l_vtoken_enum = verilog_tokentype(l.token_enum);
  if (l.token_enum != r.token_enum) {
    if (errstream != nullptr) {
      (*errstream) << "Mismatched token enums.  got: "
                   << verilog_symbol_name(l.token_enum) << " vs. "
                   << verilog_symbol_name(r.token_enum) << std::endl;
    }
    return false;
  }
  if (IsIdentifierLike(l_vtoken_enum)) {
    return l.EquivalentBySpace(r);
  } else if (IsUnlexed(l_vtoken_enum)) {
    return RecursivelyLexAndCompare(l.text, r.text, errstream,
                                    ObfuscationEquivalent);
  }
  return l.EquivalentWithoutLocation(r);
}

bool ObfuscationEquivalent(absl::string_view left, absl::string_view right,
                           std::ostream* errstream) {
  return LexicallyEquivalent(
      left, right,
      [](const TokenInfo&) {
        // Whitespaces are required to match exactly.
        return false;
      },
      [=](const TokenInfo& l, const TokenInfo& r) {
        return ObfuscationEquivalentTokens(l, r, errstream);
      },
      errstream);
}

}  // namespace verilog
