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

// This header defines constants common to all lexers and parsers.

#ifndef VERIBLE_COMMON_TEXT_CONSTANTS_H_
#define VERIBLE_COMMON_TEXT_CONSTANTS_H_

namespace verible {

// This is the value of EOF token from all Flex-generated lexers.
// This is also the value expected by Bison-generated parsers for $end.
inline constexpr int TK_EOF = 0;

// Language-specific tags for various nonterminals should be nonzero.
enum NodeEnum {
  kUntagged = 0,
};

}  // namespace verible

#endif  // VERIBLE_COMMON_TEXT_CONSTANTS_H_
