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

#ifndef VERIBLE_CONFIG_FILE_LEXER_H_
#define VERIBLE_CONFIG_FILE_LEXER_H_

// lint_waiver_config.lex has "%prefix=verible", meaning the class flex
// creates is veribleFlexLexer. Unfortunately, FlexLexer.h doesn't have proper
// ifdefs around its inclusion, so we have to put a bar around it here.
#include "common/analysis/lint_waiver.h"
#include "common/lexer/flex_lexer_adapter.h"
#include "common/text/token_info.h"

// clang-format off
#ifndef _COMMANDFILE_FLEXLEXER_H_
#  undef yyFlexLexer  // this is how FlexLexer.h says to do things
#  define yyFlexLexer veribleCommandFileFlexLexer
#  include <FlexLexer.h>
#endif
// clang-format on

#include "absl/strings/string_view.h"

namespace verible {

class CommandFileLexer : public FlexLexerAdapter<veribleCommandFileFlexLexer> {
  using parent_lexer_type = FlexLexerAdapter<veribleCommandFileFlexLexer>;
  using parent_lexer_type::Restart;

 public:
  // Acceptable syntax:
  //
  // kCommand [--kFlag] [--kFlagWithArg=kArg] [kParam]
  enum ConfigToken {
    kCommand = 1,
    kFlag,
    kFlagWithArg,
    kArg,
    kParam,
    kNewline,
    kComment,
    kError,
  };

  explicit CommandFileLexer(absl::string_view config);

  // Returns true if token is invalid.
  bool TokenIsError(const verible::TokenInfo&) const final;

  // Runs the Lexer and attached command handlers
  std::vector<TokenRange> GetCommandsTokenRanges();

 private:
  // Main lexing function. Will be defined by Flex.
  int yylex() final;

  TokenSequence tokens_;
};

}  // namespace verible

#endif  // VERIBLE_CONFIG_FILE_LEXER_H_
