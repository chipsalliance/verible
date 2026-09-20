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

#include "verible/verilog/analysis/verilog-equivalence.h"

#include <cstddef>
#include <functional>
#include <iostream>
#include <sstream>
#include <string_view>
#include <vector>

#include "absl/strings/match.h"
#include "absl/types/span.h"
#include "gtest/gtest.h"

#undef EXPECT_OK
#define EXPECT_OK(value) EXPECT_TRUE((value).ok())

#undef ASSERT_OK
#define ASSERT_OK(value) ASSERT_TRUE((value).ok())

namespace verilog {
namespace {

TEST(DiffStatusTest, Print) {
  // check a few enums
  {
    std::ostringstream stream;
    stream << DiffStatus::kDifferent;
    EXPECT_EQ(stream.str(), "different");
  }
  {
    std::ostringstream stream;
    stream << DiffStatus::kEquivalent;
    EXPECT_EQ(stream.str(), "equivalent");
  }
}

static DiffStatus FlipStatus(DiffStatus status) {
  switch (status) {
    case DiffStatus::kLeftError:
      return DiffStatus::kRightError;
    case DiffStatus::kRightError:
      return DiffStatus::kLeftError;
    default:
      return status;
  }
}

static void ExpectCompareWithErrstream(
    const std::function<DiffStatus(std::string_view, std::string_view,
                                   std::ostream *)> &func,
    DiffStatus expect_compare, std::string_view left, std::string_view right,
    std::ostream *errstream = &std::cout) {
  EXPECT_EQ(func(left, right, errstream), expect_compare)
      << "left:\n"
      << left << "\nright:\n"
      << right;
  {  // commutative comparison check (should be same)
    std::ostringstream errstream;
    EXPECT_EQ(func(right, left, &errstream), FlipStatus(expect_compare))
        << "(commutative) " << errstream.str();
  }
}

TEST(FormatEquivalentTest, Spaces) {
  const std::vector<const char *> kTestCases = {
      "",
      " ",
      "\n",
      "\t",
  };
  for (size_t i = 0; i < kTestCases.size(); ++i) {
    for (size_t j = i + 1; j < kTestCases.size(); ++j) {
      ExpectCompareWithErrstream(FormatEquivalent, DiffStatus::kEquivalent,
                                 kTestCases[i], kTestCases[j]);
    }
  }
}

TEST(FormatEquivalentTest, ShortSequences) {
  const char *kTestCases[] = {
      "1",
      "2",
      "1;",
      "1 ;",
  };
  ExpectCompareWithErrstream(FormatEquivalent, DiffStatus::kDifferent,
                             kTestCases[0], kTestCases[1]);
  ExpectCompareWithErrstream(FormatEquivalent, DiffStatus::kDifferent,
                             kTestCases[0], kTestCases[2]);
  ExpectCompareWithErrstream(FormatEquivalent, DiffStatus::kDifferent,
                             kTestCases[0], kTestCases[3]);
  ExpectCompareWithErrstream(FormatEquivalent, DiffStatus::kDifferent,
                             kTestCases[1], kTestCases[2]);
  ExpectCompareWithErrstream(FormatEquivalent, DiffStatus::kDifferent,
                             kTestCases[1], kTestCases[3]);
  ExpectCompareWithErrstream(FormatEquivalent, DiffStatus::kEquivalent,
                             kTestCases[2], kTestCases[3]);
}

TEST(FormatEquivalentTest, Identifiers) {
  const char *kTestCases[] = {
      "foo bar;",
      "   foo\t\tbar    ;   ",
      "foobar;",  // only 2 tokens
      "foo bar\n;\n",
  };
  ExpectCompareWithErrstream(FormatEquivalent, DiffStatus::kEquivalent,
                             kTestCases[0], kTestCases[1]);
  ExpectCompareWithErrstream(FormatEquivalent, DiffStatus::kDifferent,
                             kTestCases[0], kTestCases[2]);
  ExpectCompareWithErrstream(FormatEquivalent, DiffStatus::kEquivalent,
                             kTestCases[0], kTestCases[3]);
  ExpectCompareWithErrstream(FormatEquivalent, DiffStatus::kDifferent,
                             kTestCases[1], kTestCases[2]);
  ExpectCompareWithErrstream(FormatEquivalent, DiffStatus::kEquivalent,
                             kTestCases[1], kTestCases[3]);
  ExpectCompareWithErrstream(FormatEquivalent, DiffStatus::kDifferent,
                             kTestCases[2], kTestCases[3]);
}

TEST(FormatEquivalentTest, Keyword) {
  const char *kTestCases[] = {
      "wire foo;",
      "  wire  \n\t\t   foo  ;\n",
  };
  ExpectCompareWithErrstream(FormatEquivalent, DiffStatus::kEquivalent,
                             kTestCases[0], kTestCases[1]);
}

TEST(FormatEquivalentTest, Comments) {
  const char *kTestCases[] = {
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
        ExpectCompareWithErrstream(FormatEquivalent, DiffStatus::kEquivalent,
                                   kTestCases[i], kTestCases[j]);
      } else {
        ExpectCompareWithErrstream(FormatEquivalent, DiffStatus::kDifferent,
                                   kTestCases[i], kTestCases[j]);
      }
    }
  }
}

TEST(FormatEquivalentTest, DiagnosticLength) {
  const char *kTestCases[] = {
      "module foo\n",
      "module foo;\n",
  };
  {
    std::ostringstream errs;
    ExpectCompareWithErrstream(FormatEquivalent, DiffStatus::kDifferent,
                               kTestCases[0], kTestCases[1], &errs);
    EXPECT_TRUE(absl::StartsWith(
        errs.str(), "Mismatch in token sequence lengths: 3 vs. 4"));
    EXPECT_TRUE(absl::StrContains(errs.str(), "First mismatched token [2]:"));
  }
  {
    std::ostringstream errs;
    ExpectCompareWithErrstream(FormatEquivalent, DiffStatus::kDifferent,
                               kTestCases[1], kTestCases[0], &errs);
    EXPECT_TRUE(absl::StartsWith(
        errs.str(), "Mismatch in token sequence lengths: 4 vs. 3"));
    EXPECT_TRUE(absl::StrContains(errs.str(), "First mismatched token [2]:"));
  }
}

TEST(FormatEquivalentTest, MismatchNumberOfTokens) {
  {
    std::ostringstream errs;
    ExpectCompareWithErrstream(FormatEquivalent, DiffStatus::kDifferent,
                               "module ", "module extra_token", &errs);
    EXPECT_TRUE(absl::StrContains(
        errs.str(), "Mismatch in token sequence lengths: 2 vs. 3"))
        << "full message:\n"
        << errs.str();
    EXPECT_TRUE(absl::StrContains(errs.str(), "extra_token"))
        << "full message:\n"
        << errs.str();
  }
  {
    std::ostringstream errs;
    ExpectCompareWithErrstream(FormatEquivalent, DiffStatus::kDifferent,
                               "module extra_token;", "module ", &errs);
    EXPECT_TRUE(absl::StrContains(
        errs.str(), "Mismatch in token sequence lengths: 4 vs. 2"))
        << "full message:\n"
        << errs.str();
    EXPECT_TRUE(absl::StrContains(errs.str(), "extra_token"))
        << "full message:\n"
        << errs.str();
  }
}

TEST(FormatEquivalentTest, MismatchTokenType) {
  std::ostringstream errs;
  ExpectCompareWithErrstream(FormatEquivalent, DiffStatus::kDifferent,
                             "module k1;", "module 1;", &errs);
  EXPECT_TRUE(absl::StrContains(errs.str(), "Mismatched token enums."))
      << "full message:\n"
      << errs.str();
  EXPECT_TRUE(absl::StrContains(errs.str(), "First mismatched token [1]:"))
      << "full message:\n"
      << errs.str();
  EXPECT_TRUE(absl::StrContains(errs.str(), "SymbolIdentifier"))
      << "full message:\n"
      << errs.str();
  EXPECT_TRUE(absl::StrContains(errs.str(), "TK_DecNumber"))
      << "full message:\n"
      << errs.str();
}

TEST(FormatEquivalentTest, EquivalenceOfRightParen) {
  const char *kTestCases[] = {
      "{`FOO()\n}\n",
      "{`FOO()}\n",
      "{`FOO()()}\n",
      "{`FOO()()\n}\n",
  };
  {
    std::ostringstream errs;
    ExpectCompareWithErrstream(FormatEquivalent, DiffStatus::kEquivalent,
                               kTestCases[0], kTestCases[1], &errs);
    // Test the other way around too.
    ExpectCompareWithErrstream(FormatEquivalent, DiffStatus::kEquivalent,
                               kTestCases[1], kTestCases[0], &errs);
    ExpectCompareWithErrstream(FormatEquivalent, DiffStatus::kEquivalent,
                               kTestCases[2], kTestCases[3], &errs);
  }
}

// MacroIdentifier vs MacroIdItem depends on whether the macro ends the line.
TEST(FormatEquivalentTest, EquivalenceOfMacroIdentifierAndMacroIdItem) {
  const char *kSameSpelling[] = {
      "assign x = f(`TOKEN);\n",
      "assign x = f(\n`TOKEN\n);\n",
  };
  ExpectCompareWithErrstream(FormatEquivalent, DiffStatus::kEquivalent,
                             kSameSpelling[0], kSameSpelling[1]);

  // Different macro names remain different.
  ExpectCompareWithErrstream(FormatEquivalent, DiffStatus::kDifferent,
                             "assign x = f(`TOKEN);\n",
                             "assign x = f(`OTHER);\n");
}

TEST(FormatEquivalentTest, DiagnosticMismatch) {
  const char *kTestCases[] = {
      "module foo;\n",
      "module bar;\n",
      "module foo,\n",
  };
  {
    std::ostringstream errs;
    ExpectCompareWithErrstream(FormatEquivalent, DiffStatus::kDifferent,
                               kTestCases[0], kTestCases[1], &errs);
    EXPECT_TRUE(absl::StrContains(errs.str(), "First mismatched token [1]:"))
        << "full message:\n"
        << errs.str();
    EXPECT_TRUE(absl::StrContains(errs.str(), "foo")) << "full message:\n"
                                                      << errs.str();
    EXPECT_TRUE(absl::StrContains(errs.str(), "bar")) << "full message:\n"
                                                      << errs.str();
  }
  {
    std::ostringstream errs;
    ExpectCompareWithErrstream(FormatEquivalent, DiffStatus::kDifferent,
                               kTestCases[0], kTestCases[2], &errs);
    EXPECT_TRUE(absl::StrContains(errs.str(), "First mismatched token [2]:"))
        << "full message:\n"
        << errs.str();
    EXPECT_TRUE(absl::StrContains(errs.str(), "','")) << "full message:\n"
                                                      << errs.str();
    EXPECT_TRUE(absl::StrContains(errs.str(), "';'")) << "full message:\n"
                                                      << errs.str();
  }
}

TEST(FormatEquivalentTest, LexErrorOnLeft) {
  std::ostringstream errs;
  ExpectCompareWithErrstream(FormatEquivalent, DiffStatus::kLeftError,
                             "hello 123badid\n", "hello good_id", &errs);
  EXPECT_TRUE(absl::StrContains(errs.str(), "error from left input"))
      << "full message:\n"
      << errs.str();
  EXPECT_TRUE(absl::StrContains(errs.str(), "123badid")) << "full message:\n"
                                                         << errs.str();
}

TEST(FormatEquivalentTest, LexErrorOnLeftInMacroArg) {
  std::ostringstream errs;
  ExpectCompareWithErrstream(FormatEquivalent, DiffStatus::kLeftError,
                             "`hello(234badid)\n", "`hello(good_id)", &errs);
  EXPECT_TRUE(absl::StrContains(errs.str(), "error from left input"))
      << "full message:\n"
      << errs.str();
  EXPECT_TRUE(absl::StrContains(errs.str(), "234badid")) << "full message:\n"
                                                         << errs.str();
}

TEST(FormatEquivalentTest, LexErrorOnLeftInMacroDefinitionBody) {
  std::ostringstream errs;
  ExpectCompareWithErrstream(FormatEquivalent, DiffStatus::kLeftError,
                             "`define hello 345badid\n",
                             "`define hello good_id\n", &errs);
  EXPECT_TRUE(absl::StrContains(errs.str(), "error from left input"))
      << "full message:\n"
      << errs.str();
  EXPECT_TRUE(absl::StrContains(errs.str(), "345badid")) << "full message:\n"
                                                         << errs.str();
}

TEST(FormatEquivalentTest, LexErrorOnRight) {
  std::ostringstream errs;
  ExpectCompareWithErrstream(FormatEquivalent, DiffStatus::kRightError,
                             "hello good_id\n", "hello 432_bad_id\n", &errs);
  EXPECT_TRUE(absl::StrContains(errs.str(), "error from right input"))
      << "full message:\n"
      << errs.str();
  EXPECT_TRUE(absl::StrContains(errs.str(), "432_bad_id")) << "full message:\n"
                                                           << errs.str();
}

TEST(FormatEquivalentTest, LexErrorOnRightInMacroArg) {
  std::ostringstream errs;
  ExpectCompareWithErrstream(FormatEquivalent, DiffStatus::kRightError,
                             "`hello(good_id)\n", "`hello(543_bad_id)\n",
                             &errs);
  EXPECT_TRUE(absl::StrContains(errs.str(), "error from right input"))
      << "full message:\n"
      << errs.str();
  EXPECT_TRUE(absl::StrContains(errs.str(), "543_bad_id")) << "full message:\n"
                                                           << errs.str();
}

TEST(FormatEquivalentTest, LexErrorOnRightInMacroDefinitionBody) {
  std::ostringstream errs;
  ExpectCompareWithErrstream(FormatEquivalent, DiffStatus::kRightError,
                             "`define hello good_id\n",
                             "`define hello 654_bad_id\n", &errs);
  EXPECT_TRUE(absl::StrContains(errs.str(), "error from right input"))
      << "full message:\n"
      << errs.str();
  EXPECT_TRUE(absl::StrContains(errs.str(), "654_bad_id")) << "full message:\n"
                                                           << errs.str();
}

struct ObfuscationTestCase {
  std::string_view before;
  std::string_view after;
  DiffStatus expect_match;
};

TEST(ObfuscationEquivalentTest, Various) {
  const ObfuscationTestCase kTestCases[] = {
      {.before = "", .after = "", .expect_match = DiffStatus::kEquivalent},
      {.before = "\n", .after = "\n", .expect_match = DiffStatus::kEquivalent},
      {.before = "\n", .after = "\n\n", .expect_match = DiffStatus::kDifferent},
      {.before = "\n",
       .after = "\t",
       .expect_match = DiffStatus::kDifferent},  // whitespace must match to be
                                                 // be equivalent
      {.before = "  ",
       .after = "\t",
       .expect_match = DiffStatus::kDifferent},  // whitespace must match to be
                                                 // be equivalent
      {.before = " ",
       .after = "\t",
       .expect_match = DiffStatus::kDifferent},  // whitespace must match to be
                                                 // be equivalent
      {.before = " ",
       .after = "\n",
       .expect_match = DiffStatus::kDifferent},  // whitespace must match to be
                                                 // be equivalent
      {.before = "aabbcc\n",
       .after = "doremi\n",
       .expect_match = DiffStatus::kEquivalent},
      {.before = "aabbcc\n",
       .after = "dorem\n",
       .expect_match = DiffStatus::kDifferent},
      {.before = "11\n",
       .after = "22\n",
       .expect_match = DiffStatus::kDifferent},
      {.before = "\"11\"\n",
       .after = "\"22\"\n",
       .expect_match = DiffStatus::kDifferent},
      {.before = "wire\n",
       .after = "wire\n",
       .expect_match = DiffStatus::kEquivalent},
      {.before = "wire\n",
       .after = "logic\n",
       .expect_match = DiffStatus::kDifferent},
      {.before = "wire w;\n",
       .after = "wire w;\n",
       .expect_match = DiffStatus::kEquivalent},
      {.before = "wire w;",
       .after = "wire w;\n",
       .expect_match = DiffStatus::kDifferent},
      {.before = "wire xxx;\n",
       .after = "wire yyy;\n",
       .expect_match = DiffStatus::kEquivalent},  // identifiers changed
      {.before = "$zzz;\n",
       .after = "$yyy;\n",
       .expect_match = DiffStatus::kEquivalent},  // identifiers changed
      {.before = "$zzz();\n",
       .after = "$yyy();\n",
       .expect_match = DiffStatus::kEquivalent},  // identifiers changed
      {.before = "$zzz;\n",
       .after = "$yyyy;\n",
       .expect_match = DiffStatus::kDifferent},
      {.before = "ff(gg, hh) + ii\n",
       .after = "pp(qq, rr) + ss\n",
       .expect_match = DiffStatus::kEquivalent},
      {.before = "ff(gg, hh) + ii\n",
       .after = "pp(qq, rr) - ss\n",
       .expect_match = DiffStatus::kDifferent},
      {.before = "ff(gg, hh) + ii\n",
       .after = "pp[qq, rr] + ss\n",
       .expect_match = DiffStatus::kDifferent},
      {.before = "ff(gg, hh) + ii\n",
       .after = "pp(qq,  rr) + ss\n",
       .expect_match = DiffStatus::kDifferent},
      {.before = "ff(gg, hh) + ii\n",
       .after = "pp(qq, rrr) + ss\n",
       .expect_match = DiffStatus::kDifferent},
      {.before = "ff(gg, hh) + ii\n",
       .after = "pp(12, rr) + ss\n",
       .expect_match = DiffStatus::kDifferent},
      {.before = "ff(gg, hh) + ii\n",
       .after = "pp(qq, rr)+ss\n",
       .expect_match = DiffStatus::kDifferent},
      {.before = "`define FOO\n",
       .after = "`define BAR\n",
       .expect_match = DiffStatus::kEquivalent},
      {.before = "`define FOO\n",
       .after = "`define BARR\n",
       .expect_match = DiffStatus::kDifferent},
      {.before = "`define FOO\n",
       .after = "`define  BAR\n",
       .expect_match = DiffStatus::kDifferent},
      {.before = "`define FOO\n",
       .after = "`define BAR \n",
       .expect_match = DiffStatus::kDifferent},
      {.before = "`define FOO xx\n",
       .after = "`define BAR yy\n",
       .expect_match = DiffStatus::kEquivalent},
      {.before = "`define FOO xx\n",
       .after = "`define BAR yyz\n",
       .expect_match = DiffStatus::kDifferent},
      {.before = "`define FOO \\\nxx\n",
       .after = "`define BAR \\\nyy\n",
       .expect_match = DiffStatus::kEquivalent},
      {.before = "`define FOO \\\nxxx\n",
       .after = "`define BAR \\\nyy\n",
       .expect_match = DiffStatus::kDifferent},
      {.before = "`define FOO \\\nxx\n",
       .after = "`define BAR \\\n\tyy\n",
       .expect_match = DiffStatus::kDifferent},
      {.before = "`define FOO \\\nxx\n",
       .after = "`define BAR \\\n\nyy\n",
       .expect_match = DiffStatus::kDifferent},
      {.before = "`define FOO \\\nxx\n",
       .after = "`define BAR \\\nyy\n\n",
       .expect_match = DiffStatus::kDifferent},
      /* TODO(b/150174736): recursive lexing looks erroneous
      {"`define FOO \\\n`define INNERFOO \\\nxx\n\n",  // `define inside `define
       "`define BAR \\\n`define INNERBAR \\\nyy\n\n", DiffStatus::kEquivalent},
       */
      {.before = "`ifdef FOO\n`endif\n",
       .after = "`ifdef BAR\n`endif\n",
       .expect_match = DiffStatus::kEquivalent},
      {.before = "`ifndef FOO\n`endif\n",
       .after = "`ifndef BAR\n`endif\n",
       .expect_match = DiffStatus::kEquivalent},
      {.before = "`ifdef FOO\n`endif\n",
       .after = "`ifndef BAR\n`endif\n",
       .expect_match = DiffStatus::kDifferent},
      {.before = "`ifdef FOO\n`elsif BLEH\n`endif\n",
       .after = "`ifdef BAR\n`elsif BLAH\n`endif\n",
       .expect_match = DiffStatus::kEquivalent},
      {.before = "`ifdef FOOO\n`endif\n",
       .after = "`ifdef BAR\n`endif\n",
       .expect_match = DiffStatus::kDifferent},
      {.before = "`ifdef FOO\n`elsif BLEH\n`endif\n",
       .after = "`ifdef BAR\n`elsif BLAHH\n`endif\n",
       .expect_match = DiffStatus::kDifferent},
      {.before = "`FOO\n",
       .after = "`BAR\n",
       .expect_match = DiffStatus::kEquivalent},
      {.before = "`FOO;\n",
       .after = "`BAR;\n",
       .expect_match = DiffStatus::kEquivalent},
      {.before = "`FOO()\n",
       .after = "`BAR()\n",
       .expect_match = DiffStatus::kEquivalent},
      {.before = "`FOO(77)\n",
       .after = "`BAR(77)\n",
       .expect_match = DiffStatus::kEquivalent},
      {.before = "`FOO();\n",
       .after = "`BAR();\n",
       .expect_match = DiffStatus::kEquivalent},
      {.before = "`FOO()\n",
       .after = "`BAAR()\n",
       .expect_match = DiffStatus::kDifferent},
      {.before = "`FOO()\n",
       .after = " `BAR()\n",
       .expect_match = DiffStatus::kDifferent},
      {.before = "`FOO()\n",
       .after = "`BAR ()\n",
       .expect_match = DiffStatus::kDifferent},
      {.before = "`FOO()\n",
       .after = "`BAR( )\n",
       .expect_match = DiffStatus::kDifferent},
      {.before = "`FOO(77)\n",
       .after = "`BAR(78)\n",
       .expect_match = DiffStatus::kDifferent},
      {.before = "`FOO(`BLAH)\n",
       .after = "`BAR(`BLEH)\n",
       .expect_match = DiffStatus::kEquivalent},
      {.before = "`FOO(`BLAH)\n",
       .after = "`BAR(`BLE)\n",
       .expect_match = DiffStatus::kDifferent},
      {.before = "`FOO(`BLAH + `BLIPP)\n",
       .after = "`BAR(`BLEH + `BLOOP)\n",
       .expect_match = DiffStatus::kEquivalent},
      {.before = "`FOO(`BLAH + `BLIPP)\n",
       .after = "`BAR(`BLEH +`BLOOP)\n",
       .expect_match = DiffStatus::kDifferent},
      {.before = "`FOO(`BLAH + `BLIPP)\n",
       .after = "`BAR(`BLEH + `BLOP)\n",
       .expect_match = DiffStatus::kDifferent},
      {.before = "`FOO(`BLAH + `BLIPP)\n",
       .after = "`BAR(`BLEH * `BLOOP)\n",
       .expect_match = DiffStatus::kDifferent},
      {.before = "`FOO(`BLAH(`BLIPP))\n",
       .after = "`BAR(`BLEH(`BLOOP))\n",
       .expect_match = DiffStatus::kEquivalent},
      {.before = "`FOO(`BLAH(`BLIP))\n",
       .after = "`BAR(`BLEH(`BLOOP))\n",
       .expect_match = DiffStatus::kDifferent},
      {.before = "`FOO(`BLAH(`BLIPP))\n",
       .after = "`BAR(`BLEH(`BLOOP ))\n",
       .expect_match = DiffStatus::kDifferent},
      {.before = "`FOO(`BLAH(`BLIPP))\n",
       .after = "`BAR(`BLEH( `BLOOP))\n",
       .expect_match = DiffStatus::kDifferent},
      {.before = "`FOO(`BLAH(`BLIPP))\n",
       .after = "`BAR(`BLEH (`BLOOP))\n",
       .expect_match = DiffStatus::kDifferent},
      {.before = "`FOO(`BLAH(`BLIPP))\n",
       .after = "`BAR( `BLEH(`BLOOP))\n",
       .expect_match = DiffStatus::kDifferent},
      {.before = "`FOO(`BLAH(`BLIPP))\n",
       .after = "`BAR(`BLEH(`BLOOP) )\n",
       .expect_match = DiffStatus::kDifferent},
      {.before = "\\FOO;!@#$% ",
       .after = "\\BAR;%$#@! ",
       .expect_match = DiffStatus::kEquivalent},  // escaped identifier
      {.before = "\\FOO;!@#$% ",
       .after = "\\BARR;%$#@! ",
       .expect_match =
           DiffStatus::kDifferent},  // escaped identifier (!= length)
      // token concatenation
      {.before = "abc``xyz",
       .after = "qrs``tuv",
       .expect_match = DiffStatus::kEquivalent},
      {.before = "abc``xyz",
       .after = "qrs``123",
       .expect_match = DiffStatus::kDifferent},
      {.before = "abc``xyz",
       .after = "789``tuv",
       .expect_match = DiffStatus::kDifferent},
      {.before = "`define CAT(ab, xy) ab``xy\n",
       .after = "`define DOG(qr, tu) qr``tu\n",
       .expect_match = DiffStatus::kEquivalent},
      {.before = "`define CAT(ab, xy) ab``xy\n",
       .after = "`define DOG(qr, tuv) qr``tuv\n",
       .expect_match = DiffStatus::kDifferent},
      {.before = "`define CAT(ab, xy) ab``xy\n",
       .after = "`define DOG(qrs, tu) qrs``tu\n",
       .expect_match = DiffStatus::kDifferent},
      {.before = "`CAT(aa``bb, cc``dd)\n",
       .after = "`DOG(jj``kk, ll``mm)\n",
       .expect_match = DiffStatus::kEquivalent},
      {.before = "`CAT(aa``bb, cc``dd)\n",
       .after = "`DOG(jj``kk, llr``mm)\n",
       .expect_match = DiffStatus::kDifferent},
      {.before = "`CAT(aa``bb, cc``dd)\n",
       .after = "`DOG(jj``kk, ll``mms)\n",
       .expect_match = DiffStatus::kDifferent},
  };
  for (const auto &test : kTestCases) {
    ExpectCompareWithErrstream(ObfuscationEquivalent, test.expect_match,
                               test.before, test.after);
  }
}

TEST(ObfuscationEquivalentTest, LexErrorOnLeft) {
  std::ostringstream errs;
  ExpectCompareWithErrstream(ObfuscationEquivalent, DiffStatus::kLeftError,
                             "hello 123badid\n", "hello good_id", &errs);
  EXPECT_TRUE(absl::StrContains(errs.str(), "error from left input"))
      << "full message:\n"
      << errs.str();
  EXPECT_TRUE(absl::StrContains(errs.str(), "123badid")) << "full message:\n"
                                                         << errs.str();
}

TEST(ObfuscationEquivalentTest, LexErrorOnRight) {
  std::ostringstream errs;
  ExpectCompareWithErrstream(ObfuscationEquivalent, DiffStatus::kRightError,
                             "hello good_id\n", "hello 432_bad_id", &errs);
  EXPECT_TRUE(absl::StrContains(errs.str(), "error from right input"))
      << "full message:\n"
      << errs.str();
  EXPECT_TRUE(absl::StrContains(errs.str(), "432_bad_id")) << "full message:\n"
                                                           << errs.str();
}

}  // namespace
}  // namespace verilog
