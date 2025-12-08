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

// Common defines and tiny helper functions for Bison-based parsers.

#include "verible/common/parser/bison-parser-common.h"

#include "verible/common/parser/parser-param.h"
#include "verible/common/text/concrete-syntax-leaf.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/token-info.h"
#include "verible/common/util/logging.h"  // for operator<<, etc

namespace verible {

// Lexer interface function, called by Bison-generated parser to get a next
// token. This no longer calls yylex(), but insteads pulls a token from a token
// stream. 'value' points to yylval in yyparse(), which can be accessed as $1,
// $2, ... in the yacc grammar semantic actions.
int LexAdapter(SymbolPtr *value, ParserParam *param) {
  const auto &last_token = param->FetchToken();
  value->reset(new SyntaxTreeLeaf(last_token));
  return last_token.token_enum();
}

// Error-reporting function.
// Called by Bison-generated parser when a recognition error occurs.
void ParseError(const ParserParam *param, const char *function_name,
                const char *message) {
  VLOG(1) << param->filename() << ": " << function_name
          << " error: " << message;
  // Bison's default and 'verbose' error messages are uninformative.
  // TODO(fangism): print information about rejected token,
  //   by examining parser stacks (have to pass in stack information).
}

}  // namespace verible
