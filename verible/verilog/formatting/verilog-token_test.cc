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

#include "verible/verilog/formatting/verilog-token.h"

#include "gtest/gtest.h"
#include "verible/common/formatting/format-token.h"
#include "verible/common/text/token-info.h"
#include "verible/verilog/parser/verilog-token-enum.h"

namespace verilog {
namespace formatter {
namespace {

using FTT = FormatTokenType;

// UBSAN check will notice, that 9999 is out of range; skip test in that case
#ifndef UNDEFINED_BEHAVIOR_SANITIZER
// Test that GetFormatTokenType() correctly converts a TokenInfo enum to FTT
TEST(VerilogTokenTest, GetFormatTokenTypeTestUnknown) {
  const int FAKE_TOKEN = 9999;
  verible::TokenInfo token_info(FAKE_TOKEN, "FakeToken");
  verible::PreFormatToken format_token(&token_info);
  EXPECT_EQ(FTT::unknown, GetFormatTokenType(verilog_tokentype(FAKE_TOKEN)));
}
#endif

struct GetFormatTokenTypeTestCase {
  verilog_tokentype token_info_type;
  FormatTokenType format_token_type;
};

const GetFormatTokenTypeTestCase GetFormatTokenTypeTestCases[] = {
    {.token_info_type = verilog_tokentype::PP_Identifier,
     .format_token_type = FTT::identifier},
    {.token_info_type = verilog_tokentype::MacroIdItem,
     .format_token_type = FTT::identifier},
    {.token_info_type = verilog_tokentype::MacroCallId,
     .format_token_type = FTT::identifier},
    {.token_info_type = verilog_tokentype::TK_Ssetup,
     .format_token_type = FTT::identifier},
    {.token_info_type = verilog_tokentype::TK_Sskew,
     .format_token_type = FTT::identifier},
    {.token_info_type = verilog_tokentype::TK_Shold,
     .format_token_type = FTT::identifier},
    {.token_info_type = verilog_tokentype::PP_include,
     .format_token_type = FTT::keyword},
    {.token_info_type = verilog_tokentype::PP_TOKEN_CONCAT,
     .format_token_type = FTT::binary_operator},
    {.token_info_type = verilog_tokentype::TK_INCR,
     .format_token_type = FTT::unary_operator},
    {.token_info_type = verilog_tokentype::TK_PIPEARROW,
     .format_token_type = FTT::binary_operator},
    {.token_info_type = verilog_tokentype::TK_SCOPE_RES,
     .format_token_type = FTT::hierarchy},
    {.token_info_type = verilog_tokentype::TK_LE,
     .format_token_type = FTT::binary_operator},
    {.token_info_type = verilog_tokentype('='),
     .format_token_type = FTT::binary_operator},  // consistent with TK_LE
    {.token_info_type = verilog_tokentype('.'),
     .format_token_type = FTT::hierarchy},
    {.token_info_type = verilog_tokentype::TK_edge_descriptor,
     .format_token_type = FTT::edge_descriptor},
    {.token_info_type = verilog_tokentype::TK_EOL_COMMENT,
     .format_token_type = FTT::eol_comment},
    {.token_info_type = verilog_tokentype::TK_COMMENT_BLOCK,
     .format_token_type = FTT::comment_block},
    {.token_info_type = verilog_tokentype('('),
     .format_token_type = FTT::open_group},
    {.token_info_type = verilog_tokentype('['),
     .format_token_type = FTT::open_group},
    {.token_info_type = verilog_tokentype('{'),
     .format_token_type = FTT::open_group},
    {.token_info_type = verilog_tokentype::TK_LP,
     .format_token_type = FTT::open_group},
    {.token_info_type = verilog_tokentype(')'),
     .format_token_type = FTT::close_group},
    {.token_info_type = verilog_tokentype(']'),
     .format_token_type = FTT::close_group},
    {.token_info_type = verilog_tokentype('}'),
     .format_token_type = FTT::close_group},
    {.token_info_type = verilog_tokentype::MacroNumericWidth,
     .format_token_type = FTT::numeric_literal},
    {.token_info_type = verilog_tokentype::TK_DecNumber,
     .format_token_type = FTT::numeric_literal},
    {.token_info_type = verilog_tokentype::TK_RealTime,
     .format_token_type = FTT::numeric_literal},
    {.token_info_type = verilog_tokentype::TK_TimeLiteral,
     .format_token_type = FTT::numeric_literal},
    {.token_info_type = verilog_tokentype::TK_BinDigits,
     .format_token_type = FTT::numeric_literal},
    {.token_info_type = verilog_tokentype::TK_OctDigits,
     .format_token_type = FTT::numeric_literal},
    {.token_info_type = verilog_tokentype::TK_HexDigits,
     .format_token_type = FTT::numeric_literal},
    {.token_info_type = verilog_tokentype::TK_UnBasedNumber,
     .format_token_type = FTT::numeric_literal},
    {.token_info_type = verilog_tokentype::TK_DecBase,
     .format_token_type = FTT::numeric_base},
    {.token_info_type = verilog_tokentype::TK_BinBase,
     .format_token_type = FTT::numeric_base},
    {.token_info_type = verilog_tokentype::TK_OctBase,
     .format_token_type = FTT::numeric_base},
    {.token_info_type = verilog_tokentype::TK_HexBase,
     .format_token_type = FTT::numeric_base},
};

// Test that every type verilog_tokentype properly maps to its respective
// FormatTokenType.
// Yes, this is change-detector test, but it says that the included test cases
// have actually been reviewed, whereas other entries in the map have not
// necessarily been reviewed, and are just set to some default value.
TEST(VerilogTokenTest, GetFormatTokenTypeTest) {
  for (const auto &test_case : GetFormatTokenTypeTestCases) {
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

}  // namespace
}  // namespace formatter
}  // namespace verilog
