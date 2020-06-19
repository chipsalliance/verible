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

#include "common/strings/patch.h"

#include <sstream>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"

namespace verible {
namespace internal {
namespace {

TEST(MarkedLineParseTest, InvalidInputs) {
  constexpr absl::string_view kTestCases[] = {
      "", "x", "x213", "abc", "diff", "====",
  };
  for (const auto& test : kTestCases) {
    MarkedLine m;
    EXPECT_FALSE(m.Parse(test).ok()) << " input: \"" << test << '"';
  }
}

struct MarkedLineTestCase {
  absl::string_view input;
  char expected_mark;
  absl::string_view expected_text;
};

TEST(MarkedLineParseTest, ValidInputs) {
  constexpr MarkedLineTestCase kTestCases[] = {
      {" ", ' ', ""},         {" x", ' ', "x"},       {" x213", ' ', "x213"},
      {"  abc", ' ', " abc"}, {"-abc", '-', "abc"},   {"+abc", '+', "abc"},
      {"- abc", '-', " abc"}, {"+ abc", '+', " abc"}, {"---", '-', "--"},
      {"+++", '+', "++"},     {"-", '-', ""},         {"+", '+', ""},
  };
  for (const auto& test : kTestCases) {
    MarkedLine m;
    EXPECT_TRUE(m.Parse(test.input).ok()) << " input: \"" << test.input << '"';
    EXPECT_EQ(m.Marker(), test.expected_mark);
    EXPECT_EQ(m.Text(), test.expected_text);
  }
}

TEST(MarkedLinePrintTest, Print) {
  constexpr absl::string_view kTestCases[] = {
      " ", "+", "-", " 1 2 3", "-xyz", "+\tabc",
  };
  for (const auto& test : kTestCases) {
    MarkedLine m;
    ASSERT_TRUE(m.Parse(test).ok());
    std::ostringstream stream;
    stream << m;
    EXPECT_EQ(stream.str(), test);
  }
}

TEST(HunkIndicesParseTest, InvalidInputs) {
  constexpr absl::string_view kTestCases[] = {
      "", ",", "4,", ",5", "2,b", "x,2", "4,5,", "1,2,3",
  };
  for (const auto& test : kTestCases) {
    HunkIndices h;
    EXPECT_FALSE(h.Parse(test).ok()) << " input: \"" << test << '"';
  }
}

struct HunkIndicesTestCase {
  absl::string_view input;
  int expected_start;
  int expected_count;
};

TEST(HunkIndicesParseAndPrintTest, ValidInputs) {
  constexpr HunkIndicesTestCase kTestCases[] = {
      {"1,1", 1, 1},
      {"14,92", 14, 92},
  };
  for (const auto& test : kTestCases) {
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

TEST(HunkHeaderParseTest, InvalidInputs) {
  // If any one character is deleted from this example, it becomes invalid.
  constexpr absl::string_view kValidText = "@@ -4,8 +5,6 @@";

  for (size_t i = 0; i < kValidText.length(); ++i) {
    std::string deleted(kValidText);
    deleted.erase(i);
    HunkHeader h;
    EXPECT_FALSE(h.Parse(deleted).ok()) << " input: \"" << deleted << '"';
  }

  for (size_t i = 0; i < kValidText.length(); ++i) {
    absl::string_view deleted(kValidText.substr(0, i));
    HunkHeader h;
    EXPECT_FALSE(h.Parse(deleted).ok()) << " input: \"" << deleted << '"';
  }

  for (size_t i = 1; i < kValidText.length(); ++i) {
    absl::string_view deleted(kValidText.substr(i));
    HunkHeader h;
    EXPECT_FALSE(h.Parse(deleted).ok()) << " input: \"" << deleted << '"';
  }
}

TEST(HunkHeaderParseTest, MalformedOldRange) {
  constexpr absl::string_view kInvalidText = "@@ 4,8 +5,6 @@";
  HunkHeader h;
  const auto status = h.Parse(kInvalidText);
  EXPECT_FALSE(status.ok()) << " input: \"" << kInvalidText << '"';
  EXPECT_TRUE(absl::StrContains(status.message(),
                                "old-file range should start with '-'"))
      << " got: " << status.message();
}

TEST(HunkHeaderParseTest, MalformedNewRange) {
  constexpr absl::string_view kInvalidText = "@@ -4,8 5,6 @@";
  HunkHeader h;
  const auto status = h.Parse(kInvalidText);
  EXPECT_FALSE(status.ok()) << " input: \"" << kInvalidText << '"';
  EXPECT_TRUE(absl::StrContains(status.message(),
                                "new-file range should start with '+'"))
      << " got: " << status.message();
}

TEST(HunkHeaderParseAndPrintTest, ValidInput) {
  constexpr absl::string_view kValidText = "@@ -14,8 +5,16 @@";
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
  constexpr absl::string_view kValidText("@@ -4,28 +51,6 @@ void foo::bar() {");
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
  constexpr absl::string_view kTestCases[] = {
      "",
      "a.txt",
      "a.txt 1985-11-05",  // date should be preceded by tab
      "a.txt\t1985-11-05]\tunexpected_text",
  };
  for (const auto& test : kTestCases) {
    HunkIndices h;
    EXPECT_FALSE(h.Parse(test).ok()) << " input: \"" << test << '"';
  }
}

TEST(SourceInfoParseAndPrintTest, ValidInputs) {
  constexpr absl::string_view kPaths[] = {
      "a.txt",
      "p/q/a.txt",
      "/p/q/a.txt",
  };
  constexpr absl::string_view kTimes[] = {
      "2020-02-02",
      "2020-02-02 20:22:02",
      "2020-02-02 20:22:02.000000",
      "2020-02-02 20:22:02.000000 -0700",
  };
  for (const auto& path : kPaths) {
    for (const auto& time : kTimes) {
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

TEST(HunkParseTest, InvalidInputs) {
  const std::vector<absl::string_view> kTestCases[] = {
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
  for (const auto& lines : kTestCases) {
    Hunk hunk;
    const LineRange range(lines.begin(), lines.end());
    EXPECT_FALSE(hunk.Parse(range).ok());
  }
}

struct UpdateHeaderTestCase {
  absl::string_view fixed_header;
  std::vector<absl::string_view> payload;
};

TEST(HunkUpdateHeaderTest, Various) {
  constexpr absl::string_view kNonsenseHeader = "@@ -222,999 +333,999 @@";
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
  for (const auto& test : kTestCases) {
    std::vector<absl::string_view> lines;
    lines.push_back(kNonsenseHeader);
    lines.insert(lines.end(), test.payload.begin(), test.payload.end());
    const LineRange range(lines.begin(), lines.end());

    Hunk hunk;
    EXPECT_FALSE(hunk.Parse(range).ok());
    hunk.UpdateHeader();
    const auto status = hunk.IsValid();
    EXPECT_TRUE(status.ok()) << status.message();

    std::ostringstream stream;
    stream << hunk.header;
    EXPECT_EQ(stream.str(), test.fixed_header);
  }
}

TEST(HunkParseAndPrintTest, ValidInputs) {
  const std::vector<absl::string_view> kTestCases[] = {
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
  for (const auto& lines : kTestCases) {
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

TEST(FilePatchParseTest, InvalidInputs) {
  const std::vector<absl::string_view> kTestCases[] = {
      {},    // empty range is invalid
      {""},  // no "---" marker for source info
      {
          "--- /path/to/file.txt\t2020-03-30",  // no "+++" marker for source
                                                // info
      },
      {
          "--- /path/to/file.txt\t2020-03-30",
          "+++ /path/to/file.txt 2020-03-30",  // "+++" line is malformed
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
  for (const auto& lines : kTestCases) {
    const LineRange range(lines.begin(), lines.end());
    FilePatch file_patch;
    EXPECT_FALSE(file_patch.Parse(range).ok());
  }
}

TEST(FilePatchParseAndPrintTest, ValidInputs) {
  const std::vector<absl::string_view> kTestCases[] = {
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
          "==== //depot/p4/style/path/to/file.txt#4 - local/path/to/file.txt "
          "====",
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
  for (const auto& lines : kTestCases) {
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

}  // namespace
}  // namespace internal

namespace {
// public interface tests

TEST(PatchSetParseTest, InvalidInputs) {
  constexpr absl::string_view kTestCases[] = {
      // no "+++" marker for source
      "--- /path/to/file.txt\t2020-03-30\n",
      // "+++" line is malformed
      "--- /path/to/file.txt\t2020-03-30\n"
      "+++ /path/to/file.txt 2020-03-30\n",
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
  for (const auto& patch_contents : kTestCases) {
    PatchSet patch_set;
    EXPECT_FALSE(patch_set.Parse(patch_contents).ok());
  }
}

TEST(PatchSetParseAndPrintTest, ValidInputs) {
  constexpr absl::string_view kTestCases[] = {
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
      // with patchset metadat and file metadata in two files
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

  for (const auto& patch_contents : kTestCases) {
    PatchSet patch_set;
    const auto status = patch_set.Parse(patch_contents);
    EXPECT_TRUE(status.ok()) << status.message();

    // Validate reversibility.
    std::ostringstream stream;
    stream << patch_set;
    EXPECT_EQ(stream.str(), patch_contents);
  }
}

}  // namespace
}  // namespace verible
