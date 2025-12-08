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

// TokenGenerator is a type alias to a TokenInfo generator function.

#ifndef VERIBLE_COMMON_LEXER_TOKEN_GENERATOR_H_
#define VERIBLE_COMMON_LEXER_TOKEN_GENERATOR_H_

#include <functional>

#include "verible/common/text/token-info.h"

namespace verible {

// TokenGenerator is any callable that returns one TokenInfo at a time.
// TODO(fangism): can we return const-reference?
using TokenGenerator = std::function<TokenInfo()>;

}  // namespace verible

#endif  // VERIBLE_COMMON_LEXER_TOKEN_GENERATOR_H_
