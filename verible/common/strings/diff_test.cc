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

#include "verible/common/strings/diff.h"

#include <cstdint>
#include <initializer_list>
#include <ostream>
#include <sstream>
#include <string_view>
#include <vector>

#include "external_libs/editscript.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "verible/common/strings/position.h"

namespace diff {
// Print functions copied from external_libs/editscript_test.cc
static std::ostream &operator<<(std::ostream &out, Operation operation) {
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

static std::ostream &operator<<(std::ostream &out, const diff::Edit &edit) {
  out << "{" << edit.operation << ",[" << edit.start << "," << edit.end << ")}";
  return out;
}

}  // namespace diff

namespace verible {
namespace {

using diff::Edits;
using diff::Operation;

using ::testing::ElementsAreArray;

struct DiffTestCase {
  std::string_view before;
  std::string_view after;
  std::string_view expected;
};

TEST(LineDiffsTest, Various) {
  constexpr DiffTestCase kTestCases[] = {
      {.before = "", .after = "", .expected = ""},
      {.before = "", .after = " ", .expected = "+ \n"},
      {.before = " ", .after = "", .expected = "- \n"},
      {.before = " ", .after = " ", .expected = "  \n"},
      {.before = "", .after = "\n", .expected = "+\n"},
      {.before = "\n", .after = "", .expected = "-\n"},
      {.before = "\n", .after = "\n", .expected = " \n"},
      {.before = "\n\n", .after = "\n", .expected = " \n-\n"},
      {.before = "\n", .after = "\n\n", .expected = " \n+\n"},
      {.before = "foo\nbar",
       .after = "foo\nBar",  // missing end \n
       .expected = " foo\n"
                   "-bar\n"
                   "+Bar\n"},
      {.before = "foo\nbar\n",
       .after = "foo\nBar\n",  // with end \n
       .expected = " foo\n"
                   "-bar\n"
                   "+Bar\n"},
      {.before = "foo\nbar\n",
       .after = "Foo\nbar\n",  // with end \n
       .expected = "-foo\n"
                   "+Foo\n"
                   " bar\n"},
      {.before = "foo\nbar\n",
       .after = "Foo\nBar\n",  // both lines changed
       .expected = "-foo\n"
                   "-bar\n"
                   "+Foo\n"
                   "+Bar\n"},
      {.before = "foo\nbar",
       .after = "foo\nbar\n",  // end \n added
       .expected = " foo\n"
                   "-bar\n"
                   "+bar\n"},
      {.before = "frodo\nsam\nmerry\npippin\n",  //
       .after = "frodo\nmerry\npippin\n",        //
       .expected = " frodo\n"
                   "-sam\n"
                   " merry\n"
                   " pippin\n"},
      {.before = "frodo\nsam\nmerry\npippin\n",     //
       .after = "frodo\nmerry\ngandalf\npippin\n",  //
       .expected = " frodo\n"
                   "-sam\n"
                   " merry\n"
                   "+gandalf\n"
                   " pippin\n"},
  };
  for (const auto &test : kTestCases) {
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
      {.edits = {},  //
       .expected_line_numbers = {}},
      {.edits = {{.operation = Operation::DELETE, .start = 0, .end = 3}},  //
       .expected_line_numbers = {}},
      {.edits = {{.operation = Operation::EQUALS, .start = 1, .end = 4}},  //
       .expected_line_numbers = {}},
      {.edits = {{.operation = Operation::INSERT, .start = 2, .end = 5}},  //
       .expected_line_numbers = {{3, 6}}},
      {.edits = {{.operation = Operation::EQUALS, .start = 0, .end = 2},   //
                 {.operation = Operation::DELETE, .start = 2, .end = 7},   //
                 {.operation = Operation::INSERT, .start = 2, .end = 4},   //
                 {.operation = Operation::EQUALS, .start = 7, .end = 9}},  //
       .expected_line_numbers = {{3, 5}}},
      {.edits = {{.operation = Operation::EQUALS, .start = 0, .end = 2},    //
                 {.operation = Operation::DELETE, .start = 2, .end = 7},    //
                 {.operation = Operation::INSERT, .start = 2, .end = 4},    //
                 {.operation = Operation::EQUALS, .start = 7, .end = 9},    //
                 {.operation = Operation::INSERT, .start = 6, .end = 11}},  //
       .expected_line_numbers = {{3, 5}, {7, 12}}},
  };
  for (const auto &test : kTestCases) {
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
diff::Edits MakeDiffEdits(const std::vector<RelativeEdit> &relative_edits,
                          int64_t old_index = 0, int64_t new_index = 0) {
  diff::Edits edits;
  for (const RelativeEdit &edit : relative_edits) {
    if (!edits.empty() && edits.back().operation == edit.operation) {
      // same type as previous operation, just combine them.
      edits.back().end += edit.size;
      continue;
    }
    switch (edit.operation) {
      case Operation::EQUALS: {
        const int64_t old_end = old_index + edit.size;
        edits.push_back(diff::Edit{
            .operation = edit.operation, .start = old_index, .end = old_end});
        old_index = old_end;
        new_index += edit.size;
        break;
      }
      case Operation::INSERT: {
        const int64_t new_end = new_index + edit.size;
        edits.push_back(diff::Edit{
            .operation = edit.operation, .start = new_index, .end = new_end});
        new_index = new_end;
        break;
      }
      case Operation::DELETE: {
        const int64_t old_end = old_index + edit.size;
        edits.push_back(diff::Edit{
            .operation = edit.operation, .start = old_index, .end = old_end});
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
      {.rel_edits = {}, .expected_edits = {}},
      // Single edit operations:
      {.rel_edits =
           {
               {.operation = Operation::EQUALS, .size = 10},
           },
       .expected_edits =
           {
               {.operation = Operation::EQUALS, .start = 0, .end = 10},
           }},
      {.rel_edits =
           {
               {.operation = Operation::DELETE, .size = 8},
           },
       .expected_edits =
           {
               {.operation = Operation::DELETE, .start = 0, .end = 8},
           }},
      {.rel_edits =
           {
               {.operation = Operation::INSERT, .size = 7},
           },
       .expected_edits =
           {
               {.operation = Operation::INSERT, .start = 0, .end = 7},
           }},
      // Repeated edit operations:
      {.rel_edits =
           {
               {.operation = Operation::EQUALS, .size = 4},
               {.operation = Operation::EQUALS, .size = 6},
           },
       .expected_edits =
           {
               {.operation = Operation::EQUALS, .start = 0, .end = 10},
           }},
      {.rel_edits =
           {
               {.operation = Operation::DELETE, .size = 5},
               {.operation = Operation::DELETE, .size = 3},
           },
       .expected_edits =
           {
               {.operation = Operation::DELETE, .start = 0, .end = 8},
           }},
      {.rel_edits =
           {
               {.operation = Operation::INSERT, .size = 2},
               {.operation = Operation::INSERT, .size = 5},
           },
       .expected_edits =
           {
               {.operation = Operation::INSERT, .start = 0, .end = 7},
           }},
      // Cover each edit transition:
      {.rel_edits =
           {
               {.operation = Operation::EQUALS, .size = 2},
               {.operation = Operation::DELETE, .size = 3},
           },
       .expected_edits =
           {
               {.operation = Operation::EQUALS, .start = 0, .end = 2},
               {.operation = Operation::DELETE, .start = 2, .end = 5},
           }},
      {.rel_edits =
           {
               {.operation = Operation::EQUALS, .size = 4},
               {.operation = Operation::INSERT, .size = 5},
           },
       .expected_edits =
           {
               {.operation = Operation::EQUALS, .start = 0, .end = 4},
               {.operation = Operation::INSERT, .start = 4, .end = 9},
           }},
      {.rel_edits =
           {
               {.operation = Operation::DELETE, .size = 3},
               {.operation = Operation::EQUALS, .size = 2},
           },
       .expected_edits =
           {
               {.operation = Operation::DELETE, .start = 0, .end = 3},
               {.operation = Operation::EQUALS, .start = 3, .end = 5},
           }},
      {.rel_edits =
           {
               {.operation = Operation::DELETE, .size = 3},
               {.operation = Operation::INSERT, .size = 6},
           },
       .expected_edits =
           {
               {.operation = Operation::DELETE, .start = 0, .end = 3},
               {.operation = Operation::INSERT, .start = 0, .end = 6},
           }},
      {.rel_edits =
           {
               {.operation = Operation::INSERT, .size = 7},
               {.operation = Operation::EQUALS, .size = 4},
           },
       .expected_edits =
           {
               {.operation = Operation::INSERT, .start = 0, .end = 7},
               {.operation = Operation::EQUALS, .start = 0, .end = 4},
           }},
      {.rel_edits =
           {
               {.operation = Operation::INSERT, .size = 7},
               {.operation = Operation::DELETE, .size = 3},
           },
       .expected_edits =
           {
               {.operation = Operation::INSERT, .start = 0, .end = 7},
               {.operation = Operation::DELETE, .start = 0, .end = 3},
           }},
      {.rel_edits =
           {
               // covers one of each transition
               {.operation = Operation::EQUALS, .size = 2},
               {.operation = Operation::DELETE, .size = 3},
               {.operation = Operation::INSERT, .size = 4},
               {.operation = Operation::EQUALS, .size = 5},
               {.operation = Operation::INSERT, .size = 6},
               {.operation = Operation::DELETE, .size = 7},
               {.operation = Operation::EQUALS, .size = 8},
           },
       .expected_edits =
           {
               {.operation = Operation::EQUALS, .start = 0, .end = 2},
               {.operation = Operation::DELETE, .start = 2, .end = 5},
               {.operation = Operation::INSERT, .start = 2, .end = 6},
               {.operation = Operation::EQUALS, .start = 5, .end = 10},
               {.operation = Operation::INSERT, .start = 11, .end = 17},
               {.operation = Operation::DELETE, .start = 10, .end = 17},
               {.operation = Operation::EQUALS, .start = 17, .end = 25},
           }},
  };
  for (const auto &test : kTestCases) {
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
  using RelEdits = std::initializer_list<RelativeEdit>;
  const DiffEditsToPatchHunksTestCase kTestCases[] = {
      {
          .whole_edits = MakeDiffEdits(
              RelEdits{{.operation = Operation::EQUALS, .size = 2}}),
          .common_context = 1,
          .expected_hunks = {}  // empty because no-change hunk was removed
      },
      {
          .whole_edits = MakeDiffEdits(
              RelEdits{{.operation = Operation::EQUALS, .size = 200}}),
          .common_context = 1,
          .expected_hunks = {}  // empty because no-change hunk was removed
      },
      {.whole_edits =
           MakeDiffEdits(RelEdits{{.operation = Operation::INSERT, .size = 3}}),
       .common_context = 1,
       .expected_hunks =
           {
               MakeDiffEdits(
                   RelEdits{{.operation = Operation::INSERT, .size = 3}}),
           }},
      {.whole_edits =
           MakeDiffEdits(RelEdits{{.operation = Operation::DELETE, .size = 4}}),
       .common_context = 1,
       .expected_hunks =
           {
               MakeDiffEdits(
                   RelEdits{{.operation = Operation::DELETE, .size = 4}}),
           }},
      {.whole_edits = MakeDiffEdits(RelEdits{
           {.operation = Operation::EQUALS, .size = 3},
           {.operation = Operation::DELETE, .size = 1},
       }),
       .common_context = 2,  // first hunk should start at line[3-2]
       .expected_hunks =
           {
               MakeDiffEdits(
                   RelEdits{
                       {.operation = Operation::EQUALS, .size = 2},
                       {.operation = Operation::DELETE, .size = 1},
                   },
                   1, 1),
           }},
      {.whole_edits = MakeDiffEdits(RelEdits{
           {.operation = Operation::DELETE, .size = 1},
           {.operation = Operation::EQUALS, .size = 3},
       }),
       .common_context = 2,  // last EQUALS edit should be no larger than this
       .expected_hunks =
           {
               MakeDiffEdits(
                   RelEdits{
                       {.operation = Operation::DELETE, .size = 1},
                       {.operation = Operation::EQUALS, .size = 2},
                   },
                   0, 0),
           }},
      {.whole_edits = MakeDiffEdits(RelEdits{
           {.operation = Operation::EQUALS, .size = 3},
           {.operation = Operation::DELETE, .size = 1},
           {.operation = Operation::EQUALS, .size = 3},
       }),
       .common_context = 2,  // first hunk should start at line[3-2]
       .expected_hunks =
           {
               MakeDiffEdits(
                   RelEdits{
                       {.operation = Operation::EQUALS, .size = 2},
                       {.operation = Operation::DELETE, .size = 1},
                       {.operation = Operation::EQUALS, .size = 2},
                   },
                   1, 1),
           }},
      {.whole_edits = MakeDiffEdits(RelEdits{
           {.operation = Operation::EQUALS, .size = 3},
           {.operation = Operation::INSERT, .size = 1},
       }),
       .common_context = 2,  // first hunk should start at line[3-2]
       .expected_hunks =
           {
               MakeDiffEdits(
                   RelEdits{
                       {.operation = Operation::EQUALS, .size = 2},
                       {.operation = Operation::INSERT, .size = 1},
                   },
                   1, 1),
           }},
      {.whole_edits = MakeDiffEdits(RelEdits{
           {.operation = Operation::INSERT, .size = 1},
           {.operation = Operation::EQUALS, .size = 3},
       }),
       .common_context = 2,  // last EQUALS edit should be no larger than this
       .expected_hunks =
           {
               MakeDiffEdits(
                   RelEdits{
                       {.operation = Operation::INSERT, .size = 1},
                       {.operation = Operation::EQUALS, .size = 2},
                   },
                   0, 0),
           }},
      {.whole_edits = MakeDiffEdits(RelEdits{
           {.operation = Operation::EQUALS, .size = 3},
           {.operation = Operation::INSERT, .size = 1},
           {.operation = Operation::EQUALS, .size = 3},
       }),
       .common_context = 2,  // first hunk should start at line[3-2]
       .expected_hunks =
           {
               MakeDiffEdits(
                   RelEdits{
                       {.operation = Operation::EQUALS, .size = 2},
                       {.operation = Operation::INSERT, .size = 1},
                       {.operation = Operation::EQUALS, .size = 2},
                   },
                   1, 1),
           }},
      {.whole_edits = MakeDiffEdits(RelEdits{
           {.operation = Operation::DELETE, .size = 2},
           {.operation = Operation::INSERT, .size = 1},
           {.operation = Operation::EQUALS,
            .size = 4},  // expect to remain in one piece
           {.operation = Operation::DELETE, .size = 1},
           {.operation = Operation::INSERT, .size = 2},
       }),
       .common_context = 2,
       .expected_hunks =
           {
               MakeDiffEdits(
                   RelEdits{
                       {.operation = Operation::DELETE, .size = 2},
                       {.operation = Operation::INSERT, .size = 1},
                       {.operation = Operation::EQUALS,
                        .size = 4},  // remain in one piece
                       {.operation = Operation::DELETE, .size = 1},
                       {.operation = Operation::INSERT, .size = 2},
                   },
                   0, 0),
           }},
      {.whole_edits = MakeDiffEdits(RelEdits{
           {.operation = Operation::DELETE, .size = 2},
           {.operation = Operation::INSERT, .size = 1},
           {.operation = Operation::EQUALS, .size = 5},  // expect to split here
           {.operation = Operation::DELETE, .size = 1},
           {.operation = Operation::INSERT, .size = 2},
       }),
       .common_context = 2,
       .expected_hunks =
           {
               // expect two hunks
               MakeDiffEdits(
                   RelEdits{
                       {.operation = Operation::DELETE, .size = 2},
                       {.operation = Operation::INSERT, .size = 1},
                       {.operation = Operation::EQUALS, .size = 2},
                   },
                   0, 0),
               // one line of EQUALS in the new gap
               MakeDiffEdits(
                   RelEdits{
                       {.operation = Operation::EQUALS, .size = 2},
                       {.operation = Operation::DELETE, .size = 1},
                       {.operation = Operation::INSERT, .size = 2},
                   },
                   5, 4),
           }},
  };
  for (const auto &test : kTestCases) {
    EXPECT_THAT(DiffEditsToPatchHunks(test.whole_edits, test.common_context),
                ElementsAreArray(test.expected_hunks));
  }
}

struct LineDiffsToUnifiedDiffTestCase {
  std::string_view before_text;
  std::string_view after_text;
  std::string_view file_a;
  std::string_view file_b;
  int common_context;
  std::string_view expected_diff_text;
};

TEST(LineDiffsToUnifiedDiffTest, Various) {
  const LineDiffsToUnifiedDiffTestCase kTestCases[] = {
      // No changes
      {
          .before_text = "a\nb\nc\n",
          .after_text = "a\nb\nc\n",
          .file_a = {},
          .file_b = {},
          .common_context = 1,
          .expected_diff_text = "",
      },
      {
          .before_text = "a\nb\nc\n",
          .after_text = "a\nb\nc\n",
          .file_a = "file.txt",
          .file_b = {},
          .common_context = 1,
          .expected_diff_text = "",
      },
      {
          .before_text = "a\nb\nc\n",
          .after_text = "a\nb\nc\n",
          .file_a = "old_file.txt",
          .file_b = "new_file.txt",
          .common_context = 1,
          .expected_diff_text = "",
      },
      // Single change
      {
          .before_text = "a\nb\nc\n",
          .after_text = "a\nb\nC\n",
          .file_a = {},
          .file_b = {},
          .common_context = 1,
          .expected_diff_text = "@@ -2,2 +2,2 @@\n"
                                " b\n"
                                "-c\n"
                                "+C\n",
      },
      {
          .before_text = "a\nb\nc\n",
          .after_text = "a\nb\nC\n",
          .file_a = "file.txt",
          .file_b = {},
          .common_context = 1,
          .expected_diff_text = "--- a/file.txt\n"
                                "+++ b/file.txt\n"
                                "@@ -2,2 +2,2 @@\n"
                                " b\n"
                                "-c\n"
                                "+C\n",
      },
      {
          .before_text = "a\nb\nc\n",
          .after_text = "a\nb\nC\n",
          .file_a = "old_file.txt",
          .file_b = "new_file.txt",
          .common_context = 1,
          .expected_diff_text = "--- old_file.txt\n"
                                "+++ new_file.txt\n"
                                "@@ -2,2 +2,2 @@\n"
                                " b\n"
                                "-c\n"
                                "+C\n",
      },
      // Multiple chunks
      {
          .before_text = "a\nb\nc\nd\ne\nf\nh\n",
          .after_text = "A\nb\nc\nd\ne\nf\ng\nh\n",
          .file_a = {},
          .file_b = {},
          .common_context = 1,
          .expected_diff_text = "@@ -1,2 +1,2 @@\n"
                                "-a\n"
                                "+A\n"
                                " b\n"
                                "@@ -6,2 +6,3 @@\n"
                                " f\n"
                                "+g\n"
                                " h\n",
      },
      {
          .before_text = "a\nb\nc\nd\ne\nf\nh\n",
          .after_text = "A\nb\nc\nd\ne\nf\ng\nh\n",
          .file_a = "file.txt",
          .file_b = {},
          .common_context = 1,
          .expected_diff_text = "--- a/file.txt\n"
                                "+++ b/file.txt\n"
                                "@@ -1,2 +1,2 @@\n"
                                "-a\n"
                                "+A\n"
                                " b\n"
                                "@@ -6,2 +6,3 @@\n"
                                " f\n"
                                "+g\n"
                                " h\n",
      },
      {
          .before_text = "a\nb\nc\nd\ne\nf\nh\n",
          .after_text = "A\nb\nc\nd\ne\nf\ng\nh\n",
          .file_a = "old_file.txt",
          .file_b = "new_file.txt",
          .common_context = 1,
          .expected_diff_text = "--- old_file.txt\n"
                                "+++ new_file.txt\n"
                                "@@ -1,2 +1,2 @@\n"
                                "-a\n"
                                "+A\n"
                                " b\n"
                                "@@ -6,2 +6,3 @@\n"
                                " f\n"
                                "+g\n"
                                " h\n",
      },
      // Large context
      {
          .before_text = "a\nb\nc\nd\ne\nf\nh\n",
          .after_text = "A\nb\nc\nd\ne\nf\ng\nh\n",
          .file_a = "file.txt",
          .file_b = {},
          .common_context = 99,
          .expected_diff_text = "--- a/file.txt\n"
                                "+++ b/file.txt\n"
                                "@@ -1,7 +1,8 @@\n"
                                "-a\n"
                                "+A\n"
                                " b\n"
                                " c\n"
                                " d\n"
                                " e\n"
                                " f\n"
                                "+g\n"
                                " h\n",
      },
      // No context
      {
          .before_text = "a\nb\nc\nd\ne\nf\nh\n",
          .after_text = "A\nb\nc\nd\ne\nf\ng\nh\n",
          .file_a = "file.txt",
          .file_b = {},
          .common_context = 0,
          .expected_diff_text = "--- a/file.txt\n"
                                "+++ b/file.txt\n"
                                "@@ -1 +1 @@\n"
                                "-a\n"
                                "+A\n"
                                "@@ -7 +7 @@\n"
                                "+g\n",
      },
      // Multiple inserts and deletions
      {
          .before_text = "a\nb\nc\nh\ni\nj\nk\nm\nn\no\np\nq\nx\ny\nz\nr\ns\n",
          .after_text =
              "a\nb\nc\nd\ne\nf\ng\nh\ni\nj\nk\nl\nm\nn\no\np\nq\nr\ns\n",
          .file_a = "file.txt",
          .file_b = {},
          .common_context = 1,
          .expected_diff_text = "--- a/file.txt\n"
                                "+++ b/file.txt\n"
                                "@@ -3,2 +3,6 @@\n"
                                " c\n"
                                "+d\n"
                                "+e\n"
                                "+f\n"
                                "+g\n"
                                " h\n"
                                "@@ -7,2 +11,3 @@\n"
                                " k\n"
                                "+l\n"
                                " m\n"
                                "@@ -12,5 +17,2 @@\n"
                                " q\n"
                                "-x\n"
                                "-y\n"
                                "-z\n"
                                " r\n",
      },
      // Missing \n in the last line of "before" text
      {
          .before_text = "a\nb\nc\nd\ne\nf\nh",
          .after_text = "A\nb\nc\nd\ne\nf\ng\nh\n",
          .file_a = "file.txt",
          .file_b = {},
          .common_context = 1,
          .expected_diff_text = "--- a/file.txt\n"
                                "+++ b/file.txt\n"
                                "@@ -1,2 +1,2 @@\n"
                                "-a\n"
                                "+A\n"
                                " b\n"
                                "@@ -6,2 +6,3 @@\n"
                                " f\n"
                                "+g\n"
                                "-h\n"
                                "\\ No newline at end of file\n"
                                "+h\n",
      },
      // Missing \n in the last line of "after" text
      {
          .before_text = "a\nb\nc\nd\ne\nf\nh\n",
          .after_text = "A\nb\nc\nd\ne\nf\ng\nh",
          .file_a = "file.txt",
          .file_b = {},
          .common_context = 1,
          .expected_diff_text = "--- a/file.txt\n"
                                "+++ b/file.txt\n"
                                "@@ -1,2 +1,2 @@\n"
                                "-a\n"
                                "+A\n"
                                " b\n"
                                "@@ -6,2 +6,3 @@\n"
                                " f\n"
                                "+g\n"
                                "-h\n"
                                "+h\n"
                                "\\ No newline at end of file\n",
      },
      // Missing \n in the last lines of both texts
      {
          .before_text = "a\nb\nc\nd\ne\nf\nh",
          .after_text = "A\nb\nc\nd\ne\nf\ng\nh",
          .file_a = "file.txt",
          .file_b = {},
          .common_context = 1,
          .expected_diff_text = "--- a/file.txt\n"
                                "+++ b/file.txt\n"
                                "@@ -1,2 +1,2 @@\n"
                                "-a\n"
                                "+A\n"
                                " b\n"
                                "@@ -6,2 +6,3 @@\n"
                                " f\n"
                                "+g\n"
                                " h\n"
                                "\\ No newline at end of file\n",
      },
      // File created
      {
          .before_text = "",
          .after_text = "A\nb\nc\nd\ne\nf\ng\nh\n",
          .file_a = "/dev/null",
          .file_b = "file.txt",
          .common_context = 1,
          .expected_diff_text = "--- /dev/null\n"
                                "+++ file.txt\n"
                                "@@ -1 +1,8 @@\n"
                                "+A\n"
                                "+b\n"
                                "+c\n"
                                "+d\n"
                                "+e\n"
                                "+f\n"
                                "+g\n"
                                "+h\n",
      },
      // File removed
      {
          .before_text = "a\nb\nc\nd\ne\nf\nh\n",
          .after_text = "",
          .file_a = "file.txt",
          .file_b = "/dev/null",
          .common_context = 1,
          .expected_diff_text = "--- file.txt\n"
                                "+++ /dev/null\n"
                                "@@ -1,7 +1 @@\n"
                                "-a\n"
                                "-b\n"
                                "-c\n"
                                "-d\n"
                                "-e\n"
                                "-f\n"
                                "-h\n",
      },
  };

  for (const auto &test : kTestCases) {
    LineDiffs linediffs(test.before_text, test.after_text);
    std::ostringstream stream;
    LineDiffsToUnifiedDiff(stream, linediffs, test.common_context, test.file_a,
                           test.file_b);
    EXPECT_EQ(stream.str(), test.expected_diff_text);
  }
}

}  // namespace
}  // namespace verible
