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

#include "verible/common/analysis/command-file-lexer.h"

#include <initializer_list>
#include <utility>

#include "absl/strings/string_view.h"
#include "gtest/gtest.h"
#include "verible/common/lexer/lexer-test-util.h"
#include "verible/common/text/constants.h"
#include "verible/common/text/token-info-test-util.h"
#include "verible/common/text/token-info.h"

namespace verible {
namespace {

// Removes non-essential tokens from token output stream, such as spaces.
class FilteredCommandFileLexer : public CommandFileLexer {
 public:
  explicit FilteredCommandFileLexer(absl::string_view code)
      : CommandFileLexer(code) {}

  bool KeepSyntaxTreeTokens(const verible::TokenInfo &t) {
    switch (t.token_enum()) {
      case ConfigToken::kNewline:
        return false;
      default:
        return true;
    }
  }

  const verible::TokenInfo &DoNextToken() final {
    do {
      CommandFileLexer::DoNextToken();
    } while (!KeepSyntaxTreeTokens(GetLastToken()));
    return GetLastToken();
  }
};

using LexerTestData = verible::SynthesizedLexerTestData;

// Forwarding function to the template test driver function.
template <typename... Args>
static void TestLexer(Args &&...args) {
  verible::TestLexer<CommandFileLexer>(std::forward<Args>(args)...);
}

// Forwarding function to the template test driver function.
template <typename... Args>
static void TestFilteredLexer(Args &&...args) {
  verible::TestLexer<FilteredCommandFileLexer>(std::forward<Args>(args)...);
}

static const std::initializer_list<LexerTestData> kTokenTests = {
    {{CommandFileLexer::ConfigToken::kComment, "#comment"}, {TK_EOF, "\0"}},
    {{CommandFileLexer::ConfigToken::kCommand, "waive"}, {TK_EOF, "\0"}},
    {{CommandFileLexer::ConfigToken::kCommand, "waive"},
     {ExpectedTokenInfo::kNoToken, " "},
     {CommandFileLexer::ConfigToken::kFlagWithArg, "--rule="},
     {CommandFileLexer::ConfigToken::kArg, "rulename"},
     {TK_EOF, "\0"}},
    {{CommandFileLexer::ConfigToken::kCommand, "waive"},
     {ExpectedTokenInfo::kNoToken, " "},
     {CommandFileLexer::ConfigToken::kParam, "parameter"},
     {TK_EOF, "\0"}},
    {{CommandFileLexer::ConfigToken::kError, "%"}},
};

TEST(CommandLexerTest, Comments) { TestFilteredLexer(kTokenTests); }

}  // namespace
}  // namespace verible
