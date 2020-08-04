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

#include "common/strings/diff.h"

#include <sstream>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/strings/string_view.h"

namespace verible {
namespace {

using diff::Edits;
using diff::Operation;

struct DiffTestCase {
  absl::string_view before;
  absl::string_view after;
  absl::string_view expected;
};

TEST(LineDiffsTest, Various) {
  constexpr DiffTestCase kTestCases[] = {
      {"", "", ""},
      {"", " ", "+ \n"},
      {" ", "", "- \n"},
      {" ", " ", "  \n"},
      {"", "\n", "+\n"},
      {"\n", "", "-\n"},
      {"\n", "\n", " \n"},
      {"\n\n", "\n", " \n-\n"},
      {"\n", "\n\n", " \n+\n"},
      {"foo\nbar", "foo\nBar",  // missing end \n
       " foo\n"
       "-bar\n"
       "+Bar\n"},
      {"foo\nbar\n", "foo\nBar\n",  // with end \n
       " foo\n"
       "-bar\n"
       "+Bar\n"},
      {"foo\nbar\n", "Foo\nbar\n",  // with end \n
       "-foo\n"
       "+Foo\n"
       " bar\n"},
      {"foo\nbar\n", "Foo\nBar\n",  // both lines changed
       "-foo\n"
       "-bar\n"
       "+Foo\n"
       "+Bar\n"},
      {"frodo\nsam\nmerry\npippin\n",  //
       "frodo\nmerry\npippin\n",       //
       " frodo\n"
       "-sam\n"
       " merry\n"
       " pippin\n"},
      {"frodo\nsam\nmerry\npippin\n",      //
       "frodo\nmerry\ngandalf\npippin\n",  //
       " frodo\n"
       "-sam\n"
       " merry\n"
       "+gandalf\n"
       " pippin\n"},
  };
  for (const auto& test : kTestCases) {
    const LineDiffs line_diffs(test.before, test.after);

    std::ostringstream stream;
    stream << line_diffs;
    EXPECT_EQ(stream.str(), test.expected);
  }
}

struct AddedLineNumbersTestCase {
  Edits edits;
  LineNumberSet expected_line_numbers;
};

TEST(DiffEditsToAddedLineNumbersTest, Various) {
  const AddedLineNumbersTestCase kTestCases[] = {
      {{},  //
       {}},
      {{{Operation::DELETE, 0, 3}},  //
       {}},
      {{{Operation::EQUALS, 1, 4}},  //
       {}},
      {{{Operation::INSERT, 2, 5}},  //
       {{3, 6}}},
      {{{Operation::EQUALS, 0, 2},   //
        {Operation::DELETE, 2, 7},   //
        {Operation::INSERT, 2, 4},   //
        {Operation::EQUALS, 7, 9}},  //
       {{3, 5}}},
      {{{Operation::EQUALS, 0, 2},    //
        {Operation::DELETE, 2, 7},    //
        {Operation::INSERT, 2, 4},    //
        {Operation::EQUALS, 7, 9},    //
        {Operation::INSERT, 6, 11}},  //
       {{3, 5}, {7, 12}}},
  };
  for (const auto& test : kTestCases) {
    EXPECT_EQ(DiffEditsToAddedLineNumbers(test.edits),
              test.expected_line_numbers);
  }
}

}  // namespace
}  // namespace verible
