// Copyright 2017-2019 The Verible Authors.
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

// TODO(fangism): re-organize under common/formatter.

#include "common/formatting/basic_format_style.h"
#include "common/formatting/unwrapped_line.h"

namespace verible {

// SearchLineWraps takes an UnwrappedLine with formatting annotations,
// and a style structure, and returns a new FormattedExcerpt with formatting
// decisions (wraps, spaces) committed.  This minimizes the numeric penalty
// during search to yield an optimal result.
// max_search_states limits the size of the optimization search.
// When the number of states evaluated exceeds this, this will abort by
// returning a greedily formatted result (which can still be rendered)
// that will be marked as !CompletedFormatting().
FormattedExcerpt SearchLineWraps(const UnwrappedLine& uwline,
                                 const BasicFormatStyle& style,
                                 int max_search_states);

// Returns false as soon as calculated line length exceeds maximum, or a token
// that requires a newline is encountered.  If everything fits, then return
// true.
bool FitsOnLine(const UnwrappedLine& uwline, const BasicFormatStyle& style);

}  // namespace verible

#endif  // VERIBLE_COMMON_FORMATTING_LINE_WRAP_SEARCHER_H_
