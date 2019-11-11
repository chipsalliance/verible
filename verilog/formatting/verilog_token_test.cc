// Copyright 2017-2019 The Verible Authors.
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

#include "verilog/formatting/verilog_token.h"

#include "gtest/gtest.h"
#include "common/formatting/format_token.h"
#include "common/text/token_info.h"
#include "verilog/parser/verilog_token_enum.h"

namespace verilog {
namespace formatter {
namespace {

using ::verible::PreFormatToken;
using FTT = FormatTokenType;

// Test that GetFormatTokenType() correctly converts a TokenInfo enum to FTT
TEST(VerilogTokenTest, GetFormatTokenTypeTestUnknown) {
  const int FAKE_TOKEN = 9999;
  verible::TokenInfo token_info(FAKE_TOKEN, "FakeToken");
  verible::PreFormatToken format_token(&token_info);
  EXPECT_EQ(FTT::unknown, GetFormatTokenType(yytokentype(FAKE_TOKEN)));
}

struct GetFormatTokenTypeTestCase {
  yytokentype token_info_type;
  FormatTokenType format_token_type;
};

const GetFormatTokenTypeTestCase GetFormatTokenTypeTestCases[] = {
    {yytokentype::PP_Identifier, FTT::identifier},
    {yytokentype::MacroIdItem, FTT::identifier},
    {yytokentype::MacroCallId, FTT::identifier},
    {yytokentype::PP_include, FTT::keyword},
    {yytokentype::TK_INCR, FTT::unary_operator},
    {yytokentype::TK_PIPEARROW, FTT::binary_operator},
    {yytokentype::TK_SCOPE_RES, FTT::hierarchy},
    {yytokentype::TK_LE, FTT::binary_operator},
    {yytokentype('='), FTT::binary_operator},  // consistent with TK_LE
    {yytokentype('.'), FTT::hierarchy},
    {yytokentype::TK_edge_descriptor, FTT::edge_descriptor},
    {yytokentype::TK_EOL_COMMENT, FTT::eol_comment},
    {yytokentype::TK_COMMENT_BLOCK, FTT::comment_block},
    {yytokentype('('), FTT::open_group},
    {yytokentype('['), FTT::open_group},
    {yytokentype('{'), FTT::open_group},
    {yytokentype::TK_LP, FTT::open_group},
    {yytokentype(')'), FTT::close_group},
    {yytokentype(']'), FTT::close_group},
    {yytokentype('}'), FTT::close_group},
    {yytokentype::MacroNumericWidth, FTT::numeric_literal},
    {yytokentype::TK_DecNumber, FTT::numeric_literal},
    {yytokentype::TK_RealTime, FTT::numeric_literal},
    {yytokentype::TK_TimeLiteral, FTT::numeric_literal},
    {yytokentype::TK_BinDigits, FTT::numeric_literal},
    {yytokentype::TK_OctDigits, FTT::numeric_literal},
    {yytokentype::TK_HexDigits, FTT::numeric_literal},
    {yytokentype::TK_UnBasedNumber, FTT::numeric_literal},
    {yytokentype::TK_DecBase, FTT::numeric_base},
    {yytokentype::TK_BinBase, FTT::numeric_base},
    {yytokentype::TK_OctBase, FTT::numeric_base},
    {yytokentype::TK_HexBase, FTT::numeric_base},
};

// Test that every type yytokentype properly maps to its respective
// FormatTokenType.
// Yes, this is change-detector test, but it says that the included test cases
// have actually been reviewed, whereas other entries in the map have not
// necessarily been reviewed, and are just set to some default value.
TEST(VerilogTokenTest, GetFormatTokenTypeTest) {
  for (const auto& test_case : GetFormatTokenTypeTestCases) {
    EXPECT_EQ(test_case.format_token_type,
              GetFormatTokenType(test_case.token_info_type));
  }
}

// Given a FormatTokenType, test that IsComment returns true only for comments
TEST(VerilogTokenTest, IsCommentFormatTokenTypeTest) {
  EXPECT_TRUE(IsComment(FTT::eol_comment));
  EXPECT_TRUE(IsComment(FTT::comment_block));
  EXPECT_FALSE(IsComment(FTT::binary_operator));
  EXPECT_FALSE(IsComment(FTT::keyword));
}

// Given a yytokentype, test that IsComment returns true only for comments
TEST(VerilogTokenTest, IsCommentyytokentypeTest) {
  EXPECT_TRUE(IsComment(yytokentype::TK_COMMENT_BLOCK));
  EXPECT_TRUE(IsComment(yytokentype::TK_EOL_COMMENT));
  EXPECT_FALSE(IsComment(yytokentype::DR_begin_keywords));
  EXPECT_FALSE(IsComment(yytokentype::SymbolIdentifier));
}

TEST(VerilogTokenTest, IsUnaryOperatorTest) {
  EXPECT_FALSE(IsUnaryOperator(yytokentype('*')));
  EXPECT_FALSE(IsUnaryOperator(yytokentype('/')));
  EXPECT_TRUE(IsUnaryOperator(yytokentype('+')));
  EXPECT_TRUE(IsUnaryOperator(yytokentype('-')));
  EXPECT_TRUE(IsUnaryOperator(yytokentype('~')));
  EXPECT_TRUE(IsUnaryOperator(yytokentype('&')));
  EXPECT_TRUE(IsUnaryOperator(yytokentype('!')));
  EXPECT_TRUE(IsUnaryOperator(yytokentype('|')));
  EXPECT_TRUE(IsUnaryOperator(yytokentype('^')));
  EXPECT_TRUE(IsUnaryOperator(yytokentype::TK_NAND));
  EXPECT_TRUE(IsUnaryOperator(yytokentype::TK_NOR));
  EXPECT_TRUE(IsUnaryOperator(yytokentype::TK_NXOR));
  EXPECT_TRUE(IsUnaryOperator(yytokentype::TK_INCR));
  EXPECT_TRUE(IsUnaryOperator(yytokentype::TK_DECR));
  EXPECT_FALSE(IsUnaryOperator(yytokentype(':')));
  EXPECT_FALSE(IsUnaryOperator(yytokentype('?')));
  EXPECT_FALSE(IsUnaryOperator(yytokentype(',')));
  EXPECT_FALSE(IsUnaryOperator(yytokentype('.')));
  EXPECT_FALSE(IsUnaryOperator(yytokentype(';')));
  EXPECT_FALSE(IsUnaryOperator(yytokentype('#')));
}

}  // namespace
}  // namespace formatter
}  // namespace verilog
