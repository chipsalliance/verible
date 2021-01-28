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

#include "common/analysis/command_file_lexer.h"

#include <initializer_list>
#include <utility>

#include "absl/strings/string_view.h"
#include "common/lexer/lexer_test_util.h"
#include "common/text/token_info.h"
#include "gtest/gtest.h"

namespace verible {
namespace {

// Removes non-essential tokens from token output stream, such as spaces.
class FilteredCommandFileLexer : public CommandFileLexer {
 public:
  explicit FilteredCommandFileLexer(absl::string_view code)
      : CommandFileLexer(code) {}

  bool KeepSyntaxTreeTokens(const verible::TokenInfo& t) {
    switch (t.token_enum()) {
      case CFG_TK_NEWLINE:
        return false;
      default:
        return true;
    }
  }

  const verible::TokenInfo& DoNextToken() override {
    do {
      CommandFileLexer::DoNextToken();
    } while (!KeepSyntaxTreeTokens(GetLastToken()));
    return GetLastToken();
  }
};

using LexerTestData = verible::SynthesizedLexerTestData;

// Forwarding function to the template test driver function.
template <typename... Args>
static void TestLexer(Args&&... args) {
  verible::TestLexer<CommandFileLexer>(std::forward<Args>(args)...);
}

// Forwarding function to the template test driver function.
template <typename... Args>
static void TestFilteredLexer(Args&&... args) {
  verible::TestLexer<FilteredCommandFileLexer>(std::forward<Args>(args)...);
}

static const std::initializer_list<LexerTestData> kTokenTests = {
    {{CFG_TK_COMMENT, "#comment"}, {TK_EOF, "\0"}},
    {{CFG_TK_COMMAND, "waive"}, {TK_EOF, "\0"}},
    {{CFG_TK_COMMAND, "waive"},
     {ExpectedTokenInfo::kNoToken, " "},
     {CFG_TK_FLAG_WITH_ARG, "--rule="},
     {CFG_TK_ARG, "rulename"},
     {TK_EOF, "\0"}},
    {{CFG_TK_COMMAND, "waive"},
     {ExpectedTokenInfo::kNoToken, " "},
     {CFG_TK_PARAM, "parameter"},
     {TK_EOF, "\0"}},
    {{CFG_TK_ERROR, "%"}},
};

TEST(CommandLexerTest, Comments) { TestFilteredLexer(kTokenTests); }

}  // namespace
}  // namespace verible
