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

#ifndef VERIBLE_COMMON_TEXT_TEXT_STRUCTURE_TEST_UTILS_H_
#define VERIBLE_COMMON_TEXT_TEXT_STRUCTURE_TEST_UTILS_H_

#include <memory>
#include <string>
#include <vector>

#include "verible/common/text/text-structure.h"
#include "verible/common/text/token-info.h"

namespace verible {

using LinesOfTokens = std::vector<std::vector<TokenInfo>>;

// Joins the text fields of TokenInfos into a newly allocated string.
// All other fields of TokenInfos are ignored.
// Each lines of tokens must end with a newline "\n" token.
std::string JoinLinesOfTokensIntoString(const LinesOfTokens &);

// Helper class for constructing a pre-tokenized text structure.
// This avoids depending on any lexer for testing.
class TextStructureTokenized : public TextStructure {
 public:
  // Each line of tokens
  explicit TextStructureTokenized(const LinesOfTokens &lines_of_tokens);
};

// Return a text structure view of a "hello, world" string
std::unique_ptr<TextStructureView> MakeTextStructureViewHelloWorld();

// Return a text structure view with an empty string, and no tokens or tree
std::unique_ptr<TextStructureView> MakeTextStructureViewWithNoLeaves();

}  // namespace verible

#endif  // VERIBLE_COMMON_TEXT_TEXT_STRUCTURE_TEST_UTILS_H_
