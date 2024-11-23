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

#ifndef VERIBLE_COMMON_ANALYSIS_MATCHER_DESCENT_PATH_H_
#define VERIBLE_COMMON_ANALYSIS_MATCHER_DESCENT_PATH_H_

#include <vector>

#include "verible/common/text/symbol.h"

namespace verible {
namespace matcher {

using DescentPath = std::vector<SymbolTag>;

// Returns a vector of all descendants of symbol that are precisely along
// path.
// Path starts at symbol's children. Reported descendants match last element
// in path.
//
// Note that this does a recursive branching descent. Every descendant that
// that is found along path is added. This can potentially traverse symbol's
// entire subtree and return a very large vector if symbol's subtree contains
// path in many different ways.
//
std::vector<const Symbol *> GetAllDescendantsFromPath(const Symbol &symbol,
                                                      const DescentPath &path);

}  // namespace matcher
}  // namespace verible

#endif  // VERIBLE_COMMON_ANALYSIS_MATCHER_DESCENT_PATH_H_
