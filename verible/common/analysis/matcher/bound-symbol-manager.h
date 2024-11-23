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

#ifndef VERIBLE_COMMON_ANALYSIS_MATCHER_BOUND_SYMBOL_MANAGER_H_
#define VERIBLE_COMMON_ANALYSIS_MATCHER_BOUND_SYMBOL_MANAGER_H_

#include <map>
#include <string>

#include "verible/common/text/symbol.h"
#include "verible/common/util/casts.h"

namespace verible {
namespace matcher {

// Manages sets of Bound Symbols created when matching against a syntax tree.
// Currently is just a simply wrapper around std::map
//
// TODO(jeremycs): evolve this to allow easier backtracking from
//   partial/pending matches, that can occur with operators like AllOf().
//
// See BoundNodesTreeBuilder and BoundNodesMap in
// ASTMatchersInternal.h for Clang's equivalents to this class.
//
class BoundSymbolManager {
 public:
  // True if id is in bound_symbols. False otherwise.
  bool ContainsSymbol(const std::string &id) const;

  // If id is in bound_symbols, return matching Symbol*.
  // Otherwise, returns nullptr.
  const Symbol *FindSymbol(const std::string &id) const;

  // Adds symbol to bound_symbols with id as key.
  void BindSymbol(const std::string &id, const Symbol *symbol);

  void Clear() { bound_symbols_.clear(); }
  int Size() const { return bound_symbols_.size(); }

  const std::map<std::string, const Symbol *> &GetBoundMap() const {
    return bound_symbols_;
  }

  template <typename T>
  const T *GetAs(const std::string &key) const {
    return down_cast<const T *>(FindSymbol(key));
  }

 private:
  std::map<std::string, const Symbol *> bound_symbols_;
};

}  // namespace matcher
}  // namespace verible

#endif  // VERIBLE_COMMON_ANALYSIS_MATCHER_BOUND_SYMBOL_MANAGER_H_
