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
    {verilog_tokentype::PP_Identifier, FTT::identifier},
    {verilog_tokentype::MacroIdItem, FTT::identifier},
    {verilog_tokentype::MacroCallId, FTT::identifier},
    {verilog_tokentype::TK_Ssetup, FTT::identifier},
    {verilog_tokentype::TK_Sskew, FTT::identifier},
    {verilog_tokentype::TK_Shold, FTT::identifier},
    {verilog_tokentype::PP_include, FTT::keyword},
    {verilog_tokentype::PP_TOKEN_CONCAT, FTT::binary_operator},
    {verilog_tokentype::TK_INCR, FTT::unary_operator},
    {verilog_tokentype::TK_PIPEARROW, FTT::binary_operator},
    {verilog_tokentype::TK_SCOPE_RES, FTT::hierarchy},
    {verilog_tokentype::TK_LE, FTT::binary_operator},
    {verilog_tokentype('='), FTT::binary_operator},  // consistent with TK_LE
    {verilog_tokentype('.'), FTT::hierarchy},
    {verilog_tokentype::TK_edge_descriptor, FTT::edge_descriptor},
    {verilog_tokentype::TK_EOL_COMMENT, FTT::eol_comment},
    {verilog_tokentype::TK_COMMENT_BLOCK, FTT::comment_block},
    {verilog_tokentype('('), FTT::open_group},
    {verilog_tokentype('['), FTT::open_group},
    {verilog_tokentype('{'), FTT::open_group},
    {verilog_tokentype::TK_LP, FTT::open_group},
    {verilog_tokentype(')'), FTT::close_group},
    {verilog_tokentype(']'), FTT::close_group},
    {verilog_tokentype('}'), FTT::close_group},
    {verilog_tokentype::MacroNumericWidth, FTT::numeric_literal},
    {verilog_tokentype::TK_DecNumber, FTT::numeric_literal},
    {verilog_tokentype::TK_RealTime, FTT::numeric_literal},
    {verilog_tokentype::TK_TimeLiteral, FTT::numeric_literal},
    {verilog_tokentype::TK_BinDigits, FTT::numeric_literal},
    {verilog_tokentype::TK_OctDigits, FTT::numeric_literal},
    {verilog_tokentype::TK_HexDigits, FTT::numeric_literal},
    {verilog_tokentype::TK_UnBasedNumber, FTT::numeric_literal},
    {verilog_tokentype::TK_DecBase, FTT::numeric_base},
    {verilog_tokentype::TK_BinBase, FTT::numeric_base},
    {verilog_tokentype::TK_OctBase, FTT::numeric_base},
    {verilog_tokentype::TK_HexBase, FTT::numeric_base},
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
