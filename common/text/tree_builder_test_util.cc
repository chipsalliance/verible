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

#include "common/text/tree_builder_test_util.h"

#include "absl/strings/string_view.h"
#include "common/text/symbol.h"

namespace verible {

constexpr absl::string_view kDontCareText("");

SymbolPtr XLeaf(int token_enum) { return Leaf(token_enum, kDontCareText); }

}  // namespace verible
