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

#ifndef VERIBLE_COMMON_FORMATTING_LINE_WRAP_SEARCHER_H_
#define VERIBLE_COMMON_FORMATTING_LINE_WRAP_SEARCHER_H_

#include <iosfwd>
#include <vector>

#include "verible/common/formatting/basic-format-style.h"
#include "verible/common/formatting/unwrapped-line.h"

namespace verible {

// SearchLineWraps takes an UnwrappedLine with formatting annotations,
// and a style structure, and returns equally-good FormattedExcerpts with
// formatting decisions (wraps, spaces) committed.
// This minimizes the numeric penalty during search to yield optimal results,
// which can result in multiple optimal formattings.
// max_search_states limits the size of the optimization search.
// When the number of states evaluated exceeds this, this will abort by
// returning a greedily formatted result (which can still be rendered)
// that will be marked as !CompletedFormatting().
// This is guaranteed to return at least one result.
std::vector<FormattedExcerpt> SearchLineWraps(const UnwrappedLine &uwline,
                                              const BasicFormatStyle &style,
                                              int max_search_states);

// Diagnostic helper for displaying when multiple optimal wrappings are found
// by SearchLineWraps.  This aids in development around wrap penalty tuning.
void DisplayEquallyOptimalWrappings(
    std::ostream &stream, const UnwrappedLine &uwline,
    const std::vector<FormattedExcerpt> &solutions);

// Returns false as soon as calculated line length exceeds maximum, or a token
// that requires a newline is encountered.  If everything fits, then return
// true. Beside fitting result function returns final column value.
struct FitResult {
  bool fits;
  int final_column;
};

FitResult FitsOnLine(const UnwrappedLine &uwline,
                     const BasicFormatStyle &style);

}  // namespace verible

#endif  // VERIBLE_COMMON_FORMATTING_LINE_WRAP_SEARCHER_H_
