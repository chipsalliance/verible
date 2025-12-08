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

// Parser interface (generic).

#ifndef VERIBLE_COMMON_PARSER_PARSE_H_
#define VERIBLE_COMMON_PARSER_PARSE_H_

#include <vector>

#include "absl/status/status.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/token-info.h"

namespace verible {

// Parser interface to be implemented by concrete language parsers.
class Parser {
 public:
  // Parse a sequence of tokens.
  virtual absl::Status Parse() = 0;

  // Transfer ownership of tree root.
  virtual ConcreteSyntaxTree TakeRoot() = 0;

  // Return the collection of rejected tokens from recovered syntax errors.
  virtual const std::vector<TokenInfo> &RejectedTokens() const = 0;

  virtual ~Parser() = default;

 protected:
  Parser() = default;

 public:                                       // Deleted should be public.
  Parser(const Parser &) = delete;             // disallow copy
  Parser &operator=(const Parser &) = delete;  // disallow assign
};

}  // namespace verible

#endif  // VERIBLE_COMMON_PARSER_PARSE_H_
