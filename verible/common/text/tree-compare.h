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

// Contains a suite of functions for comparing to SyntaxTrees

#ifndef VERIBLE_COMMON_TEXT_TREE_COMPARE_H_
#define VERIBLE_COMMON_TEXT_TREE_COMPARE_H_

#include <functional>

#include "verible/common/text/symbol.h"
#include "verible/common/text/token-info.h"

namespace verible {

using TokenComparator =
    std::function<bool(const TokenInfo &, const TokenInfo &)>;

// Compares two SyntaxTrees. Two trees are equal if they have the same
// structure and every TokenInfo is equal under compare_tokens.
bool EqualTrees(const Symbol *lhs, const Symbol *rhs,
                const TokenComparator &compare_tokens);
// Compare two syntax trees exactly, using TokenInfo equality.
bool EqualTrees(const Symbol *lhs, const Symbol *rhs);

// Compares two tree using the EqualTrees with the EqualByEnum function
bool EqualTreesByEnum(const Symbol *lhs, const Symbol *rhs);

// compare two trees using EqualTrees with the EqualByEnumString function
bool EqualTreesByEnumString(const Symbol *lhs, const Symbol *rhs);

// Compare two TokenInfo by their enum
bool EqualByEnum(const TokenInfo &lhs, const TokenInfo &rhs);

// Compare two TokenInfo by both enum and text
bool EqualByEnumString(const TokenInfo &lhs, const TokenInfo &rhs);

}  // namespace verible

#endif  // VERIBLE_COMMON_TEXT_TREE_COMPARE_H_
