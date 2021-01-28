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

// Unit tests for bison_parser_common.

#include "common/parser/bison_parser_common.h"

#include <memory>

#include "absl/strings/string_view.h"
#include "common/lexer/lexer.h"
#include "common/lexer/token_stream_adapter.h"
#include "common/parser/parser_param.h"
#include "common/text/concrete_syntax_leaf.h"
#include "common/text/concrete_syntax_tree.h"
#include "common/text/symbol.h"
#include "common/text/token_info.h"
#include "gtest/gtest.h"

namespace verible {
namespace {

// MockLexer is just for testing, and returns a fixed token.
class MockLexer : public Lexer {
 public:
  MockLexer() : token_(13, "foo") {}

  const TokenInfo& GetLastToken() const override { return token_; }

  const TokenInfo& DoNextToken() override { return token_; }

  void Restart(absl::string_view) override {}

  bool TokenIsError(const TokenInfo&) const override { return false; }

 private:
  TokenInfo token_;
};

// Test that Lex fetches a token of the expected value.
TEST(BisonParserCommonTest, LexTest) {
  MockLexer lexer;
  auto generator = MakeTokenGenerator(&lexer);
  ParserParam parser_param(&generator);
  SymbolPtr value;
  const int token_enum = verible::LexAdapter(&value, &parser_param);
  const TokenInfo& t(parser_param.GetLastToken());
  EXPECT_EQ(13, token_enum);
  EXPECT_EQ(13, t.token_enum());
  EXPECT_EQ(t.text(), "foo");
  EXPECT_EQ(value->Kind(), SymbolKind::kLeaf);
  const auto* value_ptr = down_cast<const SyntaxTreeLeaf*>(value.get());
  ASSERT_NE(value_ptr, nullptr);
  const TokenInfo& tref(value_ptr->get());
  EXPECT_EQ(13, tref.token_enum());
  EXPECT_EQ(tref.text(), "foo");
}

}  // namespace
}  // namespace verible
