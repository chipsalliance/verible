// Copyright 2022 The Verible Authors.
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

#include "verible/common/util/simple-zip.h"

#include <string>
#include <string_view>

#include "gtest/gtest.h"
#include "verible/common/util/file-util.h"

// Note, these tests are currently not testing that the generated content
// is actually unzippable (we don't have the reverse functionality), so we
// just probe that the generated file looks right.

static int CountSubstr(std::string_view needle, std::string_view haystack) {
  int count = 0;
  std::string_view::size_type pos = 0;
  while ((pos = haystack.find(needle, pos)) != std::string_view::npos) {
    count++;
    pos += needle.length();
  }
  return count;
}

TEST(SimpleZip, NoCompress) {
  std::string result;
  verible::zip::Encoder zipper(0, [&result](std::string_view out) {
    result.append(out.begin(), out.end());
    return true;
  });
  zipper.AddFile("essay.txt", verible::zip::MemoryByteSource("Hello world"));
  zipper.AddFile("empty.txt", verible::zip::MemoryByteSource("FOOFOO"));
  zipper.Finish();

  EXPECT_EQ(CountSubstr("Hello world", result), 1);  // Non-compressed content
  EXPECT_EQ(CountSubstr("FOO", result), 2);

  EXPECT_EQ(CountSubstr("essay.txt", result), 2);  // Filename in 2 headers
  EXPECT_EQ(CountSubstr("empty.txt", result), 2);  // Filename in 2 headers

  EXPECT_EQ(CountSubstr("PK\x03\x04", result), 2);  // one header per file
  EXPECT_EQ(CountSubstr("PK\x01\x02", result), 2);  // one per file in directory
  EXPECT_EQ(CountSubstr("PK\x05\x06", result), 1);  // directory footer
}

TEST(SimpleZip, WithCompression) {
  std::string result;
  verible::zip::Encoder zipper(9, [&result](std::string_view out) {
    result.append(out.begin(), out.end());
    return true;
  });
  zipper.AddFile("essay.txt", verible::zip::MemoryByteSource("Hello world"));
  zipper.AddFile("empty.txt", verible::zip::MemoryByteSource(""));
  zipper.Finish();

  EXPECT_EQ(CountSubstr("Hello world", result), 0);  // compressed string differ

  EXPECT_EQ(CountSubstr("essay.txt", result), 2);  // Filename in two headers
  EXPECT_EQ(CountSubstr("empty.txt", result), 2);  // Filename in two headers

  EXPECT_EQ(CountSubstr("PK\x03\x04", result), 2);  // one header per file
  EXPECT_EQ(CountSubstr("PK\x01\x02", result), 2);  // one per file in directory
  EXPECT_EQ(CountSubstr("PK\x05\x06", result), 1);  // directory footer
}

TEST(SimpleZip, ReadFromFileByteSource) {
  std::string result;
  verible::zip::Encoder zipper(0, [&result](std::string_view out) {
    result.append(out.begin(), out.end());
    return true;
  });

  verible::file::testing::ScopedTestFile tmpfile(::testing::TempDir(),
                                                 "Text from file");

  zipper.AddFile("hello.txt",
                 verible::zip::FileByteSource(tmpfile.filename().c_str()));
  zipper.Finish();

  EXPECT_EQ(CountSubstr("Text from file", result), 1);  // contained plain

  EXPECT_EQ(CountSubstr("hello.txt", result), 2);  // Filename in two headers

  EXPECT_EQ(CountSubstr("PK\x03\x04", result), 1);  // one per file
  EXPECT_EQ(CountSubstr("PK\x01\x02", result), 1);  // one per file in directory
  EXPECT_EQ(CountSubstr("PK\x05\x06", result), 1);  // directory footer
}

TEST(SimpleZip, ImplicitFinishOnDestruction) {
  std::string result;

  {
    verible::zip::Encoder zipper(0, [&result](std::string_view out) {
      result.append(out.begin(), out.end());
      return true;
    });
    zipper.AddFile("foo.txt", verible::zip::MemoryByteSource("Hello world"));
    // no explicit call to Finish()
  }

  EXPECT_EQ(CountSubstr("Hello world", result), 1);
  EXPECT_EQ(CountSubstr("PK\x03\x04", result), 1);  // one per file
  EXPECT_EQ(CountSubstr("PK\x01\x02", result), 1);  // one per file in directory
  EXPECT_EQ(CountSubstr("PK\x05\x06", result), 1);  // directory footer
}
