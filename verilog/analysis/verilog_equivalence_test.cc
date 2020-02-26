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

#include "verilog/analysis/verilog_equivalence.h"

#include <iostream>
#include <memory>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/strings/match.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "common/text/token_info.h"
#include "common/util/logging.h"

#undef EXPECT_OK
#define EXPECT_OK(value) EXPECT_TRUE((value).ok())
#undef ASSERT_OK
#define ASSERT_OK(value) ASSERT_TRUE((value).ok())

namespace verilog {
namespace {

static void ExpectCompareWithErrstream(
    std::function<bool(absl::string_view, absl::string_view, std::ostream*)>
        func,
    bool expect_compare, absl::string_view left, absl::string_view right,
    std::ostream* errstream = &std::cout) {
  EXPECT_EQ(func(left, right, errstream), expect_compare)
      << "left:\n"
      << left << "\nright:\n"
      << right;
  {  // commutative comparison check (should be same)
    std::ostringstream errstream;
    EXPECT_EQ(func(right, left, &errstream), expect_compare)
        << "(commutative) " << errstream.str();
  }
}

TEST(FormatEquivalentTest, Spaces) {
  const std::vector<const char*> kTestCases = {
      "",
      " ",
      "\n",
      "\t",
  };
  for (int i = 0; i < kTestCases.size(); ++i) {
    for (int j = i + 1; j < kTestCases.size(); ++j) {
      ExpectCompareWithErrstream(FormatEquivalent, true, kTestCases[i],
                                 kTestCases[j]);
    }
  }
}

TEST(FormatEquivalentTest, ShortSequences) {
  const char* kTestCases[] = {
      "1",
      "2",
      "1;",
      "1 ;",
  };
  ExpectCompareWithErrstream(FormatEquivalent, false, kTestCases[0],
                             kTestCases[1]);
  ExpectCompareWithErrstream(FormatEquivalent, false, kTestCases[0],
                             kTestCases[2]);
  ExpectCompareWithErrstream(FormatEquivalent, false, kTestCases[0],
                             kTestCases[3]);
  ExpectCompareWithErrstream(FormatEquivalent, false, kTestCases[1],
                             kTestCases[2]);
  ExpectCompareWithErrstream(FormatEquivalent, false, kTestCases[1],
                             kTestCases[3]);
  ExpectCompareWithErrstream(FormatEquivalent, true, kTestCases[2],
                             kTestCases[3]);
}

TEST(FormatEquivalentTest, Identifiers) {
  const char* kTestCases[] = {
      "foo bar;",
      "   foo\t\tbar    ;   ",
      "foobar;",  // only 2 tokens
      "foo bar\n;\n",
  };
  ExpectCompareWithErrstream(FormatEquivalent, true, kTestCases[0],
                             kTestCases[1]);
  ExpectCompareWithErrstream(FormatEquivalent, false, kTestCases[0],
                             kTestCases[2]);
  ExpectCompareWithErrstream(FormatEquivalent, true, kTestCases[0],
                             kTestCases[3]);
  ExpectCompareWithErrstream(FormatEquivalent, false, kTestCases[1],
                             kTestCases[2]);
  ExpectCompareWithErrstream(FormatEquivalent, true, kTestCases[1],
                             kTestCases[3]);
  ExpectCompareWithErrstream(FormatEquivalent, false, kTestCases[2],
                             kTestCases[3]);
}

TEST(FormatEquivalentTest, Keyword) {
  const char* kTestCases[] = {
      "wire foo;",
      "  wire  \n\t\t   foo  ;\n",
  };
  ExpectCompareWithErrstream(FormatEquivalent, true, kTestCases[0],
                             kTestCases[1]);
}

TEST(FormatEquivalentTest, Comments) {
  const char* kTestCases[] = {
      "// comment1\n",        //
      "// comment1 \n",       //
      "//    comment1\n",     //
      "   //    comment1\n",  // same as [2]
      "// comment2\n",        //
      "/* comment1 */\n",     //
      "/*  comment1  */\n",   //
  };
  auto span = absl::MakeSpan(kTestCases);
  // At some point in the future when token-reflowing is implemented, these
  // will need to become smarter checks.
  // For now, they only check for exact match.
  for (size_t i = 0; i < span.size(); ++i) {
    for (size_t j = i + 1; j < span.size(); ++j) {
      if (i == 2 && j == 3) {
        ExpectCompareWithErrstream(FormatEquivalent, true, kTestCases[i],
                                   kTestCases[j]);
      } else {
        ExpectCompareWithErrstream(FormatEquivalent, false, kTestCases[i],
                                   kTestCases[j]);
      }
    }
  }
}

TEST(FormatEquivalentTest, DiagnosticLength) {
  const char* kTestCases[] = {
      "module foo\n",
      "module foo;\n",
  };
  {
    std::ostringstream errs;
    ExpectCompareWithErrstream(FormatEquivalent, false, kTestCases[0],
                               kTestCases[1], &errs);
    EXPECT_TRUE(absl::StartsWith(
        errs.str(), "Mismatch in token sequence lengths: 3 vs. 4"));
    EXPECT_TRUE(absl::StrContains(errs.str(), "First mismatched token [2]:"));
  }
  {
    std::ostringstream errs;
    ExpectCompareWithErrstream(FormatEquivalent, false, kTestCases[1],
                               kTestCases[0], &errs);
    EXPECT_TRUE(absl::StartsWith(
        errs.str(), "Mismatch in token sequence lengths: 4 vs. 3"));
    EXPECT_TRUE(absl::StrContains(errs.str(), "First mismatched token [2]:"));
  }
}

TEST(FormatEquivalentTest, DiagnosticMismatch) {
  const char* kTestCases[] = {
      "module foo;\n",
      "module bar;\n",
      "module foo,\n",
  };
  {
    std::ostringstream errs;
    ExpectCompareWithErrstream(FormatEquivalent, false, kTestCases[0],
                               kTestCases[1], &errs);
    EXPECT_TRUE(absl::StartsWith(errs.str(), "First mismatched token [1]:"));
  }
  {
    std::ostringstream errs;
    ExpectCompareWithErrstream(FormatEquivalent, false, kTestCases[0],
                               kTestCases[2], &errs);
    EXPECT_TRUE(absl::StartsWith(errs.str(), "First mismatched token [2]:"));
  }
}

TEST(FormatEquivalentTest, LexErrorOnLeft) {
  std::ostringstream errs;
  ExpectCompareWithErrstream(FormatEquivalent, false, "hello 123badid\n",
                             "hello good_id", &errs);
  EXPECT_TRUE(absl::StrContains(errs.str(), "Error lexing left text"));
  EXPECT_TRUE(absl::StrContains(errs.str(), "123badid"));
}

TEST(FormatEquivalentTest, LexErrorOnRight) {
  std::ostringstream errs;
  ExpectCompareWithErrstream(FormatEquivalent, false, "hello good_id\n",
                             "hello 432_bad_id", &errs);
  EXPECT_TRUE(absl::StrContains(errs.str(), "Error lexing right text"));
  EXPECT_TRUE(absl::StrContains(errs.str(), "432_bad_id"));
}

struct ObfuscationTestCase {
  absl::string_view before;
  absl::string_view after;
  bool expect_match;
};

TEST(ObfuscationEquivalentTest, Various) {
  const ObfuscationTestCase kTestCases[] = {
      {"", "", true},
      {"\n", "\n", true},
      {"\n", "\n\n", false},
      {"\n", "\t", false},  // whitespace must match to be be equivalent
      {"  ", "\t", false},  // whitespace must match to be be equivalent
      {" ", "\t", false},   // whitespace must match to be be equivalent
      {" ", "\n", false},   // whitespace must match to be be equivalent
      {"aabbcc\n", "doremi\n", true},
      {"aabbcc\n", "dorem\n", false},
      {"11\n", "22\n", false},
      {"\"11\"\n", "\"22\"\n", false},
      {"wire\n", "wire\n", true},
      {"wire\n", "logic\n", false},
      {"wire w;\n", "wire w;\n", true},
      {"wire w;", "wire w;\n", false},
      {"wire xxx;\n", "wire yyy;\n", true},  // identifiers changed
      {"$zzz;\n", "$yyy;\n", true},          // identifiers changed
      {"$zzz();\n", "$yyy();\n", true},      // identifiers changed
      {"$zzz;\n", "$yyyy;\n", false},
      {"ff(gg, hh) + ii\n", "pp(qq, rr) + ss\n", true},
      {"ff(gg, hh) + ii\n", "pp(qq, rr) - ss\n", false},
      {"ff(gg, hh) + ii\n", "pp[qq, rr] + ss\n", false},
      {"ff(gg, hh) + ii\n", "pp(qq,  rr) + ss\n", false},
      {"ff(gg, hh) + ii\n", "pp(qq, rrr) + ss\n", false},
      {"ff(gg, hh) + ii\n", "pp(12, rr) + ss\n", false},
      {"ff(gg, hh) + ii\n", "pp(qq, rr)+ss\n", false},
      {"`define FOO\n", "`define BAR\n", true},
      {"`define FOO\n", "`define BARR\n", false},
      {"`define FOO\n", "`define  BAR\n", false},
      {"`define FOO\n", "`define BAR \n", false},
      {"`define FOO xx\n", "`define BAR yy\n", true},
      {"`define FOO xx\n", "`define BAR yyz\n", false},
      {"`define FOO \\\nxx\n", "`define BAR \\\nyy\n", true},
      {"`define FOO \\\nxxx\n", "`define BAR \\\nyy\n", false},
      {"`define FOO \\\nxx\n", "`define BAR \\\n\tyy\n", false},
      {"`define FOO \\\nxx\n", "`define BAR \\\n\nyy\n", false},
      {"`define FOO \\\nxx\n", "`define BAR \\\nyy\n\n", false},
      /* TODO(b/150174736): recursive lexing looks erroneous
      {"`define FOO \\\n`define INNERFOO \\\nxx\n\n",  // `define inside `define
       "`define BAR \\\n`define INNERBAR \\\nyy\n\n", true},
       */
      {"`ifdef FOO\n`endif\n", "`ifdef BAR\n`endif\n", true},
      {"`ifndef FOO\n`endif\n", "`ifndef BAR\n`endif\n", true},
      {"`ifdef FOO\n`endif\n", "`ifndef BAR\n`endif\n", false},
      {"`ifdef FOO\n`elsif BLEH\n`endif\n", "`ifdef BAR\n`elsif BLAH\n`endif\n",
       true},
      {"`ifdef FOOO\n`endif\n", "`ifdef BAR\n`endif\n", false},
      {"`ifdef FOO\n`elsif BLEH\n`endif\n",
       "`ifdef BAR\n`elsif BLAHH\n`endif\n", false},
      {"`FOO\n", "`BAR\n", true},
      {"`FOO;\n", "`BAR;\n", true},
      {"`FOO()\n", "`BAR()\n", true},
      {"`FOO(77)\n", "`BAR(77)\n", true},
      {"`FOO();\n", "`BAR();\n", true},
      {"`FOO()\n", "`BAAR()\n", false},
      {"`FOO()\n", " `BAR()\n", false},
      {"`FOO()\n", "`BAR ()\n", false},
      {"`FOO()\n", "`BAR( )\n", false},
      {"`FOO(77)\n", "`BAR(78)\n", false},
      {"`FOO(`BLAH)\n", "`BAR(`BLEH)\n", true},
      {"`FOO(`BLAH)\n", "`BAR(`BLE)\n", false},
      {"`FOO(`BLAH + `BLIPP)\n", "`BAR(`BLEH + `BLOOP)\n", true},
      {"`FOO(`BLAH + `BLIPP)\n", "`BAR(`BLEH +`BLOOP)\n", false},
      {"`FOO(`BLAH + `BLIPP)\n", "`BAR(`BLEH + `BLOP)\n", false},
      {"`FOO(`BLAH + `BLIPP)\n", "`BAR(`BLEH * `BLOOP)\n", false},
      {"`FOO(`BLAH(`BLIPP))\n", "`BAR(`BLEH(`BLOOP))\n", true},
      {"`FOO(`BLAH(`BLIP))\n", "`BAR(`BLEH(`BLOOP))\n", false},
      {"`FOO(`BLAH(`BLIPP))\n", "`BAR(`BLEH(`BLOOP ))\n", false},
      {"`FOO(`BLAH(`BLIPP))\n", "`BAR(`BLEH( `BLOOP))\n", false},
      {"`FOO(`BLAH(`BLIPP))\n", "`BAR(`BLEH (`BLOOP))\n", false},
      {"`FOO(`BLAH(`BLIPP))\n", "`BAR( `BLEH(`BLOOP))\n", false},
      {"`FOO(`BLAH(`BLIPP))\n", "`BAR(`BLEH(`BLOOP) )\n", false},
      {"\\FOO;!@#$% ", "\\BAR;%$#@! ", true},  // escaped identifier
      {"\\FOO;!@#$% ", "\\BARR;%$#@! ",
       false},  // escaped identifier (!= length)
  };
  for (const auto& test : kTestCases) {
    ExpectCompareWithErrstream(ObfuscationEquivalent, test.expect_match,
                               test.before, test.after);
  }
}

TEST(ObfuscationEquivalentTest, LexErrorOnLeft) {
  std::ostringstream errs;
  ExpectCompareWithErrstream(ObfuscationEquivalent, false, "hello 123badid\n",
                             "hello good_id", &errs);
  EXPECT_TRUE(absl::StrContains(errs.str(), "Error lexing left text"));
  EXPECT_TRUE(absl::StrContains(errs.str(), "123badid"));
}

TEST(ObfuscationEquivalentTest, LexErrorOnRight) {
  std::ostringstream errs;
  ExpectCompareWithErrstream(ObfuscationEquivalent, false, "hello good_id\n",
                             "hello 432_bad_id", &errs);
  EXPECT_TRUE(absl::StrContains(errs.str(), "Error lexing right text"));
  EXPECT_TRUE(absl::StrContains(errs.str(), "432_bad_id"));
}

}  // namespace
}  // namespace verilog
