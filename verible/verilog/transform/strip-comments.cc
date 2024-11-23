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

#include "verible/verilog/transform/strip-comments.h"

#include <iostream>
#include <vector>

#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "verible/common/strings/comment-utils.h"
#include "verible/common/strings/range.h"
#include "verible/common/text/token-info.h"
#include "verible/common/util/iterator-range.h"
#include "verible/common/util/logging.h"
#include "verible/common/util/spacer.h"
#include "verible/verilog/parser/verilog-lexer.h"
#include "verible/verilog/parser/verilog-parser.h"
#include "verible/verilog/parser/verilog-token-enum.h"

namespace verilog {

using verible::make_string_view_range;
using verible::Spacer;
using verible::StripComment;
using verible::TokenInfo;

// Replace non-newline characters with a single char, like <space>.
// Tabs are considered non-newline characters.
static void ReplaceNonNewlines(absl::string_view text, std::ostream *output,
                               char replacement) {
  if (text.empty()) return;
  const std::vector<absl::string_view> lines(
      absl::StrSplit(text, absl::ByChar('\n')));
  // no newline before first element
  *output << Spacer(lines.front().size(), replacement);
  for (const auto &line : verible::make_range(lines.begin() + 1, lines.end())) {
    *output << '\n' << Spacer(line.size(), replacement);
  }
}

void StripVerilogComments(absl::string_view content, std::ostream *output,
                          char replacement) {
  VLOG(1) << __FUNCTION__;
  verilog::VerilogLexer lexer(content);

  const TokenInfo::Context context(content, [](std::ostream &stream, int e) {
    stream << verilog_symbol_name(e);
  });

  for (;;) {
    const verible::TokenInfo &token(lexer.DoNextToken());
    if (token.isEOF()) break;

    VLOG(2) << "token: " << verible::TokenWithContext{token, context};
    const absl::string_view text = token.text();
    switch (token.token_enum()) {
      case verilog_tokentype::TK_EOL_COMMENT:
        switch (replacement) {
          case '\0':
            // There is always a '\n' that follows, so there is no risk of
            // accidentally fusing tokens by deleting these comments.
            break;
          case ' ':
            // The lexer guarantees the text does not contain '\n'.
            *output << Spacer(text.length());
            break;
          default: {
            // Retain the "//" but erase everything thereafter.
            const absl::string_view body(StripComment(text));
            const absl::string_view head(
                make_string_view_range(text.begin(), body.begin()));
            *output << head << Spacer(body.length(), replacement);
            break;
          }
        }
        break;

      case verilog_tokentype::TK_COMMENT_BLOCK:
        switch (replacement) {
          case '\0':
            // Print one space to prevent accidental token fusion in
            // cases like: "a/**/b".
            *output << ' ';
            break;
          case ' ':
            // Preserve newlines, but replace everything else with space.
            ReplaceNonNewlines(text, output, replacement);
            break;
          default: {
            // Retain the "/*" and "*/" but erase everything in between.
            const absl::string_view body(StripComment(text));
            const absl::string_view head(
                make_string_view_range(text.begin(), body.begin()));
            const absl::string_view tail(
                make_string_view_range(body.end(), text.end()));

            *output << head;
            ReplaceNonNewlines(body, output, replacement);
            *output << tail;
            break;
          }
        }
        break;
      // The following tokens are un-lexed, so they need to be lexed
      // recursively.
      case verilog_tokentype::MacroArg:
      case verilog_tokentype::PP_define_body:
        StripVerilogComments(text, output, replacement);
        break;
      default:
        // Preserve all other text, including lexical error tokens.
        *output << text;
    }  // switch
  }
  VLOG(1) << "end of " << __FUNCTION__;
}

}  // namespace verilog
