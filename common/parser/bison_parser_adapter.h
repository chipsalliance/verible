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

// BisonParserAdapter class implements Parser interface consuming tokens from
// a TokenGenerator and calling a Bison-generated parsing function (ParseFunc
// template parameter).  With this design, the parser is not directly tied
// to a particular lexer, so it is easier to transform the token stream
// before feeding it to the parser.
//
// Sample usage:
//     using VerilogParser = BisonParserAdapter<verilog_parse>;

#ifndef VERIBLE_COMMON_PARSER_BISON_PARSER_ADAPTER_H_
#define VERIBLE_COMMON_PARSER_BISON_PARSER_ADAPTER_H_

#include <cstddef>  // for size_t
#include <vector>

#include "absl/status/status.h"
#include "common/lexer/token_generator.h"
#include "common/parser/parse.h"
#include "common/parser/parser_param.h"
#include "common/text/concrete_syntax_tree.h"
#include "common/text/token_info.h"

namespace verible {

// ParseFunc is a yacc/bison generated yyparse() function.
template <int (*ParseFunc)(ParserParam*)>
class BisonParserAdapter : public Parser {
 public:
  explicit BisonParserAdapter(TokenGenerator* token_generator)
      : Parser(), param_(token_generator) {}

  absl::Status Parse() override {
    int result = ParseFunc(&param_);
    // Results of parsing are stored in param_.
    VLOG(3) << "max_used_stack_size : " << MaxUsedStackSize();
    if (result == 0 && param_.RecoveredSyntaxErrors().empty()) {
      return absl::OkStatus();
    } else {
      // We could potentially dump the parser's symbol stack from ParserParam.
      // We could also print information about recovered errors.
      return absl::InvalidArgumentError("Syntax error.");
      // More detailed error information is stored inside param_.
    }
  }

  const TokenInfo& GetLastToken() const override {
    return param_.GetLastToken();
  }

  const std::vector<TokenInfo>& RejectedTokens() const override {
    return param_.RecoveredSyntaxErrors();
  }

  const ConcreteSyntaxTree& Root() const override { return param_.Root(); }

  ConcreteSyntaxTree TakeRoot() override { return param_.TakeRoot(); }

  size_t MaxUsedStackSize() const { return param_.MaxUsedStackSize(); }

 private:
  // Holds the state of the parser stacks, resulting tree, and rejected tokens.
  ParserParam param_;
};

}  // namespace verible

#endif  // VERIBLE_COMMON_PARSER_BISON_PARSER_ADAPTER_H_
