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

#include "absl/strings/string_view.h"
#include "gtest/gtest.h"

namespace verilog {
namespace {

struct StripCommentsTestCase {
  absl::string_view input;
  absl::string_view expect_deleted;
  absl::string_view expect_spaced;
  absl::string_view expect_otherchar;
};

TEST(StripVerilogCommentsTest, Various) {
  constexpr StripCommentsTestCase kTestCases[] = {
      {
          "",
          "",
          "",
          "",
      },
      {
          // Not even valid Verilog, but still lexes.
          "This is not the greatest code in the world,\n"
          "This is just a tribute.\n",
          // delete
          "This is not the greatest code in the world,\n"
          "This is just a tribute.\n",
          // space-out
          "This is not the greatest code in the world,\n"
          "This is just a tribute.\n",
          // other char
          "This is not the greatest code in the world,\n"
          "This is just a tribute.\n",
      },
      {
          "//shush\n",
          "\n",
          "       \n",
          "//.....\n",
      },
      {
          "//////\n",
          "\n",
          "      \n",
          "//////\n",
      },
      {
          "//////sh\n",
          "\n",
          "        \n",
          "//////..\n",
      },
      {
          "/*hush*/\n",
          " \n",
          "        \n",
          "/*....*/\n",
      },
      {
          "/***hush***/\n",
          " \n",
          "            \n",
          "/***....***/\n",
      },
      {
          "key/**/word",
          "key word",  // one space to prevent accidental joining of tokens
          "key    word",
          "key/**/word",
      },
      {
          // ignore lexcically invalid tokens, and pass them through
          "/*yyyy*/123badid/*zzzz*/654anotherbadone//xxxxx\n",
          " 123badid 654anotherbadone\n",
          "        123badid        654anotherbadone       \n",
          "/*....*/123badid/*....*/654anotherbadone//.....\n",
      },
      {
          "begin\n"
          "  /*\n"
          "a a\n"
          "bb\n"
          "c\n"
          "*/  \n"  // end-of-comment at start of line
          "end\n",
          // delete
          "begin\n"
          "     \n"
          "end\n",
          // space-out
          "begin\n"
          "    \n"
          "   \n"
          "  \n"
          " \n"
          "    \n"
          "end\n",
          // other char
          "begin\n"
          "  /*\n"
          "...\n"
          "..\n"
          ".\n"
          "*/  \n"  // trailing spaces
          "end\n",
      },
      {
          "begin\n"
          "  /*\n"
          "a a\n"
          "bb\n"
          "c\n"
          "  */  \n"  // trailing spaces
          "end\n",
          // delete
          "begin\n"
          "     \n"
          "end\n",
          // space-out
          "begin\n"
          "    \n"
          "   \n"
          "  \n"
          " \n"
          "      \n"
          "end\n",
          // other char
          "begin\n"
          "  /*\n"
          "...\n"
          "..\n"
          ".\n"
          "..*/  \n"  // trailing spaces
          "end\n",
      },
      {
          // macro call, no comments
          "`MACRO(a, b)\n",
          "`MACRO(a, b)\n",
          "`MACRO(a, b)\n",
          "`MACRO(a, b)\n",
      },
      {
          // macro call, one comment arg
          "`MACRO(/*abc*/)\n",
          "`MACRO( )\n",
          "`MACRO(       )\n",
          "`MACRO(/*...*/)\n",
      },
      {
          // macro call, comments around args
          "`MACRO(/*!*/a/*?*/,/*!*/b/*?*/)\n",
          "`MACRO( a , b )\n",
          "`MACRO(     a     ,     b     )\n",
          "`MACRO(/*.*/a/*.*/,/*.*/b/*.*/)\n",
      },
      {
          // macro call, where args are themselves macro calls
          "`MACRO(/*!*/`INNER(a/*?*/,/*!*/b)/*?*/)\n",
          "`MACRO( `INNER(a , b) )\n",
          "`MACRO(     `INNER(a     ,     b)     )\n",
          "`MACRO(/*.*/`INNER(a/*.*/,/*.*/b)/*.*/)\n",
      },
      {
          // macro call, EOL comments inside
          "`MACRO(//abc\n"
          "  //defg\n"
          ")\n",
          // delete
          "`MACRO(\n"
          "  \n"
          ")\n",
          // space-out
          "`MACRO(     \n"
          "        \n"
          ")\n",
          // other char
          "`MACRO(//...\n"
          "  //....\n"
          ")\n",
      },
      {
          "`define MACRO  //xyzxyz\n",
          "`define MACRO  \n",
          "`define MACRO          \n",
          "`define MACRO  //......\n",
      },
      {
          // same, but missing terminating \n
          "`define MACRO  //xyzxyz",
          "`define MACRO  ",
          "`define MACRO          ",
          "`define MACRO  //......",
      },
      {
          "`define MACRO /*-*/ab/*+*/\n",
          "`define MACRO  ab \n",
          "`define MACRO      ab     \n",
          "`define MACRO /*.*/ab/*.*/\n",
      },
      {
          // multiline macro definition body using line-continuations
          "`define MACRO //---\\\n"
          "  //---  \\\n"
          "  //-----\n",
          // delete
          "`define MACRO \\\n"
          "  \\\n"
          "  \n",
          // space-out
          "`define MACRO      \\\n"
          "         \\\n"
          "         \n",
          // other char
          "`define MACRO //...\\\n"
          "  //.....\\\n"
          "  //.....\n",
      },
      {
          // multiline macro definition body using line-continuations (no end
          // \n)
          "`define MACRO //---\\\n"
          "  //---  \\\n"
          "  //-----",
          // delete
          "`define MACRO \\\n"
          "  \\\n"
          "  ",
          // space-out
          "`define MACRO      \\\n"
          "         \\\n"
          "         ",
          // other char
          "`define MACRO //...\\\n"
          "  //.....\\\n"
          "  //.....",
      },
      {
          // multiline macro definition body using line-continuations
          "`define MACRO /*-*/\\\n"
          "  /*-*/  \\\n"
          "  /*---*/\n",
          // delete
          "`define MACRO  \\\n"
          "     \\\n"
          "   \n",
          // space-out
          "`define MACRO      \\\n"
          "         \\\n"
          "         \n",
          // other char
          "`define MACRO /*.*/\\\n"
          "  /*.*/  \\\n"
          "  /*...*/\n",
      },
      {
          // `define inside `define
          "`define FOO \\\n"
          " // description of BAR \\\n"
          "`define BAR \\\n"
          "  // placeholder1 \\\n"
          "  // placeholder2\n",
          // delete
          "`define FOO \\\n"
          " \\\n"
          "`define BAR \\\n"
          "  \\\n"
          "  \n",
          // space-out
          "`define FOO \\\n"
          "                       \\\n"
          "`define BAR \\\n"
          "                  \\\n"
          "                 \n",
          // other char
          "`define FOO \\\n"
          " //....................\\\n"
          "`define BAR \\\n"
          "  //..............\\\n"
          "  //.............\n",
      },
      {
          // macro call inside `define, one comment arg
          "`define DEF `MACRO(/*abc*/)\n",
          "`define DEF `MACRO( )\n",
          "`define DEF `MACRO(       )\n",
          "`define DEF `MACRO(/*...*/)\n",
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
