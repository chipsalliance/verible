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

// ParserParam class used as a helper during lexing and parsing
// to access actual lexer implementation and static code information
// requestor, and to resize parser stacks when needed.

#ifndef VERIBLE_COMMON_PARSER_PARSER_PARAM_H_
#define VERIBLE_COMMON_PARSER_PARSER_PARAM_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "verible/common/lexer/token-generator.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/token-info.h"

namespace verible {

// This should be the same type as yytype_int16 from yy.tab.cc (yacc/bison).
// Unfortunately, that type isn't exposed in any header, so we have to
// check it in the .yc (yacc) grammar file.
using bison_state_int_type = int16_t;

// TODO(hzeller): Naming. Very generic name; unclear what this class does from
// the name alone (naming derived because it is passed as %param to bison).
class ParserParam {
  using StateStack = std::vector<bison_state_int_type>;
  using ValueStack = std::vector<SymbolPtr>;

 public:
  // The "filename" is merely to have better error messages, it is purely
  // FYI, does not change processing.
  ParserParam(TokenGenerator *token_stream, absl::string_view filename);

  ~ParserParam();

  const TokenInfo &FetchToken();

  const TokenInfo &GetLastToken() const { return last_token_; }

  // Save a copy of the offending token before bison error-recovery
  // discards it.
  void RecordSyntaxError(const SymbolPtr &symbol_ptr);

  // Filename being processed, if known.
  absl::string_view filename() const { return filename_; }

  const std::vector<TokenInfo> &RecoveredSyntaxErrors() const {
    return recovered_syntax_errors_;
  }

  // Resizes parser stacks. All data from the current stacks is copied
  // to the new (larger) ones and stack pointers are updated.
  // All stacks must be of the same size which is updated too.
  // Method updates the *size ptr with the new size.
  //
  // New bison (at least 3.5) define the size type to be ptrdiff_t, while old
  // bisons use size_t. Be compatible with any reasonable long-ish type.
  template <typename SizeType>
  void ResizeStacks(bison_state_int_type **state_stack, SymbolPtr **value_stack,
                    SizeType *size) {
    int64_t s = *size;
    ResizeStacksInternal(state_stack, value_stack, &s);
    *size = s;
  }

  // Returns the maximum allocated size of parser stacks or 0
  // if ResizeStacks() was never called.
  // This is useful to determine a reasonable default parser stack size.
  size_t MaxUsedStackSize() const { return max_used_stack_size_; }

  // Relinquishes ownership of syntax tree.
  ConcreteSyntaxTree TakeRoot() { return std::move(root_); }

  // Takes ownership of syntax tree.
  void SetRoot(ConcreteSyntaxTree r) { root_ = std::move(r); }

 private:
  void ResizeStacksInternal(bison_state_int_type **state_stack,
                            SymbolPtr **value_stack, int64_t *size);

  // Container of syntax-rejected tokens.
  // TODO(fangism): Pair this with recovery token, the point at which
  // error-recovery is complete and parsing resumes (for diagnostic purposes).
  std::vector<TokenInfo> recovered_syntax_errors_;

  TokenGenerator *const token_stream_;
  const std::string filename_;

  TokenInfo last_token_;
  ConcreteSyntaxTree root_;

  // Overflow storage for parser's internal symbol and value stack.
  StateStack state_stack_;
  ValueStack value_stack_;
  size_t max_used_stack_size_;

 public:  // deleted member functions: public.
  ParserParam(const ParserParam &) = delete;
  ParserParam &operator=(const ParserParam &) = delete;
};

}  // namespace verible

#endif  // VERIBLE_COMMON_PARSER_PARSER_PARAM_H_
