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

// TokenStreamView is the interface to parsers.

#ifndef VERIBLE_COMMON_TEXT_TOKEN_STREAM_VIEW_H_
#define VERIBLE_COMMON_TEXT_TOKEN_STREAM_VIEW_H_

#include <functional>
#include <vector>

#include "absl/strings/string_view.h"
#include "verible/common/text/token-info.h"
#include "verible/common/util/iterator-range.h"

namespace verible {

// TODO(fangism): If we make this std::list, then we never have to worry
// about iterator invalidation due to insertion and deletion.
// Evaluate this decision later.
using TokenSequence = std::vector<TokenInfo>;

// Slice of tokens that works like a container.
using TokenRange = iterator_range<TokenSequence::const_iterator>;

// TokenStreamView is the type that is transformed and returned by filters.
using TokenStreamView = std::vector<TokenSequence::const_iterator>;

// TokenStreamReferenceView is TokenStreamView with writeable iterators.
// This should only be used in functions that intend to transform a token stream
// such as when applying context to alter token symbols.
using TokenStreamReferenceView = std::vector<TokenSequence::iterator>;

// Slice of TokenSequence iterators that works like a container
using TokenViewRange = iterator_range<TokenStreamView::const_iterator>;

// Tokens that evaluate to false with these predicates are removed.
using TokenFilterPredicate = std::function<bool(const TokenInfo &)>;

// Populates a TokenStreamView with every iterator of a TokenSequence.
void InitTokenStreamView(const TokenSequence &, TokenStreamView *);

// Create a new TokenStreamView with tokens conditionally omitted.
void FilterTokenStreamView(const TokenFilterPredicate &keep,
                           const TokenStreamView &src, TokenStreamView *dest);

// Remove tokens from a TokenStreamView according to a predicate.
void FilterTokenStreamViewInPlace(const TokenFilterPredicate &keep,
                                  TokenStreamView *);

// Returns iterator range of TokenSequence iterators that span the given file
// offsets. The second iterator points 1-past-the-end of the range.
TokenViewRange TokenViewRangeSpanningOffsets(const TokenStreamView &view,
                                             absl::string_view range);

}  // namespace verible

#endif  // VERIBLE_COMMON_TEXT_TOKEN_STREAM_VIEW_H_
