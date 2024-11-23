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

#include "verible/common/text/token-stream-view.h"

#include <algorithm>
#include <vector>

#include "absl/strings/string_view.h"
#include "verible/common/text/token-info.h"
#include "verible/common/util/iterator-range.h"

namespace verible {

void InitTokenStreamView(const TokenSequence &tokens, TokenStreamView *view) {
  view->resize(tokens.size());
  auto iter = tokens.begin();
  const auto end = tokens.end();
  auto view_iter = view->begin();
  for (; iter != end; ++iter, ++view_iter) {
    *view_iter = iter;
  }
}

void FilterTokenStreamView(const TokenFilterPredicate &keep,
                           const TokenStreamView &src, TokenStreamView *dest) {
  dest->clear();
  dest->reserve(src.size() / 2);  // Estimate size of filtered result.
  for (const auto &iter : src) {
    if (keep(*iter)) {
      dest->push_back(iter);
    }
  }
}

void FilterTokenStreamViewInPlace(const TokenFilterPredicate &keep,
                                  TokenStreamView *view) {
  TokenStreamView temp;
  FilterTokenStreamView(keep, *view, &temp);
  view->swap(temp);  // old stream view deleted at end-of-scope
}

static bool TokenLocationLess(const TokenSequence::const_iterator &token_iter,
                              const char *offset) {
  return token_iter->text().begin() < offset;
}

TokenViewRange TokenViewRangeSpanningOffsets(const TokenStreamView &view,
                                             absl::string_view range) {
  const auto lower = range.begin();
  const auto upper = range.end();
  const auto left =
      std::lower_bound(view.cbegin(), view.cend(), lower, &TokenLocationLess);
  const auto right =
      std::lower_bound(left, view.cend(), upper, &TokenLocationLess);
  return make_range(left, right);
}

}  // namespace verible
