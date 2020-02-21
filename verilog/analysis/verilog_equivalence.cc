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
#include "common/text/token_info.h"
#include "common/text/token_stream_view.h"
#include "common/util/logging.h"
#include "verilog/parser/verilog_lexer.h"
#include "verilog/parser/verilog_parser.h"
#include "verilog/parser/verilog_token_classifications.h"
#include "verilog/parser/verilog_token_enum.h"

namespace verilog {

using verible::TokenInfo;
using verible::TokenSequence;

bool LexicallyEquivalent(
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

bool FormatEquivalent(const TokenSequence& left, const TokenSequence& right,
                      std::ostream* errstream) {
  return LexicallyEquivalent(
      left, right,
      [](const TokenInfo& t) {
        return IsWhitespace(verilog_tokentype(t.token_enum));
      },
      [](const TokenInfo& l, const TokenInfo& r) {
        return l.EquivalentWithoutLocation(r);
      },
      errstream);
}

}  // namespace verilog
