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

#include "verible/common/text/tree-compare.h"

#include "verible/common/text/symbol.h"
#include "verible/common/text/token-info.h"

namespace verible {

bool EqualTrees(const Symbol *lhs, const Symbol *rhs,
                const TokenComparator &compare_tokens) {
  if (lhs == nullptr && rhs == nullptr) {
    return true;
  }
  if (lhs == nullptr || rhs == nullptr) {
    return false;
  }
  const Symbol *rhs_pointer = rhs;
  return lhs->equals(rhs_pointer, compare_tokens);
}

bool EqualTrees(const Symbol *lhs, const Symbol *rhs) {
  return EqualTrees(lhs, rhs, &TokenInfo::operator==);
}

bool EqualTreesByEnum(const Symbol *lhs, const Symbol *rhs) {
  return EqualTrees(lhs, rhs, EqualByEnum);
}

bool EqualTreesByEnumString(const Symbol *lhs, const Symbol *rhs) {
  return EqualTrees(lhs, rhs, EqualByEnumString);
}

bool EqualByEnum(const TokenInfo &lhs, const TokenInfo &rhs) {
  return lhs.token_enum() == rhs.token_enum();
}

bool EqualByEnumString(const TokenInfo &lhs, const TokenInfo &rhs) {
  return lhs.token_enum() == rhs.token_enum() && lhs.text() == rhs.text();
}

}  // namespace verible
