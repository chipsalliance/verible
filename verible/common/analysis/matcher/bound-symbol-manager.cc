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

#include "verible/common/analysis/matcher/bound-symbol-manager.h"

#include <map>
#include <string>

#include "verible/common/text/symbol.h"
#include "verible/common/util/container-util.h"
#include "verible/common/util/logging.h"

using verible::container::FindOrNull;

namespace verible {
namespace matcher {

bool BoundSymbolManager::ContainsSymbol(const std::string &id) const {
  return bound_symbols_.find(id) != bound_symbols_.end();
}

const Symbol *BoundSymbolManager::FindSymbol(const std::string &id) const {
  auto *result = FindOrNull(bound_symbols_, id);
  return result ? *result : nullptr;
}

void BoundSymbolManager::BindSymbol(const std::string &id,
                                    const Symbol *symbol) {
  bound_symbols_[id] = ABSL_DIE_IF_NULL(symbol);
}

}  // namespace matcher
}  // namespace verible
