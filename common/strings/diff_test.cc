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

#include <initializer_list>
#include <sstream>

#include "absl/strings/string_view.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace diff {
// Print functions copied from external_libs/editscript_test.cc
std::ostream& operator<<(std::ostream& out, Operation operation) {
  switch (operation) {
    case Operation::EQUALS:
      return (out << "EQUALS");
    case Operation::DELETE:
      return (out << "DELETE");
    case Operation::INSERT:
      return (out << "INSERT");
  }
  return out;
}

std::ostream& operator<<(std::ostream& out, const diff::Edit& edit) {
  out << "{" << edit.operation << ",[" << edit.start << "," << edit.end << ")}";
  return out;
}

std::ostream& operator<<(std::ostream& out, const Edits& edits) {
  out << "Edits{";
  std::string outer_delim = "";
  for (auto& edit : edits) {
    out << outer_delim << edit;
    outer_delim = ",";
  }
  out << "};";
  return out;
}

}  // namespace diff

namespace verible {
namespace {

using diff::Edits;
using diff::Operation;

using ::testing::ElementsAreArray;

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
    EXPECT_EQ(stream.str(), test.expected) << "\bbefore:\n"
                                           << test.before << "\nafter:\n"
                                           << test.after;
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

// Represents an Edit operation over a number of elements.
struct RelativeEdit {
  Operation operation;
  int64_t size;
};

// Construct a well-formed sequence of Edits with consistent and contiguous
// start/end ranges given a sequence of RelativeEdits.
// Rationale: it is much easier to reason about relative-sized edit ranges
// and absolute indices when hand-crafting test cases.
//
// Example:
//   RelativeEdits:
//   {Operation::EQUALS, 2},
//   {Operation::DELETE, 3},
//   {Operation::INSERT, 4},
//   {Operation::EQUALS, 5},
//
// starting at indices 0 for both sequences,
// translates into diff::Edit's (absolute indices):
//   {Operation::EQUALS, 0, 2},  // both files start at 0 for 2 lines
//   {Operation::DELETE, 2, 5},  // 3 lines [2,5) of old sequence deleted
//   {Operation::INSERT, 2, 6},  // 4 lines [2,6) of new sequence added
//   {Operation::EQUALS, 5, 10}, // both files advance 5 lines in common
//
// See MakeDiffEditsTest below for examples.
//
// (This is currently well-suited for a test-only library, but
// could eventually become a crucial piece of future diff-to-patch-library.)
diff::Edits MakeDiffEdits(const std::vector<RelativeEdit>& relative_edits,
                          int64_t old_index = 0, int64_t new_index = 0) {
  diff::Edits edits;
  for (const RelativeEdit& edit : relative_edits) {
    if (!edits.empty() && edits.back().operation == edit.operation) {
      // same type as previous operation, just combine them.
      edits.back().end += edit.size;
      continue;
    }
    switch (edit.operation) {
      case Operation::EQUALS: {
        const int64_t old_end = old_index + edit.size;
        edits.push_back(diff::Edit{edit.operation, old_index, old_end});
        old_index = old_end;
        new_index += edit.size;
        break;
      }
      case Operation::INSERT: {
        const int64_t new_end = new_index + edit.size;
        edits.push_back(diff::Edit{edit.operation, new_index, new_end});
        new_index = new_end;
        break;
      }
      case Operation::DELETE: {
        const int64_t old_end = old_index + edit.size;
        edits.push_back(diff::Edit{edit.operation, old_index, old_end});
        old_index = old_end;
        break;
      }
    }
  }
  return edits;
}

struct MakeDiffEditsTestCase {
  std::vector<RelativeEdit> rel_edits;
  diff::Edits expected_edits;
};

TEST(MakeDiffEditsTest, Various) {
  const MakeDiffEditsTestCase kTestCases[] = {
      {{}, {}},
      // Single edit operations:
      {{
           {Operation::EQUALS, 10},
       },
       {
           {Operation::EQUALS, 0, 10},
       }},
      {{
           {Operation::DELETE, 8},
       },
       {
           {Operation::DELETE, 0, 8},
       }},
      {{
           {Operation::INSERT, 7},
       },
       {
           {Operation::INSERT, 0, 7},
       }},
      // Repeated edit operations:
      {{
           {Operation::EQUALS, 4},
           {Operation::EQUALS, 6},
       },
       {
           {Operation::EQUALS, 0, 10},
       }},
      {{
           {Operation::DELETE, 5},
           {Operation::DELETE, 3},
       },
       {
           {Operation::DELETE, 0, 8},
       }},
      {{
           {Operation::INSERT, 2},
           {Operation::INSERT, 5},
       },
       {
           {Operation::INSERT, 0, 7},
       }},
      // Cover each edit transition:
      {{
           {Operation::EQUALS, 2},
           {Operation::DELETE, 3},
       },
       {
           {Operation::EQUALS, 0, 2},
           {Operation::DELETE, 2, 5},
       }},
      {{
           {Operation::EQUALS, 4},
           {Operation::INSERT, 5},
       },
       {
           {Operation::EQUALS, 0, 4},
           {Operation::INSERT, 4, 9},
       }},
      {{
           {Operation::DELETE, 3},
           {Operation::EQUALS, 2},
       },
       {
           {Operation::DELETE, 0, 3},
           {Operation::EQUALS, 3, 5},
       }},
      {{
           {Operation::DELETE, 3},
           {Operation::INSERT, 6},
       },
       {
           {Operation::DELETE, 0, 3},
           {Operation::INSERT, 0, 6},
       }},
      {{
           {Operation::INSERT, 7},
           {Operation::EQUALS, 4},
       },
       {
           {Operation::INSERT, 0, 7},
           {Operation::EQUALS, 0, 4},
       }},
      {{
           {Operation::INSERT, 7},
           {Operation::DELETE, 3},
       },
       {
           {Operation::INSERT, 0, 7},
           {Operation::DELETE, 0, 3},
       }},
      {{
           // covers one of each transition
           {Operation::EQUALS, 2},
           {Operation::DELETE, 3},
           {Operation::INSERT, 4},
           {Operation::EQUALS, 5},
           {Operation::INSERT, 6},
           {Operation::DELETE, 7},
           {Operation::EQUALS, 8},
       },
       {
           {Operation::EQUALS, 0, 2},
           {Operation::DELETE, 2, 5},
           {Operation::INSERT, 2, 6},
           {Operation::EQUALS, 5, 10},
           {Operation::INSERT, 11, 17},
           {Operation::DELETE, 10, 17},
           {Operation::EQUALS, 17, 25},
       }},
  };
  for (const auto& test : kTestCases) {
    EXPECT_THAT(MakeDiffEdits(test.rel_edits),
                ElementsAreArray(test.expected_edits));
  }
}

struct DiffEditsToPatchHunksTestCase {
  diff::Edits whole_edits;
  int common_context;
  std::vector<diff::Edits> expected_hunks;
};

TEST(DiffEditsToPatchHunksTest, Various) {
  typedef std::initializer_list<RelativeEdit> RelEdits;
  const DiffEditsToPatchHunksTestCase kTestCases[] = {
      {
          .whole_edits = MakeDiffEdits(RelEdits{{Operation::EQUALS, 2}}),
          .common_context = 1,
          .expected_hunks = {}  // empty because no-change hunk was removed
      },
      {
          .whole_edits = MakeDiffEdits(RelEdits{{Operation::EQUALS, 200}}),
          .common_context = 1,
          .expected_hunks = {}  // empty because no-change hunk was removed
      },
      {.whole_edits = MakeDiffEdits(RelEdits{{Operation::INSERT, 3}}),
       .common_context = 1,
       .expected_hunks =
           {
               MakeDiffEdits(RelEdits{{Operation::INSERT, 3}}),
           }},
      {.whole_edits = MakeDiffEdits(RelEdits{{Operation::DELETE, 4}}),
       .common_context = 1,
       .expected_hunks =
           {
               MakeDiffEdits(RelEdits{{Operation::DELETE, 4}}),
           }},
      {.whole_edits = MakeDiffEdits(RelEdits{
           {Operation::EQUALS, 3},
           {Operation::DELETE, 1},
       }),
       .common_context = 2,  // first hunk should start at line[3-2]
       .expected_hunks =
           {
               MakeDiffEdits(
                   RelEdits{
                       {Operation::EQUALS, 2},
                       {Operation::DELETE, 1},
                   },
                   1, 1),
           }},
      {.whole_edits = MakeDiffEdits(RelEdits{
           {Operation::DELETE, 1},
           {Operation::EQUALS, 3},
       }),
       .common_context = 2,  // last EQUALS edit should be no larger than this
       .expected_hunks =
           {
               MakeDiffEdits(
                   RelEdits{
                       {Operation::DELETE, 1},
                       {Operation::EQUALS, 2},
                   },
                   0, 0),
           }},
      {.whole_edits = MakeDiffEdits(RelEdits{
           {Operation::EQUALS, 3},
           {Operation::DELETE, 1},
           {Operation::EQUALS, 3},
       }),
       .common_context = 2,  // first hunk should start at line[3-2]
       .expected_hunks =
           {
               MakeDiffEdits(
                   RelEdits{
                       {Operation::EQUALS, 2},
                       {Operation::DELETE, 1},
                       {Operation::EQUALS, 2},
                   },
                   1, 1),
           }},
      {.whole_edits = MakeDiffEdits(RelEdits{
           {Operation::EQUALS, 3},
           {Operation::INSERT, 1},
       }),
       .common_context = 2,  // first hunk should start at line[3-2]
       .expected_hunks =
           {
               MakeDiffEdits(
                   RelEdits{
                       {Operation::EQUALS, 2},
                       {Operation::INSERT, 1},
                   },
                   1, 1),
           }},
      {.whole_edits = MakeDiffEdits(RelEdits{
           {Operation::INSERT, 1},
           {Operation::EQUALS, 3},
       }),
       .common_context = 2,  // last EQUALS edit should be no larger than this
       .expected_hunks =
           {
               MakeDiffEdits(
                   RelEdits{
                       {Operation::INSERT, 1},
                       {Operation::EQUALS, 2},
                   },
                   0, 0),
           }},
      {.whole_edits = MakeDiffEdits(RelEdits{
           {Operation::EQUALS, 3},
           {Operation::INSERT, 1},
           {Operation::EQUALS, 3},
       }),
       .common_context = 2,  // first hunk should start at line[3-2]
       .expected_hunks =
           {
               MakeDiffEdits(
                   RelEdits{
                       {Operation::EQUALS, 2},
                       {Operation::INSERT, 1},
                       {Operation::EQUALS, 2},
                   },
                   1, 1),
           }},
      {.whole_edits = MakeDiffEdits(RelEdits{
           {Operation::DELETE, 2},
           {Operation::INSERT, 1},
           {Operation::EQUALS, 4},  // expect to remain in one piece
           {Operation::DELETE, 1},
           {Operation::INSERT, 2},
       }),
       .common_context = 2,
       .expected_hunks =
           {
               MakeDiffEdits(
                   RelEdits{
                       {Operation::DELETE, 2},
                       {Operation::INSERT, 1},
                       {Operation::EQUALS, 4},  // remain in one piece
                       {Operation::DELETE, 1},
                       {Operation::INSERT, 2},
                   },
                   0, 0),
           }},
      {.whole_edits = MakeDiffEdits(RelEdits{
           {Operation::DELETE, 2},
           {Operation::INSERT, 1},
           {Operation::EQUALS, 5},  // expect to split here
           {Operation::DELETE, 1},
           {Operation::INSERT, 2},
       }),
       .common_context = 2,
       .expected_hunks =
           {
               // expect two hunks
               MakeDiffEdits(
                   RelEdits{
                       {Operation::DELETE, 2},
                       {Operation::INSERT, 1},
                       {Operation::EQUALS, 2},
                   },
                   0, 0),
               // one line of EQUALS in the new gap
               MakeDiffEdits(
                   RelEdits{
                       {Operation::EQUALS, 2},
                       {Operation::DELETE, 1},
                       {Operation::INSERT, 2},
                   },
                   5, 4),
           }},
  };
  for (const auto& test : kTestCases) {
    EXPECT_THAT(DiffEditsToPatchHunks(test.whole_edits, test.common_context),
                ElementsAreArray(test.expected_hunks));
  }
}

}  // namespace
}  // namespace verible
