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

#include <sstream>
#include <string_view>

#include "gtest/gtest.h"

namespace verilog {
namespace {

struct StripCommentsTestCase {
  std::string_view input;
  std::string_view expect_deleted;
  std::string_view expect_spaced;
  std::string_view expect_otherchar;
};

TEST(StripVerilogCommentsTest, Various) {
  constexpr StripCommentsTestCase kTestCases[] = {
      {
          .input = "",
          .expect_deleted = "",
          .expect_spaced = "",
          .expect_otherchar = "",
      },
      {
          // Not even valid Verilog, but still lexes.
          .input = "This is not the greatest code in the world,\n"
                   "This is just a tribute.\n",
          // delete
          .expect_deleted = "This is not the greatest code in the world,\n"
                            "This is just a tribute.\n",
          // space-out
          .expect_spaced = "This is not the greatest code in the world,\n"
                           "This is just a tribute.\n",
          // other char
          .expect_otherchar = "This is not the greatest code in the world,\n"
                              "This is just a tribute.\n",
      },
      {
          .input = "//shush\n",
          .expect_deleted = "\n",
          .expect_spaced = "       \n",
          .expect_otherchar = "//.....\n",
      },
      {
          .input = "//////\n",
          .expect_deleted = "\n",
          .expect_spaced = "      \n",
          .expect_otherchar = "//////\n",
      },
      {
          .input = "//////sh\n",
          .expect_deleted = "\n",
          .expect_spaced = "        \n",
          .expect_otherchar = "//////..\n",
      },
      {
          .input = "/*hush*/\n",
          .expect_deleted = " \n",
          .expect_spaced = "        \n",
          .expect_otherchar = "/*....*/\n",
      },
      {
          .input = "/***hush***/\n",
          .expect_deleted = " \n",
          .expect_spaced = "            \n",
          .expect_otherchar = "/***....***/\n",
      },
      {
          .input = "key/**/word",
          .expect_deleted =
              "key word",  // one space to prevent accidental joining of tokens
          .expect_spaced = "key    word",
          .expect_otherchar = "key/**/word",
      },
      {
          // ignore lexcically invalid tokens, and pass them through
          .input = "/*yyyy*/123badid/*zzzz*/654anotherbadone//xxxxx\n",
          .expect_deleted = " 123badid 654anotherbadone\n",
          .expect_spaced = "        123badid        654anotherbadone       \n",
          .expect_otherchar =
              "/*....*/123badid/*....*/654anotherbadone//.....\n",
      },
      {
          .input = "begin\n"
                   "  /*\n"
                   "a a\n"
                   "bb\n"
                   "c\n"
                   "*/  \n"  // end-of-comment at start of line
                   "end\n",
          // delete
          .expect_deleted = "begin\n"
                            "     \n"
                            "end\n",
          // space-out
          .expect_spaced = "begin\n"
                           "    \n"
                           "   \n"
                           "  \n"
                           " \n"
                           "    \n"
                           "end\n",
          // other char
          .expect_otherchar = "begin\n"
                              "  /*\n"
                              "...\n"
                              "..\n"
                              ".\n"
                              "*/  \n"  // trailing spaces
                              "end\n",
      },
      {
          .input = "begin\n"
                   "  /*\n"
                   "a a\n"
                   "bb\n"
                   "c\n"
                   "  */  \n"  // trailing spaces
                   "end\n",
          // delete
          .expect_deleted = "begin\n"
                            "     \n"
                            "end\n",
          // space-out
          .expect_spaced = "begin\n"
                           "    \n"
                           "   \n"
                           "  \n"
                           " \n"
                           "      \n"
                           "end\n",
          // other char
          .expect_otherchar = "begin\n"
                              "  /*\n"
                              "...\n"
                              "..\n"
                              ".\n"
                              "..*/  \n"  // trailing spaces
                              "end\n",
      },
      {
          // macro call, no comments
          .input = "`MACRO(a, b)\n",
          .expect_deleted = "`MACRO(a, b)\n",
          .expect_spaced = "`MACRO(a, b)\n",
          .expect_otherchar = "`MACRO(a, b)\n",
      },
      {
          // macro call, one comment arg
          .input = "`MACRO(/*abc*/)\n",
          .expect_deleted = "`MACRO( )\n",
          .expect_spaced = "`MACRO(       )\n",
          .expect_otherchar = "`MACRO(/*...*/)\n",
      },
      {
          // macro call, comments around args
          .input = "`MACRO(/*!*/a/*?*/,/*!*/b/*?*/)\n",
          .expect_deleted = "`MACRO( a , b )\n",
          .expect_spaced = "`MACRO(     a     ,     b     )\n",
          .expect_otherchar = "`MACRO(/*.*/a/*.*/,/*.*/b/*.*/)\n",
      },
      {
          // macro call, where args are themselves macro calls
          .input = "`MACRO(/*!*/`INNER(a/*?*/,/*!*/b)/*?*/)\n",
          .expect_deleted = "`MACRO( `INNER(a , b) )\n",
          .expect_spaced = "`MACRO(     `INNER(a     ,     b)     )\n",
          .expect_otherchar = "`MACRO(/*.*/`INNER(a/*.*/,/*.*/b)/*.*/)\n",
      },
      {
          // macro call, EOL comments inside
          .input = "`MACRO(//abc\n"
                   "  //defg\n"
                   ")\n",
          // delete
          .expect_deleted = "`MACRO(\n"
                            "  \n"
                            ")\n",
          // space-out
          .expect_spaced = "`MACRO(     \n"
                           "        \n"
                           ")\n",
          // other char
          .expect_otherchar = "`MACRO(//...\n"
                              "  //....\n"
                              ")\n",
      },
      {
          .input = "`define MACRO  //xyzxyz\n",
          .expect_deleted = "`define MACRO  \n",
          .expect_spaced = "`define MACRO          \n",
          .expect_otherchar = "`define MACRO  //......\n",
      },
      {
          // same, but missing terminating \n
          .input = "`define MACRO  //xyzxyz",
          .expect_deleted = "`define MACRO  ",
          .expect_spaced = "`define MACRO          ",
          .expect_otherchar = "`define MACRO  //......",
      },
      {
          .input = "`define MACRO /*-*/ab/*+*/\n",
          .expect_deleted = "`define MACRO  ab \n",
          .expect_spaced = "`define MACRO      ab     \n",
          .expect_otherchar = "`define MACRO /*.*/ab/*.*/\n",
      },
      {
          // multiline macro definition body using line-continuations
          .input = "`define MACRO //---\\\n"
                   "  //---  \\\n"
                   "  //-----\n",
          // delete
          .expect_deleted = "`define MACRO \\\n"
                            "  \\\n"
                            "  \n",
          // space-out
          .expect_spaced = "`define MACRO      \\\n"
                           "         \\\n"
                           "         \n",
          // other char
          .expect_otherchar = "`define MACRO //...\\\n"
                              "  //.....\\\n"
                              "  //.....\n",
      },
      {
          // multiline macro definition body using line-continuations (no end
          // \n)
          .input = "`define MACRO //---\\\n"
                   "  //---  \\\n"
                   "  //-----",
          // delete
          .expect_deleted = "`define MACRO \\\n"
                            "  \\\n"
                            "  ",
          // space-out
          .expect_spaced = "`define MACRO      \\\n"
                           "         \\\n"
                           "         ",
          // other char
          .expect_otherchar = "`define MACRO //...\\\n"
                              "  //.....\\\n"
                              "  //.....",
      },
      {
          // multiline macro definition body using line-continuations
          .input = "`define MACRO /*-*/\\\n"
                   "  /*-*/  \\\n"
                   "  /*---*/\n",
          // delete
          .expect_deleted = "`define MACRO  \\\n"
                            "     \\\n"
                            "   \n",
          // space-out
          .expect_spaced = "`define MACRO      \\\n"
                           "         \\\n"
                           "         \n",
          // other char
          .expect_otherchar = "`define MACRO /*.*/\\\n"
                              "  /*.*/  \\\n"
                              "  /*...*/\n",
      },
      {
          // `define inside `define
          .input = "`define FOO \\\n"
                   " // description of BAR \\\n"
                   "`define BAR \\\n"
                   "  // placeholder1 \\\n"
                   "  // placeholder2\n",
          // delete
          .expect_deleted = "`define FOO \\\n"
                            " \\\n"
                            "`define BAR \\\n"
                            "  \\\n"
                            "  \n",
          // space-out
          .expect_spaced = "`define FOO \\\n"
                           "                       \\\n"
                           "`define BAR \\\n"
                           "                  \\\n"
                           "                 \n",
          // other char
          .expect_otherchar = "`define FOO \\\n"
                              " //....................\\\n"
                              "`define BAR \\\n"
                              "  //..............\\\n"
                              "  //.............\n",
      },
      {
          // macro call inside `define, one comment arg
          .input = "`define DEF `MACRO(/*abc*/)\n",
          .expect_deleted = "`define DEF `MACRO( )\n",
          .expect_spaced = "`define DEF `MACRO(       )\n",
          .expect_otherchar = "`define DEF `MACRO(/*...*/)\n",
      },
  };
  for (const auto &test : kTestCases) {
    {
      std::ostringstream stream;
      StripVerilogComments(test.input, &stream, '\0');
      EXPECT_EQ(stream.str(), test.expect_deleted);
    }
    {
      std::ostringstream stream;
      StripVerilogComments(test.input, &stream, ' ');
      EXPECT_EQ(stream.str(), test.expect_spaced);
    }
    {
      std::ostringstream stream;
      StripVerilogComments(test.input, &stream, '.');
      EXPECT_EQ(stream.str(), test.expect_otherchar);
    }
  }
}

}  // namespace
}  // namespace verilog
