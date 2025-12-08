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

#include "verible/verilog/parser/verilog-token-classifications.h"

#include "gtest/gtest.h"
#include "verible/verilog/parser/verilog-token-enum.h"

namespace verilog {
namespace {

TEST(VerilogTokenTest, IsWhitespaceTest) {
  EXPECT_TRUE(IsWhitespace(verilog_tokentype::TK_NEWLINE));
  EXPECT_TRUE(IsWhitespace(verilog_tokentype::TK_SPACE));
  EXPECT_FALSE(IsWhitespace(verilog_tokentype::TK_class));
  EXPECT_FALSE(IsWhitespace(verilog_tokentype::SymbolIdentifier));
}

// Given a verilog_tokentype, test that IsComment returns true only for comments
TEST(VerilogTokenTest, IsCommentTest) {
  EXPECT_TRUE(IsComment(verilog_tokentype::TK_COMMENT_BLOCK));
  EXPECT_TRUE(IsComment(verilog_tokentype::TK_EOL_COMMENT));
  EXPECT_FALSE(IsComment(verilog_tokentype::DR_begin_keywords));
  EXPECT_FALSE(IsComment(verilog_tokentype::SymbolIdentifier));
}

TEST(VerilogTokenTest, IsUnaryOperatorTest) {
  EXPECT_FALSE(IsUnaryOperator(verilog_tokentype('*')));
  EXPECT_FALSE(IsUnaryOperator(verilog_tokentype('/')));
  EXPECT_TRUE(IsUnaryOperator(verilog_tokentype('+')));
  EXPECT_TRUE(IsUnaryOperator(verilog_tokentype('-')));
  EXPECT_TRUE(IsUnaryOperator(verilog_tokentype('~')));
  EXPECT_TRUE(IsUnaryOperator(verilog_tokentype('&')));
  EXPECT_TRUE(IsUnaryOperator(verilog_tokentype('!')));
  EXPECT_TRUE(IsUnaryOperator(verilog_tokentype('|')));
  EXPECT_TRUE(IsUnaryOperator(verilog_tokentype('^')));
  EXPECT_TRUE(IsUnaryOperator(verilog_tokentype::TK_NAND));
  EXPECT_TRUE(IsUnaryOperator(verilog_tokentype::TK_NOR));
  EXPECT_TRUE(IsUnaryOperator(verilog_tokentype::TK_NXOR));
  EXPECT_TRUE(IsUnaryOperator(verilog_tokentype::TK_INCR));
  EXPECT_TRUE(IsUnaryOperator(verilog_tokentype::TK_DECR));
  EXPECT_FALSE(IsUnaryOperator(verilog_tokentype(':')));
  EXPECT_FALSE(IsUnaryOperator(verilog_tokentype('?')));
  EXPECT_FALSE(IsUnaryOperator(verilog_tokentype(',')));
  EXPECT_FALSE(IsUnaryOperator(verilog_tokentype('.')));
  EXPECT_FALSE(IsUnaryOperator(verilog_tokentype(';')));
  EXPECT_FALSE(IsUnaryOperator(verilog_tokentype('#')));
}

TEST(VerilogTokenTest, IsAssociativeOperatorTest) {
  EXPECT_TRUE(IsAssociativeOperator(verilog_tokentype('*')));
  EXPECT_FALSE(IsAssociativeOperator(verilog_tokentype('/')));
  EXPECT_FALSE(IsAssociativeOperator(verilog_tokentype('%')));
  EXPECT_TRUE(IsAssociativeOperator(verilog_tokentype('+')));
  EXPECT_FALSE(IsAssociativeOperator(verilog_tokentype('-')));
  EXPECT_FALSE(IsAssociativeOperator(verilog_tokentype('~')));
  EXPECT_TRUE(IsAssociativeOperator(verilog_tokentype('&')));
  EXPECT_FALSE(IsAssociativeOperator(verilog_tokentype('!')));
  EXPECT_TRUE(IsAssociativeOperator(verilog_tokentype('|')));
  EXPECT_TRUE(IsAssociativeOperator(verilog_tokentype('^')));
  EXPECT_TRUE(IsAssociativeOperator(verilog_tokentype::TK_LAND));
  EXPECT_TRUE(IsAssociativeOperator(verilog_tokentype::TK_LOR));
  EXPECT_FALSE(IsAssociativeOperator(verilog_tokentype::TK_NAND));
  EXPECT_FALSE(IsAssociativeOperator(verilog_tokentype::TK_NOR));
  EXPECT_TRUE(IsAssociativeOperator(verilog_tokentype::TK_NXOR));
  EXPECT_FALSE(IsAssociativeOperator(verilog_tokentype::TK_INCR));
  EXPECT_FALSE(IsAssociativeOperator(verilog_tokentype::TK_DECR));
  EXPECT_FALSE(IsAssociativeOperator(verilog_tokentype(':')));
  EXPECT_FALSE(IsAssociativeOperator(verilog_tokentype('?')));
  EXPECT_FALSE(IsAssociativeOperator(verilog_tokentype(',')));
  EXPECT_FALSE(IsAssociativeOperator(verilog_tokentype('.')));
  EXPECT_FALSE(IsAssociativeOperator(verilog_tokentype(';')));
  EXPECT_FALSE(IsAssociativeOperator(verilog_tokentype('#')));
}

TEST(VerilogTokenTest, IsTernaryOperatorTest) {
  EXPECT_FALSE(IsTernaryOperator(verilog_tokentype('*')));
  EXPECT_FALSE(IsTernaryOperator(verilog_tokentype('/')));
  EXPECT_FALSE(IsTernaryOperator(verilog_tokentype('+')));
  EXPECT_FALSE(IsTernaryOperator(verilog_tokentype('-')));
  EXPECT_TRUE(IsTernaryOperator(verilog_tokentype('?')));
  EXPECT_TRUE(IsTernaryOperator(verilog_tokentype(':')));
}

TEST(VerilogTokenTest, IsUnlexedTest) {
  EXPECT_TRUE(IsUnlexed(verilog_tokentype::PP_define_body));
  EXPECT_TRUE(IsUnlexed(verilog_tokentype::MacroArg));
  EXPECT_FALSE(IsUnlexed(verilog_tokentype::TK_COMMENT_BLOCK));
  EXPECT_FALSE(IsUnlexed(verilog_tokentype::SymbolIdentifier));
}

TEST(VerilogTokenTest, IsIdentifierLikeTest) {
  EXPECT_TRUE(IsIdentifierLike(verilog_tokentype::SymbolIdentifier));
  EXPECT_TRUE(IsIdentifierLike(verilog_tokentype::PP_Identifier));
  EXPECT_TRUE(IsIdentifierLike(verilog_tokentype::MacroIdentifier));
  EXPECT_TRUE(IsIdentifierLike(verilog_tokentype::MacroIdItem));
  EXPECT_TRUE(IsIdentifierLike(verilog_tokentype::MacroCallId));
  EXPECT_TRUE(IsIdentifierLike(verilog_tokentype::SystemTFIdentifier));
  EXPECT_TRUE(IsIdentifierLike(verilog_tokentype::EscapedIdentifier));
  EXPECT_TRUE(IsIdentifierLike(verilog_tokentype::TK_Swidth));
  EXPECT_TRUE(IsIdentifierLike(verilog_tokentype::TK_Srecrem));
  EXPECT_TRUE(IsIdentifierLike(verilog_tokentype::TK_Ssetuphold));
  EXPECT_TRUE(IsIdentifierLike(verilog_tokentype::TK_Speriod));
  EXPECT_TRUE(IsIdentifierLike(verilog_tokentype::TK_Shold));
  EXPECT_TRUE(IsIdentifierLike(verilog_tokentype::TK_Srecovery));
  EXPECT_TRUE(IsIdentifierLike(verilog_tokentype::TK_Sremoval));
  EXPECT_TRUE(IsIdentifierLike(verilog_tokentype::TK_Ssetup));
  EXPECT_TRUE(IsIdentifierLike(verilog_tokentype::TK_Sskew));
  EXPECT_TRUE(IsIdentifierLike(verilog_tokentype::TK_Stimeskew));
  EXPECT_TRUE(IsIdentifierLike(verilog_tokentype::TK_Swidth));
  EXPECT_FALSE(IsIdentifierLike(verilog_tokentype::TK_StringLiteral));
  EXPECT_FALSE(IsIdentifierLike(verilog_tokentype::TK_DecNumber));
  EXPECT_FALSE(IsIdentifierLike(verilog_tokentype::TK_DecBase));
  EXPECT_FALSE(IsIdentifierLike(verilog_tokentype::TK_DecDigits));
  EXPECT_FALSE(IsIdentifierLike(verilog_tokentype::TK_always));
  EXPECT_FALSE(IsIdentifierLike(verilog_tokentype::TK_wire));
  EXPECT_FALSE(IsIdentifierLike(verilog_tokentype::TK_EOL_COMMENT));
  EXPECT_FALSE(IsIdentifierLike(verilog_tokentype::TK_COMMENT_BLOCK));
  EXPECT_FALSE(IsIdentifierLike(verilog_tokentype::TKK_attribute));
  EXPECT_FALSE(IsIdentifierLike(verilog_tokentype::TK_ATTRIBUTE));
  EXPECT_FALSE(IsIdentifierLike(verilog_tokentype('+')));
}

}  // namespace
}  // namespace verilog
