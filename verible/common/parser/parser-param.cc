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

// ParserParam implementation.

#include "verible/common/parser/parser-param.h"

#include <cstdint>
#include <functional>
#include <string_view>
#include <utility>
#include <vector>

#include "verible/common/lexer/token-generator.h"
#include "verible/common/text/concrete-syntax-leaf.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/token-info.h"
#include "verible/common/util/casts.h"
#include "verible/common/util/logging.h"

namespace verible {

ParserParam::ParserParam(TokenGenerator *token_stream,
                         std::string_view filename)
    : token_stream_(token_stream),
      filename_(filename),
      last_token_(TokenInfo::EOFToken()),
      max_used_stack_size_(0) {}

ParserParam::~ParserParam() = default;

const TokenInfo &ParserParam::FetchToken() {
  last_token_ = (*token_stream_)();
  return last_token_;
}

void ParserParam::RecordSyntaxError(const SymbolPtr &symbol_ptr) {
  const auto *leaf = down_cast<const SyntaxTreeLeaf *>(symbol_ptr.get());
  const auto token = leaf->get();
  VLOG(1) << filename_ << ": recovered syntax error: " << token;
  recovered_syntax_errors_.push_back(token);
}

template <typename T>
static void move_stack(T **raw_stack, const int64_t *size,
                       std::vector<T> *stack) {
  stack->resize(*size);
  std::move(*raw_stack, *raw_stack + *size, stack->begin());
}

// See bison_parser_common.h for use of this (yyoverflow).
void ParserParam::ResizeStacksInternal(bison_state_int_type **state_stack,
                                       SymbolPtr **value_stack, int64_t *size) {
  if (state_stack_.empty()) {
    // This is the first reallocation case.
    move_stack(state_stack, size, &state_stack_);
    move_stack(value_stack, size, &value_stack_);
  }
  (*size) *= 2;
  state_stack_.resize(*size);
  value_stack_.resize(*size);
  *state_stack = state_stack_.data();
  *value_stack = value_stack_.data();
  max_used_stack_size_ = *size;
}

}  // namespace verible
