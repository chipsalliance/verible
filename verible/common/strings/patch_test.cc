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

#include "verible/common/strings/patch.h"

#include <cstddef>
#include <initializer_list>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "verible/common/strings/position.h"

namespace verible {

using ::testing::ElementsAre;
using ::testing::ElementsAreArray;

namespace internal {
namespace {

static std::vector<MarkedLine> MakeMarkedLines(
    const std::vector<std::string_view> &lines) {
  std::vector<MarkedLine> result;
  result.reserve(lines.size());
  for (const auto &line : lines) {
    result.emplace_back(line);
  }
  return result;
}

TEST(MarkedLineConstructionTest, BadLine) {
  EXPECT_DEATH(MarkedLine(""), "must start with a marker");
  EXPECT_DEATH(MarkedLine("?"), "Unexpected marker '?'");
}

TEST(MarkedLineEquality, EqualityTests) {
  EXPECT_NE(MarkedLine(" "), MarkedLine("+"));
  EXPECT_NE(MarkedLine(" "), MarkedLine("-"));
  EXPECT_EQ(MarkedLine(" "), MarkedLine(" "));
  EXPECT_NE(MarkedLine(" "), MarkedLine("  "));
  EXPECT_NE(MarkedLine(" 1"), MarkedLine(" 2"));
}

TEST(MarkedLineParseTest, InvalidInputs) {
  constexpr std::string_view kTestCases[] = {
      "", "x", "x213", "abc", "diff", "====",
  };
  for (const auto &test : kTestCases) {
    MarkedLine m;
    EXPECT_FALSE(m.Parse(test).ok()) << " input: \"" << test << '"';
  }
}

struct MarkedLineTestCase {
  std::string_view input;
  char expected_mark;
  std::string_view expected_text;
};

TEST(MarkedLineParseTest, ValidInputs) {
  constexpr MarkedLineTestCase kTestCases[] = {
      {" ", ' ', ""},         {" x", ' ', "x"},       {" x213", ' ', "x213"},
      {"  abc", ' ', " abc"}, {"-abc", '-', "abc"},   {"+abc", '+', "abc"},
      {"- abc", '-', " abc"}, {"+ abc", '+', " abc"}, {"---", '-', "--"},
      {"+++", '+', "++"},     {"-", '-', ""},         {"+", '+', ""},
  };
  for (const auto &test : kTestCases) {
    MarkedLine m;
    EXPECT_TRUE(m.Parse(test.input).ok()) << " input: \"" << test.input << '"';
    EXPECT_EQ(m.Marker(), test.expected_mark);
    EXPECT_EQ(m.Text(), test.expected_text);
  }
}

TEST(MarkedLinePrintTest, Print) {
  constexpr std::string_view kTestCases[] = {
      " ", "+", "-", " 1 2 3", "-xyz", "+\tabc",
  };
  for (const auto &test : kTestCases) {
    MarkedLine m;
    const auto status = m.Parse(test);
    ASSERT_TRUE(status.ok()) << status.message();
    std::ostringstream stream;
    stream << m;
    EXPECT_EQ(stream.str(), test);
  }
}

TEST(HunkIndicesEqualityTest, Comparisons) {
  EXPECT_EQ((HunkIndices{1, 2}), (HunkIndices{1, 2}));
  EXPECT_NE((HunkIndices{1, 1}), (HunkIndices{1, 2}));
  EXPECT_NE((HunkIndices{1, 2}), (HunkIndices{1, 1}));
  EXPECT_NE((HunkIndices{1, 2}), (HunkIndices{2, 2}));
  EXPECT_NE((HunkIndices{3, 2}), (HunkIndices{1, 2}));
}

TEST(HunkIndicesParseTest, InvalidInputs) {
  constexpr std::string_view kTestCases[] = {
      "", ",", "4,", ",5", "2,b", "x,2", "4,5,", "1,2,3",
  };
  for (const auto &test : kTestCases) {
    HunkIndices h;
    EXPECT_FALSE(h.Parse(test).ok()) << " input: \"" << test << '"';
  }
}

struct HunkIndicesTestCase {
  std::string_view input;
  int expected_start;
  int expected_count;
};

TEST(HunkIndicesParseAndPrintTest, ValidInputs) {
  constexpr HunkIndicesTestCase kTestCases[] = {
      {"1,1", 1, 1},
      {"14,92", 14, 92},
  };
  for (const auto &test : kTestCases) {
    HunkIndices h;
    EXPECT_TRUE(h.Parse(test.input).ok()) << " input: \"" << test.input << '"';
    EXPECT_EQ(h.start, test.expected_start);
    EXPECT_EQ(h.count, test.expected_count);

    // Use same data to test printing
    std::ostringstream stream;
    stream << h;
    EXPECT_EQ(stream.str(), test.input);
  }
}

TEST(HunkHeaderEqualityTest, Comparisons) {
  EXPECT_EQ((HunkHeader{{1, 2}, {3, 4}, ""}), (HunkHeader{{1, 2}, {3, 4}, ""}));
  EXPECT_NE((HunkHeader{{1, 2}, {3, 4}, ""}), (HunkHeader{{2, 2}, {3, 4}, ""}));
  EXPECT_NE((HunkHeader{{1, 2}, {3, 4}, ""}), (HunkHeader{{1, 1}, {3, 4}, ""}));
  EXPECT_NE((HunkHeader{{1, 2}, {3, 4}, ""}), (HunkHeader{{1, 2}, {1, 4}, ""}));
  EXPECT_NE((HunkHeader{{1, 2}, {3, 4}, ""}), (HunkHeader{{1, 2}, {3, 5}, ""}));
  EXPECT_NE((HunkHeader{{1, 2}, {3, 4}, ""}),
            (HunkHeader{{1, 2}, {3, 4}, " "}));
}

TEST(HunkHeaderParseTest, InvalidInputs) {
  // If any one character is deleted from this example, it becomes invalid.
  constexpr std::string_view kValidText = "@@ -4,8 +5,6 @@";

  for (size_t i = 0; i < kValidText.length(); ++i) {
    std::string deleted(kValidText);
    deleted.erase(i);
    HunkHeader h;
    EXPECT_FALSE(h.Parse(deleted).ok()) << " input: \"" << deleted << '"';
  }

  for (size_t i = 0; i < kValidText.length(); ++i) {
    std::string_view deleted(kValidText.substr(0, i));
    HunkHeader h;
    EXPECT_FALSE(h.Parse(deleted).ok()) << " input: \"" << deleted << '"';
  }

  for (size_t i = 1; i < kValidText.length(); ++i) {
    std::string_view deleted(kValidText.substr(i));
    HunkHeader h;
    EXPECT_FALSE(h.Parse(deleted).ok()) << " input: \"" << deleted << '"';
  }
}

TEST(HunkHeaderParseTest, MalformedOldRange) {
  constexpr std::string_view kInvalidText = "@@ 4,8 +5,6 @@";
  HunkHeader h;
  const auto status = h.Parse(kInvalidText);
  EXPECT_FALSE(status.ok()) << " input: \"" << kInvalidText << '"';
  EXPECT_TRUE(absl::StrContains(status.message(),
                                "old-file range should start with '-'"))
      << " got: " << status.message();
}

TEST(HunkHeaderParseTest, MalformedNewRange) {
  constexpr std::string_view kInvalidText = "@@ -4,8 5,6 @@";
  HunkHeader h;
  const auto status = h.Parse(kInvalidText);
  EXPECT_FALSE(status.ok()) << " input: \"" << kInvalidText << '"';
  EXPECT_TRUE(absl::StrContains(status.message(),
                                "new-file range should start with '+'"))
      << " got: " << status.message();
}

TEST(HunkHeaderParseAndPrintTest, ValidInput) {
  constexpr std::string_view kValidText = "@@ -14,8 +5,16 @@";
  HunkHeader h;
  EXPECT_TRUE(h.Parse(kValidText).ok()) << " input: \"" << kValidText << '"';
  EXPECT_EQ(h.old_range.start, 14);
  EXPECT_EQ(h.old_range.count, 8);
  EXPECT_EQ(h.new_range.start, 5);
  EXPECT_EQ(h.new_range.count, 16);
  EXPECT_EQ(h.context, "");

  // Validate reversibility.
  std::ostringstream stream;
  stream << h;
  EXPECT_EQ(stream.str(), kValidText);
}

TEST(HunkHeaderParseAndPrintTest, ValidInputWithContext) {
  constexpr std::string_view kValidText("@@ -4,28 +51,6 @@ void foo::bar() {");
  HunkHeader h;
  EXPECT_TRUE(h.Parse(kValidText).ok()) << " input: \"" << kValidText << '"';
  EXPECT_EQ(h.old_range.start, 4);
  EXPECT_EQ(h.old_range.count, 28);
  EXPECT_EQ(h.new_range.start, 51);
  EXPECT_EQ(h.new_range.count, 6);
  EXPECT_EQ(h.context, " void foo::bar() {");

  // Validate reversibility.
  std::ostringstream stream;
  stream << h;
  EXPECT_EQ(stream.str(), kValidText);
}

TEST(SourceInfoParseTest, InvalidInputs) {
  constexpr std::string_view kTestCases[] = {
      "",  // path must be non-empty
  };
  for (const auto &test : kTestCases) {
    SourceInfo info;
    EXPECT_FALSE(info.Parse(test).ok()) << " input: \"" << test << '"';
  }
}

TEST(SourceInfoParseAndPrintTest, ValidInputsPathOnly) {
  constexpr std::string_view kPaths[] = {
      "a.txt",
      "p/q/a.txt",
      "/p/q/a.txt",
  };
  for (const auto &path : kPaths) {
    SourceInfo info;
    EXPECT_TRUE(info.Parse(path).ok());
    EXPECT_EQ(info.path, path);
    EXPECT_TRUE(info.timestamp.empty());

    // Validate reversibility.
    std::ostringstream stream;
    stream << info;
    EXPECT_EQ(stream.str(), path);
  }
}

TEST(SourceInfoParseAndPrintTest, ValidInputsWithTimestamps) {
  constexpr std::string_view kPaths[] = {
      "a.txt",
      "p/q/a.txt",
      "/p/q/a.txt",
  };
  constexpr std::string_view kTimes[] = {
      "2020-02-02",
      "2020-02-02 20:22:02",
      "2020-02-02 20:22:02.000000",
      "2020-02-02 20:22:02.000000 -0700",
  };
  for (const auto &path : kPaths) {
    for (const auto &time : kTimes) {
      SourceInfo info;
      const std::string text(absl::StrCat(path, "\t", time));
      EXPECT_TRUE(info.Parse(text).ok());
      EXPECT_EQ(info.path, path);
      EXPECT_EQ(info.timestamp, time);

      // Validate reversibility.
      std::ostringstream stream;
      stream << info;
      EXPECT_EQ(stream.str(), text);
    }
  }
}

TEST(HunkEqualityTest, Comparisons) {
  const std::vector<std::string_view> lines{
      " a",
      "-b",
      "+c",
      " d",
  };
  EXPECT_EQ(Hunk(1, 1, lines.begin(), lines.end()),  //
            Hunk(1, 1, lines.begin(), lines.end()));
  EXPECT_NE(Hunk(1, 1, lines.begin(), lines.end()),  //
            Hunk(2, 1, lines.begin(), lines.end()));
  EXPECT_NE(Hunk(1, 1, lines.begin(), lines.end()),  //
            Hunk(1, 3, lines.begin(), lines.end()));
  EXPECT_NE(Hunk(2, 4, lines.begin(), lines.end()),  //
            Hunk(2, 4, lines.begin() + 1, lines.end()));
  EXPECT_NE(Hunk(2, 4, lines.begin(), lines.end()),  //
            Hunk(2, 4, lines.begin(), lines.end() - 1));
}

TEST(HunkParseTest, InvalidInputs) {
  const std::vector<std::string_view> kTestCases[] = {
      // malformed headers:
      {"@@ -1,0 +2,0 @"},
      {"@ -1,0 +2,0 @@"},
      {"@@ -1,0+2,0 @@"},
      // malformed MarkedLines:
      {"@@ -1,1 +2,1 @@", ""},  // missing marker character
      {"@@ -1,1 +2,1 @@", "missing leading marker character"},
      // inconsistent line counts:
      {"@@ -1,0 +2,0 @@", "-unexpected"},
      {"@@ -1,0 +2,0 @@", "+unexpected"},
      {"@@ -1,0 +2,0 @@", " unexpected"},
      {
          "@@ -1,1 +2,0 @@",
          // missing: "-..."
      },
      {
          "@@ -1,0 +2,1 @@",
          // missing: "+..."
      },
      {
          "@@ -1,1 +2,1 @@",
          // missing: " ..."
      },
  };
  for (const auto &lines : kTestCases) {
    Hunk hunk;
    const LineRange range(lines.begin(), lines.end());
    EXPECT_FALSE(hunk.Parse(range).ok());
  }
}

struct UpdateHeaderTestCase {
  std::string_view fixed_header;
  std::vector<std::string_view> payload;
};

TEST(HunkUpdateHeaderTest, Various) {
  constexpr std::string_view kNonsenseHeader = "@@ -222,999 +333,999 @@";
  const UpdateHeaderTestCase kTestCases[] = {
      {"@@ -222,0 +333,0 @@", {/* empty lines */}},
      {"@@ -222,1 +333,0 @@",
       {
           "-removed",
       }},
      {"@@ -222,0 +333,1 @@",
       {
           "+added",
       }},
      {"@@ -222,1 +333,1 @@",
       {
           " common",
       }},
      {"@@ -222,4 +333,3 @@",
       {" common", "-removed", "-removed2", "+added", " common again"}},
  };
  for (const auto &test : kTestCases) {
    std::vector<std::string_view> lines;
    lines.push_back(kNonsenseHeader);
    lines.insert(lines.end(), test.payload.begin(), test.payload.end());
    const LineRange range(lines.begin(), lines.end());

    Hunk hunk;
    EXPECT_FALSE(hunk.Parse(range).ok());
    hunk.UpdateHeader();
    const auto status = hunk.IsValid();
    EXPECT_TRUE(status.ok()) << status.message();

    std::ostringstream stream;
    stream << hunk.Header();
    EXPECT_EQ(stream.str(), test.fixed_header);
  }
}

struct AddedLinesTestCase {
  std::vector<std::string_view> hunk_text;
  LineNumberSet expected_added_lines;
};

TEST(HunkAddedLinesTest, Various) {
  const AddedLinesTestCase kTestCases[] = {
      {
          {
              "@@ -7,1 +8,1 @@",
              " common line, not added",
          },
          {},
      },
      {
          {
              "@@ -7,2 +8,1 @@",
              "-deleted line",
              " common line, not added",
          },
          {},
      },
      {
          {"@@ -7,2 +8,1 @@", " common line, not added", "-deleted line"},
          {},
      },
      {
          {
              "@@ -7,4 +8,2 @@",
              " common line, not added",
              "-deleted line",
              "-deleted line 2",
              " common line, not added",
          },
          {},
      },
      {
          {
              "@@ -7,1 +8,2 @@",
              " common line, not added",
              "+added line",
          },
          {{9, 10}},
      },
      {
          {
              "@@ -7,1 +8,2 @@",
              "+added line",
              " common line, not added",
          },
          {{8, 9}},
      },
      {
          {
              "@@ -17,2 +28,4 @@",
              " common line, not added",
              "+added line",
              "+added line 2",
              " common line, not added",
          },
          {{29, 31}},
      },
      {
          {
              "@@ -7,3 +4,3 @@",
              " common line, not added",
              "-deleted line",
              "+added line",
              " common line, not added",
          },
          {{5, 6}},
      },
      {
          {
              "@@ -7,3 +4,3 @@",
              " common line, not added",
              "+added line",
              "-deleted line",
              " common line, not added",
          },
          {{5, 6}},
      },
      {
          {
              "@@ -380,8 +401,12 @@",
              " common line, not added",
              "+added line",
              "+added line 2",
              " nothing interesting",
              " ",
              "-delete me",
              "+replacement",
              " ",
              " nothing interesting",
              " ",
              "+added line",
              "+added line 2",
              " common line, not added",
          },
          {{402, 404}, {406, 407}, {410, 412}},
      },
  };
  for (const auto &test : kTestCases) {
    const LineRange range(test.hunk_text.begin(), test.hunk_text.end());
    Hunk hunk;
    const auto status = hunk.Parse(range);
    ASSERT_TRUE(status.ok()) << status.message();
    EXPECT_EQ(hunk.AddedLines(), test.expected_added_lines);
  }
}

TEST(HunkSplitTest, ExpectedSingletons) {
  const std::vector<std::string_view> kTestCases[] = {
      {" no-change"},
      {"+one addition"},
      {"-one deletion"},
      {" no-change",  //
       " multi-no-change"},
      {" same",   //
       "+added",  //
       " same"},
      {" same",        //
       "+added",       //
       "+added more",  //
       " same"},
      {" same",     //
       "-removed",  //
       " same"},
      {" same",          //
       "-removed",       //
       "-removed more",  //
       " same"},
      {" same",          //
       "-replace this",  //
       "+with this",     //
       " same"},
      {" same",          //
       "-replace this",  //
       "-and this",      //
       "+with this",     //
       " same"},
      {" same",          //
       "-replace this",  //
       "+with this",     //
       "+and this",      //
       " same"},
      {" same",               //
       " same more context",  //
       "-replace this",       //
       "-and this",           //
       "+with this",          //
       "+and this",           //
       " same more context",  //
       " same"},
  };
  for (const auto &test : kTestCases) {
    const std::vector<MarkedLine> lines(MakeMarkedLines(test));
    // can pick any starting line numbers, doesn't matter
    const Hunk hunk(10, 12, lines.begin(), lines.end());
    const std::vector<Hunk> splits(hunk.Split());
    // expect returned list contains exactly the original hunk
    EXPECT_THAT(splits, ElementsAre(hunk));
  }
}

struct ExpectedHunk {
  int old_starting_line;
  int new_starting_line;
  std::pair<int, int> marked_line_offsets;
};

static std::vector<Hunk> MakeExpectedHunks(
    const Hunk &original, const std::vector<ExpectedHunk> &hunk_infos) {
  const auto &marked_lines = original.MarkedLines();
  std::vector<Hunk> results;
  results.reserve(hunk_infos.size());
  for (const auto &hunk_info : hunk_infos) {
    results.emplace_back(
        hunk_info.old_starting_line, hunk_info.new_starting_line,
        marked_lines.begin() + hunk_info.marked_line_offsets.first,
        marked_lines.begin() + hunk_info.marked_line_offsets.second);
  }
  return results;
}

struct HunkSplitTestCase {
  std::vector<MarkedLine> marked_lines;
  Hunk original_hunk;
  std::vector<Hunk> expected_sub_hunks;

  HunkSplitTestCase(int old_starting_line, int new_starting_line,
                    const std::vector<std::string_view> &lines,
                    const std::vector<ExpectedHunk> &sub_hunks)
      : marked_lines(MakeMarkedLines(lines)),
        original_hunk(old_starting_line, new_starting_line,
                      marked_lines.begin(), marked_lines.end()),
        expected_sub_hunks(MakeExpectedHunks(original_hunk, sub_hunks)) {}
};

TEST(HunkSplitTest, MultipleSubHunks) {
  const HunkSplitTestCase kTestCases[] = {
      HunkSplitTestCase(4, 6,
                        {" no-change",  //
                         "-remove",     //
                         "+replace",    //
                         " no-change"},
                        {
                            // one hunk (no split)
                            {4, 6, {0, 4}},
                        }),
      HunkSplitTestCase(4, 5,
                        {"+insert",  //
                         " coins",   //
                         "+to continue"},
                        {
                            // two hunks
                            {4, 5, {0, 1}},
                            {4, 6, {1, 3}},
                        }),
      HunkSplitTestCase(4, 5,
                        {"+insert",  //
                         " more",    //
                         " coins",   //
                         "+to continue"},
                        {
                            // two hunks
                            {4, 5, {0, 1}},
                            {4, 6, {1, 4}},
                        }),
      HunkSplitTestCase(7, 7,
                        {"-insert",  //
                         " coins",   //
                         "-to continue"},
                        {
                            // two hunks
                            {7, 7, {0, 1}},
                            {8, 7, {1, 3}},
                        }),
      HunkSplitTestCase(7, 7,
                        {"-move",       //
                         "-these",      //
                         " untouched",  //
                         "+move", "+these"},
                        {
                            // two hunks
                            {7, 7, {0, 2}},
                            {9, 7, {2, 5}},
                        }),
      HunkSplitTestCase(2, 1,
                        {" context",  //
                         "-in",       //
                         "+space",    //
                         " no one",   //
                         "-can",      //
                         "-hear",     //
                         "+you",      //
                         "+scream"},
                        {
                            // two hunks
                            {2, 1, {0, 3}},
                            {4, 3, {3, 8}},
                        }),
      HunkSplitTestCase(3, 4,
                        {" delete",  //
                         "-every",   //
                         " other",   //
                         "-line",    //
                         " ",        //
                         "-delete",  //
                         " every",   //
                         "-other",   //
                         " line"},
                        {
                            // many hunks
                            {3, 4, {0, 2}},
                            {5, 5, {2, 4}},
                            {7, 6, {4, 6}},
                            {9, 7, {6, 9}},
                        }),
      HunkSplitTestCase(10, 10,
                        {" a",          //
                         "-long long",  //
                         "+time",       //
                         "-ago",        //
                         " in",         //
                         "+a",          //
                         "+galaxy",     //
                         " far",        //
                         "-far",        //
                         " away"},
                        {
                            // many hunks
                            {10, 10, {0, 4}},
                            {13, 12, {4, 7}},
                            {14, 15, {7, 10}},
                        }),
  };
  for (const auto &test : kTestCases) {
    EXPECT_THAT(test.original_hunk.Split(),
                ElementsAreArray(test.expected_sub_hunks));
  }
}

TEST(HunkParseAndPrintTest, ValidInputs) {
  const std::vector<std::string_view> kTestCases[] = {
      {"@@ -1,0 +2,0 @@"},  // 0 line counts, technically consistent
      {"@@ -1,2 +2,2 @@",   // 2 lines of context, common to before/after
       " same1",            //
       " same2"},
      {"@@ -1,2 +2,2 @@ int foo(void) {",  // additional context
       " same1",                           //
       " same2"},
      {"@@ -1,2 +2,0 @@",  // only deletions, no context lines
       "-erase me",        //
       "-erase me too"},
      {"@@ -1,0 +2,2 @@",  // only additions, no context lines
       "+new line 1",      //
       "+new line 2"},
      {"@@ -1,1 +2,1 @@",            // 0 lines of context, 1 line changed
       "-at first I was like whoa",  //
       "+and then I was like WHOA"},
      {"@@ -1,3 +2,4 @@",            // with 1 line of surrounding context
       " common line1",              //
       "-at first I was like whoa",  //
       "+and then I was like WHOA",  //
       "+  and then like whoa",      //
       " common line2"},
  };
  for (const auto &lines : kTestCases) {
    Hunk hunk;
    const LineRange range(lines.begin(), lines.end());
    const auto status = hunk.Parse(range);
    EXPECT_TRUE(status.ok()) << status.message();

    // Validate reversibility.
    std::ostringstream stream;
    stream << hunk;
    EXPECT_EQ(stream.str(), absl::StrJoin(lines, "\n") + "\n");
  }
}

TEST(HunkVerifyAgainstOriginalLinesTest, LineNumberOutOfBounds) {
  const std::vector<std::string_view> kHunkText = {
      {
          "@@ -2,3 +4,3 @@",  // dont' care about position in new-file
          " line2",           //
          "-line3",           //
          "+line pi",         //
          " line4",           // this line doesn't exist in original
      },
  };
  const std::vector<std::string_view> kOriginal = {
      "line1", "line2", "line3",
      // no line4
  };
  Hunk hunk;
  {
    const LineRange range(kHunkText.begin(), kHunkText.end());
    const auto status = hunk.Parse(range);
    ASSERT_TRUE(status.ok()) << status.message();
  }
  {
    const auto status = hunk.VerifyAgainstOriginalLines(kOriginal);
    EXPECT_EQ(status.code(), absl::StatusCode::kOutOfRange);
    EXPECT_TRUE(absl::StrContains(status.message(), "references line 4"));
    EXPECT_TRUE(absl::StrContains(status.message(), "with only 3 lines"));
  }
}

TEST(HunkVerifyAgainstOriginalLinesTest, InconsistentRetainedLine) {
  const std::vector<std::string_view> kHunkText = {
      {
          "@@ -2,2 +4,2 @@",
          " line2",    //
          "-line3",    //
          "+line pi",  //
      },
  };
  const std::vector<std::string_view> kOriginal = {
      "line1",
      "line2 different",
      "line3",
  };
  Hunk hunk;
  {
    const LineRange range(kHunkText.begin(), kHunkText.end());
    const auto status = hunk.Parse(range);
    ASSERT_TRUE(status.ok()) << status.message();
  }
  {
    const auto status = hunk.VerifyAgainstOriginalLines(kOriginal);
    EXPECT_EQ(status.code(), absl::StatusCode::kDataLoss);
    EXPECT_TRUE(absl::StrContains(status.message(),
                                  "Patch is inconsistent with original file"));
  }
}

TEST(HunkVerifyAgainstOriginalLinesTest, InconsistentDeletedLine) {
  const std::vector<std::string_view> kHunkText = {
      {
          "@@ -2,2 +4,2 @@",
          " line2",    //
          "-line3",    //
          "+line pi",  //
      },
  };
  const std::vector<std::string_view> kOriginal = {
      "line1",
      "line2",
      "line3 different",
  };
  Hunk hunk;
  {
    const LineRange range(kHunkText.begin(), kHunkText.end());
    const auto status = hunk.Parse(range);
    ASSERT_TRUE(status.ok()) << status.message();
  }
  {
    const auto status = hunk.VerifyAgainstOriginalLines(kOriginal);
    EXPECT_EQ(status.code(), absl::StatusCode::kDataLoss);
    EXPECT_TRUE(absl::StrContains(status.message(),
                                  "Patch is inconsistent with original file"));
  }
}

TEST(FilePatchParseTest, InvalidInputs) {
  const std::vector<std::string_view> kTestCases[] = {
      {},    // empty range is invalid
      {""},  // no "---" marker for source info
      {
          "--- /path/to/file.txt\t2020-03-30",  // no "+++" marker for source
                                                // info
      },
      {
          "--- /path/to/file.txt\t2020-03-29",
          "+++ /path/to/file.txt\t2020-03-30",
          "@@ -2,1 +3,1 @@",  // hunk line counts are inconsistent
      },
      {
          "--- /path/to/file.txt\t2020-03-29",
          "+++ /path/to/file.txt\t2020-03-30",
          "@@ -12,0 +13,0 @@",  // empty, but ok
          "@@ -42,1 +43,1 @@",  // hunk line counts are inconsistent
      },
      {
          "--- /path/to/file.txt\t2020-03-29",
          "+++ /path/to/file.txt\t2020-03-30",
          "@@ -2,1 +3,1 @@",
          "malformed line does not begin with [ -+]",
      },
  };
  for (const auto &lines : kTestCases) {
    const LineRange range(lines.begin(), lines.end());
    FilePatch file_patch;
    EXPECT_FALSE(file_patch.Parse(range).ok()) << "lines:\n"
                                               << absl::StrJoin(lines, "\n");
  }
}

TEST(FilePatchParseAndPrintTest, ValidInputs) {
  const std::vector<std::string_view> kTestCases[] = {
      {
          "--- /path/to/file.txt\t2020-03-30",
          "+++ /path/to/file.txt\t2020-03-30",
          // no hunks, but still valid
      },
      {
          "--- /path/to/file.txt\t2020-03-30",
          "+++ /path/to/file.txt\t2020-03-30",
          "@@ -12,0 +13,0 @@",  // empty, but ok
      },
      {
          "--- /path/to/file.txt\t2020-03-30",
          "+++ /path/to/file.txt\t2020-03-30",
          "@@ -12,1 +13,1 @@",
          " no change here",
      },
      {
          "--- /path/to/file.txt\t2020-03-30",
          "+++ /path/to/file.txt\t2020-03-30",
          "@@ -12,3 +13,2 @@",
          " no change here",
          "-delete me",
          " no change here",
      },
      {
          "--- /path/to/file.txt\t2020-03-30",
          "+++ /path/to/file.txt\t2020-03-30",
          "@@ -12,2 +13,3 @@",
          " no change here",
          "+add me",
          " no change here",
      },
      {
          "--- /path/to/file.txt\t2020-03-30",
          "+++ /path/to/file.txt\t2020-03-30",
          "@@ -12,3 +13,2 @@",  // first hunk
          " no change here",
          "-delete me",
          " no change here",
          "@@ -52,2 +53,3 @@",  // second hunk
          " no change here",
          "+add me",
          " no change here",
      },
      {
          // one line of file metadata
          ("==== //depot/p4/style/path/to/file.txt#4 - local/path/to/file.txt "
           "===="),
          "--- /path/to/file.txt\t2020-03-30",
          "+++ /path/to/file.txt\t2020-03-30",
          "@@ -12,1 +13,1 @@",
          " no change here",
      },
      {
          // one line of file metadata
          "diff -u a/path/to/file.txt b/path/to/file.txt",
          "--- /path/to/file.txt\t2020-03-30",
          "+++ /path/to/file.txt\t2020-03-30",
          "@@ -12,1 +13,1 @@",
          " no change here",
      },
  };
  for (const auto &lines : kTestCases) {
    const LineRange range(lines.begin(), lines.end());
    FilePatch file_patch;
    const auto status = file_patch.Parse(range);
    EXPECT_TRUE(status.ok()) << status.message();

    // Validate reversibility.
    std::ostringstream stream;
    stream << file_patch;
    EXPECT_EQ(stream.str(), absl::StrJoin(lines, "\n") + "\n");
  }
}

TEST(FilePatchIsNewFileTest, NewFile) {
  const std::vector<std::string_view> kInput = {
      "--- /dev/null\t2020-03-30",
      "+++ /path/to/file.txt\t2020-03-30",
      "@@ -0,0 +1,2 @@",
      "+new content 1",
      "+new content 2",
  };

  const LineRange range(kInput.begin(), kInput.end());
  FilePatch file_patch;
  const auto status = file_patch.Parse(range);
  EXPECT_TRUE(status.ok()) << status.message();
  EXPECT_TRUE(file_patch.IsNewFile());
}

TEST(FilePatchIsNewFileTest, ExistingFile) {
  const std::vector<std::string_view> kInput = {
      "--- /path/to/file.txt\t2020-03-30",
      "+++ /path/to/file.txt\t2020-03-30",
      "@@ -12,1 +13,1 @@",
      " no change here",
  };

  const LineRange range(kInput.begin(), kInput.end());
  FilePatch file_patch;
  const auto status = file_patch.Parse(range);
  EXPECT_TRUE(status.ok()) << status.message();
  EXPECT_FALSE(file_patch.IsNewFile());
}

TEST(FilePatchIsDeletedFileTest, DeletedFile) {
  const std::vector<std::string_view> kInput = {
      "--- /path/to/file.txt\t2020-03-30",
      "+++ /dev/null\t2020-03-30",
      "@@ -1,2 +0,0 @@",
      "-deleted content 1",
      "-deleted content 2",
  };

  const LineRange range(kInput.begin(), kInput.end());
  FilePatch file_patch;
  const auto status = file_patch.Parse(range);
  EXPECT_TRUE(status.ok()) << status.message();
  EXPECT_TRUE(file_patch.IsDeletedFile());
}

TEST(FilePatchIsDeletedFileTest, ExistingFile) {
  const std::vector<std::string_view> kInput = {
      "--- /path/to/file.txt\t2020-03-30",
      "+++ /path/to/file.txt\t2020-03-30",
      "@@ -12,2 +13,2 @@",
      " no change here",
      "+you win some",
      "-you lose some",
  };

  const LineRange range(kInput.begin(), kInput.end());
  FilePatch file_patch;
  const auto status = file_patch.Parse(range);
  EXPECT_TRUE(status.ok()) << status.message();
  EXPECT_FALSE(file_patch.IsDeletedFile());
}

TEST(FilePatchAddedLinesTest, Various) {
  const AddedLinesTestCase kTestCases[] = {
      {
          {
              "--- /path/to/file.txt\t2019-12-01",
              "+++ /path/to/file.txt\t2019-12-31",
              "@@ -12,1 +13,1 @@",
              " no change here",
          },
          {},
      },
      {
          {
              "--- /path/to/file.txt\t2019-12-01",
              "+++ /path/to/file.txt\t2019-12-31",
              "@@ -12,1 +13,1 @@",
              " no change here",
              "@@ -21,3 +20,2 @@",
              " ",
              "-bye",
              " ",
              "@@ -31,2 +43,4 @@",
              " ",
              "+hello",  // line 45
              "+world",  // line 46
              " ",
              "@@ -61,3 +80,3 @@",
              " ",
              "-adios",
              "+hola",  // line 81
              " ",
          },
          {{44, 46}, {81, 82}},
      },
  };
  for (const auto &test : kTestCases) {
    const LineRange range(test.hunk_text.begin(), test.hunk_text.end());
    FilePatch file_patch;
    const auto status = file_patch.Parse(range);
    ASSERT_TRUE(status.ok()) << status.message();
    EXPECT_EQ(file_patch.AddedLines(), test.expected_added_lines);
  }
}

class FilePatchPickApplyTest : public FilePatch, public ::testing::Test {
 protected:
  absl::Status ParseLines(const std::vector<std::string_view> &lines) {
    const LineRange range(lines.begin(), lines.end());
    return Parse(range);
  }

  static absl::StatusOr<std::string> NullFileReader(std::string_view filename) {
    return "";
  }

  static absl::Status NullFileWriter(std::string_view filename,
                                     std::string_view contents) {
    return absl::OkStatus();
  }
};

// Takes the place of a real file on the filesystem.
// TODO(fangism): move this to a "file_test_util" library.
struct StringFile {
  const std::string_view path;
  const std::string_view contents;
};

class StringFileSequence {
 public:
  explicit StringFileSequence(const std::vector<StringFile> &files)
      : files_(files) {}
  StringFileSequence(std::initializer_list<StringFile> files) : files_(files) {}

 protected:
  const std::vector<StringFile> files_;
  // stateful value needed inside std::function to emulate a sequence of calls
  size_t index_ = 0;
};

// Functor that can mimic a sequence of calls to file::GetContents()
// Can be passed anywhere that takes a FileReaderFunction.
// TODO(fangism): move this to a "file_test_util" library.
struct ReadStringFileSequence : public StringFileSequence {
  explicit ReadStringFileSequence(const std::vector<StringFile> &files)
      : StringFileSequence(files) {}
  ReadStringFileSequence(std::initializer_list<StringFile> files)
      : StringFileSequence(files) {}

  // like file::GetContentAsString()
  absl::StatusOr<std::string> operator()(std::string_view filename) {
    // ASSERT_LT(i, files_.size());  // can't use ASSERT_* which returns void
    if (index_ >= files_.size()) {
      return absl::OutOfRangeError(
          absl::StrCat("No more files to read beyond index=", index_));
    }
    const auto &file = files_[index_];
    EXPECT_EQ(filename, file.path) << " at index " << index_;
    ++index_;
    return std::string{file.contents};
  }
};

// Functor that can mimic a sequence of calls to file::SetContents()
// Can be passed anywhere that takes a FileWriterFunction.
// TODO(fangism): move this to a "file_test_util" library.
struct ExpectStringFileSequence : public StringFileSequence {
  explicit ExpectStringFileSequence(const std::vector<StringFile> &files)
      : StringFileSequence(files) {}
  ExpectStringFileSequence(std::initializer_list<StringFile> files)
      : StringFileSequence(files) {}

  // like file::SetContents()
  absl::Status operator()(std::string_view filename, std::string_view src) {
    // ASSERT_LT(i, files_.size());  // can't use ASSERT_* which returns void
    if (index_ >= files_.size()) {
      return absl::OutOfRangeError(
          absl::StrCat("No more files to compare beyond index=", index_));
    }
    const auto &file = files_[index_];
    EXPECT_EQ(filename, file.path) << " at index " << index_;
    EXPECT_EQ(file.contents, src) << " at index " << index_;
    ++index_;
    return absl::OkStatus();  // "file" is successfully written
  }
};

TEST_F(FilePatchPickApplyTest, ErrorReadingFile) {
  std::istringstream ins;
  std::ostringstream outs;
  constexpr std::string_view kErrorMessage = "File not found.";
  auto error_file_reader = [=](std::string_view filename) {
    return absl::StatusOr<std::string>(absl::NotFoundError(kErrorMessage));
  };
  const auto status = PickApply(ins, outs, error_file_reader, &NullFileWriter);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.message(), kErrorMessage);
  EXPECT_TRUE(outs.str().empty()) << "Unexpected: " << outs.str();
}

TEST_F(FilePatchPickApplyTest, IgnoreNewFile) {
  {
    const std::vector<std::string_view> kHunkText{
        "--- /dev/null\t2012-01-01",
        "+++ foo.txt\t2012-01-01",
    };
    const auto status = ParseLines(kHunkText);
    ASSERT_TRUE(status.ok()) << status.message();
  }
  std::istringstream ins;
  std::ostringstream outs;
  const auto status = PickApply(ins, outs, &NullFileReader, &NullFileWriter);
  EXPECT_TRUE(status.ok()) << "Got: " << status.message();
  EXPECT_TRUE(outs.str().empty()) << "Unexpected: " << outs.str();
}

TEST_F(FilePatchPickApplyTest, IgnoreDeletedFile) {
  {
    const std::vector<std::string_view> kHunkText{
        "--- bar.txt\t2012-01-01",
        "+++ /dev/null\t2012-01-01",
    };
    const auto status = ParseLines(kHunkText);
    ASSERT_TRUE(status.ok()) << status.message();
  }
  std::istringstream ins;
  std::ostringstream outs;
  const auto status = PickApply(ins, outs, &NullFileReader, &NullFileWriter);
  EXPECT_TRUE(status.ok()) << "Got: " << status.message();
  EXPECT_TRUE(outs.str().empty()) << "Unexpected: " << outs.str();
}

TEST_F(FilePatchPickApplyTest, EmptyPatchNoPrompt) {
  {
    const std::vector<std::string_view> kHunkText{
        "--- foo.txt\t2012-01-01",
        "+++ foo-formatted.txt\t2012-01-01",
    };
    const auto status = ParseLines(kHunkText);
    ASSERT_TRUE(status.ok()) << status.message();
  }

  std::istringstream ins;
  std::ostringstream outs;

  constexpr std::string_view kOriginal = "aaa\nbbb\nccc\n";
  constexpr std::string_view kExpected = kOriginal;  // no change

  const auto status =
      PickApply(ins, outs,  //
                internal::ReadStringFileSequence({{"foo.txt", kOriginal}}),
                internal::ExpectStringFileSequence({{"foo.txt", kExpected}}));
  EXPECT_TRUE(status.ok()) << "Got: " << status.message();
  EXPECT_TRUE(outs.str().empty()) << "Unexpected: " << outs.str();
}

TEST_F(FilePatchPickApplyTest, ErrorWritingFileInPlace) {
  {
    const std::vector<std::string_view> kHunkText{
        "--- foo.txt\t2012-01-01",
        "+++ foo-formatted.txt\t2012-01-01",
    };
    const auto status = ParseLines(kHunkText);
    ASSERT_TRUE(status.ok()) << status.message();
  }

  std::istringstream ins;
  std::ostringstream outs;

  constexpr std::string_view kOriginal = "aaa\nbbb\nccc\n";
  constexpr std::string_view kErrorMessage = "Cannot write file.";

  auto error_file_writer = [=](std::string_view, std::string_view) {
    return absl::PermissionDeniedError(kErrorMessage);
  };

  const auto status = PickApply(
      ins, outs,  //
      ReadStringFileSequence({{"foo.txt", kOriginal}}), error_file_writer);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.message(), kErrorMessage);
  EXPECT_TRUE(outs.str().empty()) << "Unexpected: " << outs.str();
}

TEST_F(FilePatchPickApplyTest, OneHunkNotApplied) {
  {
    const std::vector<std::string_view> kHunkText{
        "--- foo.txt\t2012-01-01",
        "+++ foo-formatted.txt\t2012-01-01",
        "@@ -2,3 +2,2 @@",
        " bbb",
        "-ccc",  // patch proposes to delete this line
        " ddd",
    };
    const auto status = ParseLines(kHunkText);
    ASSERT_TRUE(status.ok()) << status.message();
  }

  std::istringstream ins("n\n");  // user declines patch hunk
  std::ostringstream outs;

  constexpr std::string_view kOriginal =
      "aaa\n"
      "bbb\n"
      "ccc\n"
      "ddd\n"
      "eee\n";
  constexpr std::string_view kExpected = kOriginal;

  const auto status =
      PickApply(ins, outs,  //
                ReadStringFileSequence({{"foo.txt", kOriginal}}),
                ExpectStringFileSequence({{"foo.txt", kExpected}}));
  EXPECT_TRUE(status.ok()) << "Got: " << status.message();
  EXPECT_FALSE(outs.str().empty());
}

TEST_F(FilePatchPickApplyTest, PatchInconsistentWithOriginalText) {
  {
    const std::vector<std::string_view> kHunkText{
        "--- foo.txt\t2012-01-01",
        "+++ foo-formatted.txt\t2012-01-01",
        "@@ -2,3 +2,2 @@",
        " bbb",  // inconsistent with original
        "-ccc",  // patch proposes to delete this line
        " ddd",
    };
    const auto status = ParseLines(kHunkText);
    ASSERT_TRUE(status.ok()) << status.message();
  }

  std::istringstream ins("y\n");  // user accepts hunk
  std::ostringstream outs;

  constexpr std::string_view kOriginal =
      "aaa\n"
      "bbb-different\n"  // inconsistent with patch
      "ccc\n"            // deleted by hunk
      "ddd\n"
      "eee\n";
  constexpr std::string_view kExpected =
      "aaa\n"
      "bbb\n"
      "ddd\n"
      "eee\n";

  const auto status =
      PickApply(ins, outs,  //
                ReadStringFileSequence({{"foo.txt", kOriginal}}),
                ExpectStringFileSequence({{"foo.txt", kExpected}}));
  EXPECT_EQ(status.code(), absl::StatusCode::kDataLoss);
}

TEST_F(FilePatchPickApplyTest, OneDeletionAccepted) {
  {
    const std::vector<std::string_view> kHunkText{
        "--- foo.txt\t2012-01-01",
        "+++ foo-formatted.txt\t2012-01-01",
        "@@ -2,3 +2,2 @@",
        " bbb",
        "-ccc",  // patch proposes to delete this line
        " ddd",
    };
    const auto status = ParseLines(kHunkText);
    ASSERT_TRUE(status.ok()) << status.message();
  }

  std::istringstream ins("y\n");  // user accepts hunk
  std::ostringstream outs;

  constexpr std::string_view kOriginal =
      "aaa\n"
      "bbb\n"
      "ccc\n"  // deleted by hunk
      "ddd\n"
      "eee\n";
  constexpr std::string_view kExpected =
      "aaa\n"
      "bbb\n"
      "ddd\n"
      "eee\n";

  const auto status =
      PickApply(ins, outs,  //
                ReadStringFileSequence({{"foo.txt", kOriginal}}),
                ExpectStringFileSequence({{"foo.txt", kExpected}}));
  EXPECT_TRUE(status.ok()) << "Got: " << status.message();
  EXPECT_FALSE(outs.str().empty());
}

TEST_F(FilePatchPickApplyTest, OneInsertionAccepted) {
  {
    const std::vector<std::string_view> kHunkText{
        "--- foo.txt\t2012-01-01",
        "+++ foo-formatted.txt\t2012-01-01",
        "@@ -2,2 +2,3 @@",
        " bbb",
        "+bbb.5",  // patch proposes to insert this line
        " ccc",
    };
    const auto status = ParseLines(kHunkText);
    ASSERT_TRUE(status.ok()) << status.message();
  }

  std::istringstream ins("y\n");  // user accepts hunk
  std::ostringstream outs;

  constexpr std::string_view kOriginal =
      "aaa\n"
      "bbb\n"
      "ccc\n"
      "ddd\n"
      "eee\n";
  constexpr std::string_view kExpected =
      "aaa\n"
      "bbb\n"
      "bbb.5\n"  // inserted
      "ccc\n"
      "ddd\n"
      "eee\n";

  const auto status =
      PickApply(ins, outs,  //
                ReadStringFileSequence({{"foo.txt", kOriginal}}),
                ExpectStringFileSequence({{"foo.txt", kExpected}}));
  EXPECT_TRUE(status.ok()) << "Got: " << status.message();
  EXPECT_FALSE(outs.str().empty());
}

TEST_F(FilePatchPickApplyTest, OneReplacementAccepted) {
  {
    const std::vector<std::string_view> kHunkText{
        "--- foo.txt\t2012-01-01",
        "+++ foo-formatted.txt\t2012-01-01",
        "@@ -2,3 +2,3 @@",
        " bbb",
        "-ccc",  // patch proposes to replace this line
        "+C++",  // with this line
        " ddd",
    };
    const auto status = ParseLines(kHunkText);
    ASSERT_TRUE(status.ok()) << status.message();
  }

  std::istringstream ins("y\n");  // user accepts hunk
  std::ostringstream outs;

  constexpr std::string_view kOriginal =
      "aaa\n"
      "bbb\n"
      "ccc\n"  // replaced
      "ddd\n"
      "eee\n";
  constexpr std::string_view kExpected =
      "aaa\n"
      "bbb\n"
      "C++\n"  // replaced
      "ddd\n"
      "eee\n";

  const auto status =
      PickApply(ins, outs,  //
                ReadStringFileSequence({{"foo.txt", kOriginal}}),
                ExpectStringFileSequence({{"foo.txt", kExpected}}));
  EXPECT_TRUE(status.ok()) << "Got: " << status.message();
  EXPECT_FALSE(outs.str().empty());
}

TEST_F(FilePatchPickApplyTest, HelpFirstThenAcceptHunk) {
  {
    const std::vector<std::string_view> kHunkText{
        "--- foo.txt\t2012-01-01",
        "+++ foo-formatted.txt\t2012-01-01",
        "@@ -2,3 +2,3 @@",
        " bbb",
        "-ccc",  // patch proposes to replace this line
        "+C++",  // with this line
        " ddd",
    };
    const auto status = ParseLines(kHunkText);
    ASSERT_TRUE(status.ok()) << status.message();
  }

  std::istringstream ins("?\ny\n");  // help, accept
  std::ostringstream outs;

  constexpr std::string_view kOriginal =
      "aaa\n"
      "bbb\n"
      "ccc\n"  // replaced
      "ddd\n"
      "eee\n";
  constexpr std::string_view kExpected =
      "aaa\n"
      "bbb\n"
      "C++\n"  // replaced
      "ddd\n"
      "eee\n";

  const auto status =
      PickApply(ins, outs,  //
                ReadStringFileSequence({{"foo.txt", kOriginal}}),
                ExpectStringFileSequence({{"foo.txt", kExpected}}));
  EXPECT_TRUE(status.ok()) << "Got: " << status.message();
  EXPECT_FALSE(outs.str().empty());
  EXPECT_TRUE(absl::StrContains(outs.str(), "print this help"));
}

TEST_F(FilePatchPickApplyTest, HunksOutOfOrder) {
  {
    const std::vector<std::string_view> kHunkText{
        "--- foo.txt\t2012-01-01",
        "+++ foo-formatted.txt\t2012-01-01",
        "@@ -5,3 +5,3 @@",
        " eee",
        "-fff",
        "+fangism",
        " ggg",
        "@@ -2,3 +2,3 @@",  // bad ordering
        " bbb",
        "-ccc",  // patch proposes to replace this line
        "+C++",  // with this line
        " ddd",
    };
    const auto status = ParseLines(kHunkText);
    ASSERT_TRUE(status.ok()) << status.message();
  }

  std::istringstream ins("y\nn\n");  // accept, reject
  std::ostringstream outs;

  constexpr std::string_view kOriginal =
      "aaa\n"
      "bbb\n"
      "ccc\n"  // replaced
      "ddd\n"
      "eee\n"
      "fff\n"
      "ggg\n";
  constexpr std::string_view kExpected =
      "aaa\n"
      "bbb\n"
      "C++\n"  // replaced
      "ddd\n"
      "eee\n"
      "fff\n"
      "ggg\n";

  const auto status =
      PickApply(ins, outs,  //
                ReadStringFileSequence({{"foo.txt", kOriginal}}),
                ExpectStringFileSequence({{"foo.txt", kExpected}}));
  EXPECT_FALSE(status.ok());
  EXPECT_TRUE(absl::StrContains(status.message(), "not properly ordered"));
}

TEST_F(FilePatchPickApplyTest, AcceptOnlyFirstOfTwoHunks) {
  {
    const std::vector<std::string_view> kHunkText{
        "--- foo.txt\t2012-01-01",
        "+++ foo-formatted.txt\t2012-01-01",
        "@@ -2,3 +2,3 @@",
        " bbb",
        "-ccc",  // patch proposes to replace this line
        "+C++",  // with this line
        " ddd",
        "@@ -5,3 +5,3 @@",
        " eee",
        "-fff",
        "+fangism",
        " ggg",
    };
    const auto status = ParseLines(kHunkText);
    ASSERT_TRUE(status.ok()) << status.message();
  }

  std::istringstream ins("y\nn\n");  // accept, reject
  std::ostringstream outs;

  constexpr std::string_view kOriginal =
      "aaa\n"
      "bbb\n"
      "ccc\n"  // replaced
      "ddd\n"
      "eee\n"
      "fff\n"
      "ggg\n";
  constexpr std::string_view kExpected =
      "aaa\n"
      "bbb\n"
      "C++\n"  // replaced
      "ddd\n"
      "eee\n"
      "fff\n"
      "ggg\n";

  const auto status =
      PickApply(ins, outs,  //
                ReadStringFileSequence({{"foo.txt", kOriginal}}),
                ExpectStringFileSequence({{"foo.txt", kExpected}}));
  EXPECT_TRUE(status.ok()) << "Got: " << status.message();
  EXPECT_FALSE(outs.str().empty());
}

TEST_F(FilePatchPickApplyTest, AcceptOnlySecondOfTwoHunks) {
  {
    const std::vector<std::string_view> kHunkText{
        "--- foo.txt\t2012-01-01",
        "+++ foo-formatted.txt\t2012-01-01",
        "@@ -2,3 +2,3 @@",
        " bbb",
        "-ccc",  // patch proposes to replace this line
        "+C++",  // with this line
        " ddd",
        "@@ -5,3 +5,3 @@",
        " eee",
        "-fff",
        "+fangism",
        " ggg",
    };
    const auto status = ParseLines(kHunkText);
    ASSERT_TRUE(status.ok()) << status.message();
  }

  std::istringstream ins("n\ny\n");  // reject, accept
  std::ostringstream outs;

  constexpr std::string_view kOriginal =
      "aaa\n"
      "bbb\n"
      "ccc\n"  // kept
      "ddd\n"
      "eee\n"
      "fff\n"  // changed
      "ggg\n";
  constexpr std::string_view kExpected =
      "aaa\n"
      "bbb\n"
      "ccc\n"  // kept
      "ddd\n"
      "eee\n"
      "fangism\n"  // changed
      "ggg\n";

  const auto status =
      PickApply(ins, outs,  //
                ReadStringFileSequence({{"foo.txt", kOriginal}}),
                ExpectStringFileSequence({{"foo.txt", kExpected}}));
  EXPECT_TRUE(status.ok()) << "Got: " << status.message();
  EXPECT_FALSE(outs.str().empty());
}

TEST_F(FilePatchPickApplyTest, SplitThenAcceptOnlyFirstOfTwoHunks) {
  {
    const std::vector<std::string_view> kHunkText{
        "--- foo.txt\t2012-01-01",
        "+++ foo-formatted.txt\t2012-01-01",
        "@@ -2,6 +2,6 @@",  // one large splittable hunk
        " bbb",
        "-ccc",  // elect to apply
        "+C++",  // this change
        " ddd",
        " eee",
        "-fff",      // but not
        "+fangism",  // this change
        " ggg",
    };
    const auto status = ParseLines(kHunkText);
    ASSERT_TRUE(status.ok()) << status.message();
  }

  std::istringstream ins("s\ny\nn\n");  // split, accept, reject
  std::ostringstream outs;

  constexpr std::string_view kOriginal =
      "aaa\n"
      "bbb\n"
      "ccc\n"  // replaced
      "ddd\n"
      "eee\n"
      "fff\n"
      "ggg\n";
  constexpr std::string_view kExpected =
      "aaa\n"
      "bbb\n"
      "C++\n"  // replaced
      "ddd\n"
      "eee\n"
      "fff\n"
      "ggg\n";

  const auto status =
      PickApply(ins, outs,  //
                ReadStringFileSequence({{"foo.txt", kOriginal}}),
                ExpectStringFileSequence({{"foo.txt", kExpected}}));
  EXPECT_TRUE(status.ok()) << "Got: " << status.message();
  EXPECT_FALSE(outs.str().empty());
}

TEST_F(FilePatchPickApplyTest, SplitThenAcceptOnlySecondOfTwoHunks) {
  {
    const std::vector<std::string_view> kHunkText{
        "--- foo.txt\t2012-01-01",
        "+++ foo-formatted.txt\t2012-01-01",
        "@@ -2,6 +2,6 @@",  // one large splittable hunk
        " bbb",
        "-ccc",  // elect to reject
        "+C++",  // this change
        " ddd",
        " eee",
        "-fff",      // but accept
        "+fangism",  // this change
        " ggg",
    };
    const auto status = ParseLines(kHunkText);
    ASSERT_TRUE(status.ok()) << status.message();
  }

  std::istringstream ins("s\nn\ny\n");  // split, reject, accept
  std::ostringstream outs;

  constexpr std::string_view kOriginal =
      "aaa\n"
      "bbb\n"
      "ccc\n"  // kept
      "ddd\n"
      "eee\n"
      "fff\n"  // replaced
      "ggg\n";
  constexpr std::string_view kExpected =
      "aaa\n"
      "bbb\n"
      "ccc\n"  // kept
      "ddd\n"
      "eee\n"
      "fangism\n"  // replaced
      "ggg\n";

  const auto status =
      PickApply(ins, outs,  //
                ReadStringFileSequence({{"foo.txt", kOriginal}}),
                ExpectStringFileSequence({{"foo.txt", kExpected}}));
  EXPECT_TRUE(status.ok()) << "Got: " << status.message();
  EXPECT_FALSE(outs.str().empty());
}

TEST_F(FilePatchPickApplyTest, AbortRightAway) {
  {
    const std::vector<std::string_view> kHunkText{
        "--- foo.txt\t2012-01-01",
        "+++ foo-formatted.txt\t2012-01-01",
        "@@ -2,3 +2,3 @@",
        " bbb",
        "-ccc",  // patch proposes to replace this line
        "+C++",  // with this line
        " ddd",
        "@@ -5,3 +5,3 @@",
        " eee",
        "-fff",
        "+fangism",
        " ggg",
    };
    const auto status = ParseLines(kHunkText);
    ASSERT_TRUE(status.ok()) << status.message();
  }

  std::istringstream ins("q\n");  // quit (abandon)
  std::ostringstream outs;

  constexpr std::string_view kOriginal =
      "aaa\n"
      "bbb\n"
      "ccc\n"
      "ddd\n"
      "eee\n"
      "fff\n"
      "ggg\n";
  constexpr std::string_view kExpected = kOriginal;

  const auto status =
      PickApply(ins, outs,  //
                ReadStringFileSequence({{"foo.txt", kOriginal}}),
                ExpectStringFileSequence({{"foo.txt", kExpected}}));
  EXPECT_TRUE(status.ok()) << "Got: " << status.message();
  EXPECT_FALSE(outs.str().empty());
}

TEST_F(FilePatchPickApplyTest, TreatEndOfUserInputAsAbort) {
  {
    const std::vector<std::string_view> kHunkText{
        "--- foo.txt\t2012-01-01",
        "+++ foo-formatted.txt\t2012-01-01",
        "@@ -2,3 +2,3 @@",
        " bbb",
        "-ccc",  // patch proposes to replace this line
        "+C++",  // with this line
        " ddd",
        "@@ -5,3 +5,3 @@",
        " eee",
        "-fff",
        "+fangism",
        " ggg",
    };
    const auto status = ParseLines(kHunkText);
    ASSERT_TRUE(status.ok()) << status.message();
  }

  std::istringstream ins;  // empty, end of user input
  std::ostringstream outs;

  constexpr std::string_view kOriginal =
      "aaa\n"
      "bbb\n"
      "ccc\n"
      "ddd\n"
      "eee\n"
      "fff\n"
      "ggg\n";
  constexpr std::string_view kExpected = kOriginal;

  const auto status =
      PickApply(ins, outs,  //
                ReadStringFileSequence({{"foo.txt", kOriginal}}),
                ExpectStringFileSequence({{"foo.txt", kExpected}}));
  EXPECT_TRUE(status.ok()) << "Got: " << status.message();
  EXPECT_FALSE(outs.str().empty());
}

TEST_F(FilePatchPickApplyTest, AbortFileAfterAcceptingOneHunk) {
  {
    const std::vector<std::string_view> kHunkText{
        "--- foo.txt\t2012-01-01",
        "+++ foo-formatted.txt\t2012-01-01",
        "@@ -2,3 +2,3 @@",
        " bbb",
        "-ccc",  // patch proposes to replace this line
        "+C++",  // with this line
        " ddd",
        "@@ -5,3 +5,3 @@",
        " eee",
        "-fff",
        "+fangism",
        " ggg",
    };
    const auto status = ParseLines(kHunkText);
    ASSERT_TRUE(status.ok()) << status.message();
  }

  std::istringstream ins("y\nq\n");  // accept, quit (abandon)
  std::ostringstream outs;

  constexpr std::string_view kOriginal =
      "aaa\n"
      "bbb\n"
      "ccc\n"  // replaced
      "ddd\n"
      "eee\n"
      "fff\n"
      "ggg\n";
  constexpr std::string_view kExpected = kOriginal;

  const auto status =
      PickApply(ins, outs,  //
                ReadStringFileSequence({{"foo.txt", kOriginal}}),
                ExpectStringFileSequence({{"foo.txt", kExpected}}));
  EXPECT_TRUE(status.ok()) << "Got: " << status.message();
  EXPECT_FALSE(outs.str().empty());
}

TEST_F(FilePatchPickApplyTest, AcceptTwoDeletions) {
  {
    const std::vector<std::string_view> kHunkText{
        "--- foo.txt\t2012-01-01",
        "+++ foo-formatted.txt\t2012-01-01",
        "@@ -2,3 +2,2 @@",
        " bbb",
        "-ccc",  // delete this line
        " ddd",
        "@@ -6,3 +5,2 @@",
        " fff",
        "-ggg",  // delete this line
        " hhh",
    };
    const auto status = ParseLines(kHunkText);
    ASSERT_TRUE(status.ok()) << status.message();
  }

  std::istringstream ins("y\ny\n");  // accept, accept
  std::ostringstream outs;

  constexpr std::string_view kOriginal =
      "aaa\n"
      "bbb\n"
      "ccc\n"
      "ddd\n"
      "eee\n"
      "fff\n"
      "ggg\n"
      "hhh\n";
  constexpr std::string_view kExpected =
      "aaa\n"
      "bbb\n"
      "ddd\n"
      "eee\n"
      "fff\n"
      "hhh\n";

  const auto status =
      PickApply(ins, outs,  //
                ReadStringFileSequence({{"foo.txt", kOriginal}}),
                ExpectStringFileSequence({{"foo.txt", kExpected}}));
  EXPECT_TRUE(status.ok()) << "Got: " << status.message();
  EXPECT_FALSE(outs.str().empty());
}

TEST_F(FilePatchPickApplyTest, AcceptAllDeletions) {
  {
    const std::vector<std::string_view> kHunkText{
        "--- foo.txt\t2012-01-01",
        "+++ foo-formatted.txt\t2012-01-01",
        "@@ -2,3 +2,2 @@",
        " bbb",
        "-ccc",  // delete this line
        " ddd",
        "@@ -6,3 +5,2 @@",
        " fff",
        "-ggg",  // delete this line
        " hhh",
    };
    const auto status = ParseLines(kHunkText);
    ASSERT_TRUE(status.ok()) << status.message();
  }

  std::istringstream ins("a\n");  // accept all
  std::ostringstream outs;

  constexpr std::string_view kOriginal =
      "aaa\n"
      "bbb\n"
      "ccc\n"
      "ddd\n"
      "eee\n"
      "fff\n"
      "ggg\n"
      "hhh\n";
  constexpr std::string_view kExpected =
      "aaa\n"
      "bbb\n"
      "ddd\n"
      "eee\n"
      "fff\n"
      "hhh\n";

  const auto status =
      PickApply(ins, outs,  //
                ReadStringFileSequence({{"foo.txt", kOriginal}}),
                ExpectStringFileSequence({{"foo.txt", kExpected}}));
  EXPECT_TRUE(status.ok()) << "Got: " << status.message();
  EXPECT_FALSE(outs.str().empty());
}

TEST_F(FilePatchPickApplyTest, RejectAllDeletions) {
  {
    const std::vector<std::string_view> kHunkText{
        "--- foo.txt\t2012-01-01",
        "+++ foo-formatted.txt\t2012-01-01",
        "@@ -2,3 +2,2 @@",
        " bbb",
        "-ccc",  // delete this line
        " ddd",
        "@@ -6,3 +5,2 @@",
        " fff",
        "-ggg",  // delete this line
        " hhh",
    };
    const auto status = ParseLines(kHunkText);
    ASSERT_TRUE(status.ok()) << status.message();
  }

  std::istringstream ins("d\n");  // accept all
  std::ostringstream outs;

  constexpr std::string_view kOriginal =
      "aaa\n"
      "bbb\n"
      "ccc\n"
      "ddd\n"
      "eee\n"
      "fff\n"
      "ggg\n"
      "hhh\n";
  constexpr std::string_view kExpected = kOriginal;  // no changes

  const auto status =
      PickApply(ins, outs,  //
                ReadStringFileSequence({{"foo.txt", kOriginal}}),
                ExpectStringFileSequence({{"foo.txt", kExpected}}));
  EXPECT_TRUE(status.ok()) << "Got: " << status.message();
  EXPECT_FALSE(outs.str().empty());
}

TEST_F(FilePatchPickApplyTest, AcceptTwoInsertions) {
  {
    const std::vector<std::string_view> kHunkText{
        "--- foo.txt\t2012-01-01",
        "+++ foo-formatted.txt\t2012-01-01",
        "@@ -2,2 +2,3 @@",
        " bbb",
        "+ccc",  // delete this line
        " ddd",
        "@@ -5,2 +6,3 @@",
        " fff",
        "+ggg",  // delete this line
        " hhh",
    };
    const auto status = ParseLines(kHunkText);
    ASSERT_TRUE(status.ok()) << status.message();
  }

  std::istringstream ins("y\ny\n");  // accept, accept
  std::ostringstream outs;

  constexpr std::string_view kOriginal =
      "aaa\n"
      "bbb\n"
      "ddd\n"
      "eee\n"
      "fff\n"
      "hhh\n";
  constexpr std::string_view kExpected =
      "aaa\n"
      "bbb\n"
      "ccc\n"
      "ddd\n"
      "eee\n"
      "fff\n"
      "ggg\n"
      "hhh\n";

  const auto status =
      PickApply(ins, outs,  //
                ReadStringFileSequence({{"foo.txt", kOriginal}}),
                ExpectStringFileSequence({{"foo.txt", kExpected}}));
  EXPECT_TRUE(status.ok()) << "Got: " << status.message();
  EXPECT_FALSE(outs.str().empty());
}

TEST_F(FilePatchPickApplyTest, AcceptAllInsertions) {
  {
    const std::vector<std::string_view> kHunkText{
        "--- foo.txt\t2012-01-01",
        "+++ foo-formatted.txt\t2012-01-01",
        "@@ -2,2 +2,3 @@",
        " bbb",
        "+ccc",  // delete this line
        " ddd",
        "@@ -5,2 +6,3 @@",
        " fff",
        "+ggg",  // delete this line
        " hhh",
    };
    const auto status = ParseLines(kHunkText);
    ASSERT_TRUE(status.ok()) << status.message();
  }

  std::istringstream ins("a\n");  // accept all
  std::ostringstream outs;

  constexpr std::string_view kOriginal =
      "aaa\n"
      "bbb\n"
      "ddd\n"
      "eee\n"
      "fff\n"
      "hhh\n";
  constexpr std::string_view kExpected =
      "aaa\n"
      "bbb\n"
      "ccc\n"
      "ddd\n"
      "eee\n"
      "fff\n"
      "ggg\n"
      "hhh\n";

  const auto status =
      PickApply(ins, outs,  //
                ReadStringFileSequence({{"foo.txt", kOriginal}}),
                ExpectStringFileSequence({{"foo.txt", kExpected}}));
  EXPECT_TRUE(status.ok()) << "Got: " << status.message();
  EXPECT_FALSE(outs.str().empty());
}

TEST_F(FilePatchPickApplyTest, RejectAllInsertions) {
  {
    const std::vector<std::string_view> kHunkText{
        "--- foo.txt\t2012-01-01",
        "+++ foo-formatted.txt\t2012-01-01",
        "@@ -2,2 +2,3 @@",
        " bbb",
        "+ccc",  // delete this line
        " ddd",
        "@@ -5,2 +6,3 @@",
        " fff",
        "+ggg",  // delete this line
        " hhh",
    };
    const auto status = ParseLines(kHunkText);
    ASSERT_TRUE(status.ok()) << status.message();
  }

  std::istringstream ins("d\n");  // reject all
  std::ostringstream outs;

  constexpr std::string_view kOriginal =
      "aaa\n"
      "bbb\n"
      "ddd\n"
      "eee\n"
      "fff\n"
      "hhh\n";
  constexpr std::string_view kExpected = kOriginal;

  const auto status =
      PickApply(ins, outs,  //
                ReadStringFileSequence({{"foo.txt", kOriginal}}),
                ExpectStringFileSequence({{"foo.txt", kExpected}}));
  EXPECT_TRUE(status.ok()) << "Got: " << status.message();
  EXPECT_FALSE(outs.str().empty());
}

}  // namespace
}  // namespace internal

namespace {
// public interface tests

TEST(PatchSetParseTest, InvalidInputs) {
  constexpr std::string_view kTestCases[] = {
      // no "+++" marker for source
      "--- /path/to/file.txt\t2020-03-30\n",
      // hunk line counts are inconsistent
      "--- /path/to/file.txt\t2020-03-29\n"
      "+++ /path/to/file.txt\t2020-03-30\n"
      "@@ -2,1 +3,1 @@\n",
      // second hunk line counts are inconsistent
      "--- /path/to/file.txt\t2020-03-29\n"
      "+++ /path/to/file.txt\t2020-03-30\n"
      "@@ -12,0 +13,0 @@\n"
      "@@ -42,1 +43,1 @@\n",
      // malformed hunk marked-line
      "--- /path/to/file.txt\t2020-03-29\n"
      "+++ /path/to/file.txt\t2020-03-30\n"
      "@@ -2,1 +3,1 @@\n"
      "malformed line does not begin with [ -+]",
  };
  for (const auto &patch_contents : kTestCases) {
    PatchSet patch_set;
    EXPECT_FALSE(patch_set.Parse(patch_contents).ok()) << "contents:\n"
                                                       << patch_contents;
  }
}

TEST(PatchSetParseAndPrintTest, ValidInputs) {
  constexpr std::string_view kTestCases[] = {
      // no metadata here
      "--- /path/to/file.txt\t2020-03-30\n"
      "+++ /path/to/file.txt\t2020-03-30\n"
      "@@ -12,3 +13,2 @@\n"
      " no change here\n"
      "-delete me\n"
      " no change here\n"
      "@@ -52,2 +53,3 @@\n"
      " no change here\n"
      "+add me\n"
      " no change here\n",
      // with patchset metadata here
      "metadata\n"
      "From: hobbit@fryingpan.org\n"
      "To: hobbit@fire.org\n"
      "metadata\n"
      "\n"
      "--- /path/to/file.txt\t2020-03-30\n"
      "+++ /path/to/file.txt\t2020-03-30\n"
      "@@ -12,3 +13,2 @@\n"
      " no change here\n"
      "-delete me\n"
      " no change here\n"
      "@@ -52,2 +53,3 @@\n"
      " no change here\n"
      "+add me\n"
      " no change here\n",
      // with file metadata in two files
      "diff -u /path/to/file1.txt local/path/to/file1.txt\n"
      "--- /path/to/file1.txt\t2020-03-30\n"
      "+++ /path/to/file1.txt\t2020-03-30\n"
      "@@ -12,3 +13,2 @@\n"
      " no change here\n"
      "-delete me\n"
      " no change here\n"
      "diff -u /path/to/file2.txt local/path/to/file2.txt\n"
      "--- /path/to/file2.txt\t2020-03-30\n"
      "+++ /path/to/file2.txt\t2020-03-30\n"
      "@@ -52,2 +53,3 @@\n"
      " no change here\n"
      "+add me\n"
      " no change here\n",
      // with patchset metadata and file metadata in two files
      "From: author@fryingpan.org\n"
      "To: reviewer@fire.org\n"
      "\n"
      "diff -u /path/to/file1.txt local/path/to/file1.txt\n"
      "--- /path/to/file1.txt\t2020-03-30\n"
      "+++ /path/to/file1.txt\t2020-03-30\n"
      "@@ -12,3 +13,2 @@\n"
      " no change here\n"
      "-delete me\n"
      " no change here\n"
      "diff -u /path/to/file2.txt local/path/to/file2.txt\n"
      "--- /path/to/file2.txt\t2020-03-30\n"
      "+++ /path/to/file2.txt\t2020-03-30\n"
      "@@ -52,2 +53,3 @@\n"
      " no change here\n"
      "+add me\n"
      " no change here\n",
  };

  for (const auto &patch_contents : kTestCases) {
    PatchSet patch_set;
    const auto status = patch_set.Parse(patch_contents);
    EXPECT_TRUE(status.ok()) << status.message();

    // Validate reversibility.
    std::ostringstream stream;
    stream << patch_set;
    EXPECT_EQ(stream.str(), patch_contents);
  }
}

TEST(PatchSetAddedLinesMapTest, NewAndExistingFile) {
  constexpr std::string_view patch_contents =  //
      "diff -u /dev/null local/path/to/file1.txt\n"
      "--- /dev/null\t2020-03-30\n"
      "+++ /path/to/file1.txt\t2020-03-30\n"  // new file
      "@@ -0,0 +1,2 @@\n"
      "+add me\n"
      "+add me too\n"
      "--- /path/to/file2.txt\t2020-03-30\n"
      "+++ /path/to/file2.txt\t2020-03-30\n"  // existing file
      "@@ -52,2 +53,4 @@\n"
      " no change here\n"
      "+add me\n"
      "+add me too\n"
      " no change here\n"
      "diff -u local/path/to/file3.txt /dev/null\n"
      "--- /path/to/file3.txt\t2020-03-30\n"  // deleted file
      "+++ /dev/null\t2020-03-30\n"
      "@@ -1,2 +0,0 @@\n"
      "-bye\n"
      "-bye\n";
  PatchSet patch_set;
  const auto status = patch_set.Parse(patch_contents);
  ASSERT_TRUE(status.ok()) << status.message();

  using P = FileLineNumbersMap::value_type;
  EXPECT_THAT(patch_set.AddedLinesMap(false),
              ElementsAre(P{"/path/to/file1.txt", {}},
                          P{"/path/to/file2.txt", {{54, 56}}}));
  EXPECT_THAT(patch_set.AddedLinesMap(true),
              ElementsAre(P{"/path/to/file1.txt", {{1, 3}}},
                          P{"/path/to/file2.txt", {{54, 56}}}));
  // Neither case should include deleted files like file3.txt
}

class PatchSetPickApplyTest : public PatchSet, public ::testing::Test {};

TEST_F(PatchSetPickApplyTest, EmptyFilePatchHunks) {
  {
    constexpr std::string_view patch_contents =  //
        "diff -u /dev/null local/path/to/file1.txt\n"
        "--- foo/bar.txt\t2020-03-30\n"
        "+++ foo/bar-formatted.txt\t2020-03-30\n";
    const auto status = Parse(patch_contents);
    ASSERT_TRUE(status.ok()) << status.message();
  }

  std::istringstream ins;
  std::ostringstream outs;
  // No file I/O or prompting because patch is empty.
  const auto status = PickApply(
      ins, outs,  //
      internal::ReadStringFileSequence({{"foo/bar.txt", "don't care\n"}}),
      internal::ExpectStringFileSequence({{"foo/bar.txt", "don't care\n"}}));
  EXPECT_TRUE(status.ok()) << "Got: " << status.message();
  EXPECT_TRUE(outs.str().empty()) << "Unexpected: " << outs.str();
}

TEST_F(PatchSetPickApplyTest, MultipleEmptyFilePatchHunks) {
  {
    constexpr std::string_view patch_contents =  //
        "diff -u /dev/null local/path/to/file1.txt\n"
        "--- foo/bar.txt\t2020-03-30\n"
        "+++ foo/bar-formatted.txt\t2020-03-30\n"
        "--- bar/foo.txt\t2020-03-30\n"
        "+++ bar/foo-formatted.txt\t2020-03-30\n";
    const auto status = Parse(patch_contents);
    ASSERT_TRUE(status.ok()) << status.message();
  }

  std::istringstream ins;
  std::ostringstream outs;
  // No file I/O or prompting because patches are empty.
  const auto status = PickApply(ins, outs,  //
                                internal::ReadStringFileSequence({
                                    {"foo/bar.txt", "don't care\n"},
                                    {"bar/foo.txt", "don't care\n"},
                                }),
                                internal::ExpectStringFileSequence({
                                    {"foo/bar.txt", "don't care\n"},
                                    {"bar/foo.txt", "don't care\n"},
                                }));
  EXPECT_TRUE(status.ok()) << "Got: " << status.message();
  EXPECT_TRUE(outs.str().empty()) << "Unexpected: " << outs.str();
}

TEST_F(PatchSetPickApplyTest, MultipleNonEmptyFilePatchHunks) {
  {
    constexpr std::string_view patch_contents =  //
        "diff -u /dev/null local/path/to/file1.txt\n"
        "--- foo/bar.txt\t2020-03-30\n"
        "+++ foo/bar-formatted.txt\t2020-03-30\n"
        "@@ -1,3 +1,2 @@\n"
        " you\n"
        "-lose\n"
        " some\n"
        "--- bar/foo.txt\t2020-03-30\n"
        "+++ bar/foo-formatted.txt\t2020-03-30\n"
        "@@ -1,2 +1,3 @@\n"
        " you\n"
        "+win\n"
        " some\n";
    const auto status = Parse(patch_contents);
    ASSERT_TRUE(status.ok()) << status.message();
  }

  std::istringstream ins("y\ny\n");  // accept one hunk in each file
  std::ostringstream outs;
  // No file I/O or prompting because patches are empty.
  const auto status = PickApply(ins, outs,  //
                                internal::ReadStringFileSequence({
                                    {"foo/bar.txt", "you\nlose\nsome\n"},
                                    {"bar/foo.txt", "you\nsome\n"},
                                }),
                                internal::ExpectStringFileSequence({
                                    {"foo/bar.txt", "you\nsome\n"},
                                    {"bar/foo.txt", "you\nwin\nsome\n"},
                                }));
  EXPECT_TRUE(status.ok()) << "Got: " << status.message();
  EXPECT_FALSE(outs.str().empty());
}

TEST_F(PatchSetPickApplyTest, FirstFilePatchOutOfOrder) {
  {
    constexpr std::string_view patch_contents =  //
        "diff -u /dev/null local/path/to/file1.txt\n"
        "--- foo/bar.txt\t2020-03-30\n"
        "+++ foo/bar-formatted.txt\t2020-03-30\n"
        "@@ -4,3 +3,2 @@\n"  // out-of-order
        " out\n"
        "-of\n"
        " order\n"
        "@@ -1,3 +1,2 @@\n"
        " you\n"
        "-lose\n"
        " some\n"
        "--- bar/foo.txt\t2020-03-30\n"
        "+++ bar/foo-formatted.txt\t2020-03-30\n"
        "@@ -1,2 +1,3 @@\n"
        " you\n"
        "+win\n"
        " some\n";
    const auto status = Parse(patch_contents);
    ASSERT_TRUE(status.ok()) << status.message();
  }

  std::istringstream ins("y\ny\n");  // accept
  std::ostringstream outs;
  // No file I/O or prompting because patches are empty.
  const auto status =
      PickApply(ins, outs,  //
                internal::ReadStringFileSequence({
                    {"foo/bar.txt", "you\nlose\nsome\nout\nof\norder"},
                    {"bar/foo.txt", "you\nsome\n"},
                }),
                internal::ExpectStringFileSequence({
                    {"foo/bar.txt", "you\nsome\n"},
                    {"bar/foo.txt", "you\nwin\nsome\n"},
                }));
  EXPECT_FALSE(status.ok());
  EXPECT_TRUE(absl::StrContains(status.message(), "not properly ordered"));
}

}  // namespace
}  // namespace verible
