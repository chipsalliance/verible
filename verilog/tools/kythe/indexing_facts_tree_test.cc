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

#include "verilog/tools/kythe/indexing_facts_tree.h"

#include <sstream>

#include "absl/memory/memory.h"
#include "absl/strings/match.h"
#include "common/text/token_info.h"
#include "common/util/range.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "verilog/tools/kythe/verilog_extractor_indexing_fact_type.h"

namespace verilog {
namespace kythe {
namespace {

using verible::TokenInfo;

class TestAnchor : public Anchor {
 public:
  // Forward all constructors
  template <typename... Args>
  TestAnchor(Args&&... args) : Anchor(std::forward<Args>(args)...) {}

  // For testing purposes only, allow direct access to this method:
  using Anchor::OwnsMemory;
};

TEST(AnchorTest, ConstructFromStringView) {
  constexpr absl::string_view text("text");
  const TestAnchor anchor(text);
  EXPECT_FALSE(anchor.OwnsMemory());
  ASSERT_EQ(anchor.Text(), text);
  EXPECT_TRUE(verible::BoundsEqual(anchor.Text(), text));
}

TEST(AnchorTest, ConstructFromTokenInfo) {
  constexpr absl::string_view text("text");
  const TokenInfo token(1, text);
  const TestAnchor anchor(token);
  EXPECT_FALSE(anchor.OwnsMemory());
  ASSERT_EQ(anchor.Text(), token.text());
  ASSERT_EQ(anchor.Text(), text);
  EXPECT_TRUE(verible::BoundsEqual(anchor.Text(), text));
}

TEST(AnchorTest, MoveConstructFromStringView) {
  constexpr absl::string_view text("text");
  TestAnchor anchor(text);
  const TestAnchor anchor2(std::move(anchor));
  EXPECT_FALSE(anchor2.OwnsMemory());
  ASSERT_EQ(anchor2.Text(), text);
  EXPECT_TRUE(verible::BoundsEqual(anchor2.Text(), text));
}

TEST(AnchorTest, ConstructFromUniquePtrString) {
  const TestAnchor anchor(absl::make_unique<std::string>("PWNED!"));
  EXPECT_TRUE(anchor.OwnsMemory());
  EXPECT_EQ(anchor.Text(), "PWNED!");
}

TEST(AnchorTest, MoveConstructFromUniquePtrString) {
  auto str_ptr = absl::make_unique<std::string>("PWNED!");
  const absl::string_view range(*str_ptr);
  TestAnchor anchor(std::move(str_ptr));
  const TestAnchor anchor2(std::move(anchor));
  EXPECT_TRUE(anchor2.OwnsMemory());
  EXPECT_EQ(anchor2.Text(), "PWNED!");
  // guaranteed move, address stable
  // moving unique_ptr circumvents any short-string-optimization effects
  EXPECT_TRUE(verible::BoundsEqual(anchor2.Text(), range));
}

TEST(AnchorTest, CopyStringViewBacked) {
  // Note: eventually copying will be disabled
  const absl::string_view text("zoned");
  TestAnchor anchor(text);
  const TestAnchor anchor2(anchor);
  EXPECT_FALSE(anchor.OwnsMemory());
  EXPECT_FALSE(anchor2.OwnsMemory());
  EXPECT_EQ(anchor, anchor2);
  EXPECT_TRUE(verible::BoundsEqual(anchor2.Text(), anchor.Text()));
}

TEST(AnchorTest, CopyUniquePtrString) {
  // Note: eventually copying will be disabled
  auto str_ptr = absl::make_unique<std::string>("loaned");
  TestAnchor anchor(std::move(str_ptr));
  const TestAnchor anchor2(anchor);
  EXPECT_TRUE(anchor.OwnsMemory());
  EXPECT_TRUE(anchor2.OwnsMemory());
  EXPECT_EQ(anchor, anchor2);
  EXPECT_FALSE(verible::BoundsEqual(anchor2.Text(), anchor.Text()));
}

TEST(AnchorTest, DebugStringUsingOffsets) {
  constexpr absl::string_view text("abcdefghij");
  const Anchor anchor(text.substr(4, 3));
  const std::string debug_string(anchor.DebugString(text));
  EXPECT_EQ(debug_string, "{efg @4-7}");
}

TEST(AnchorTest, DebugStringUsingAddresses) {
  constexpr absl::string_view text("abcdefghij");
  const Anchor anchor(text.substr(3, 4));
  std::ostringstream stream;
  stream << anchor;
  EXPECT_TRUE(absl::StrContains(stream.str(), "{defg @"));
}

TEST(AnchorTest, RebaseStringView) {
  constexpr absl::string_view text1("abcdefghij");
  constexpr absl::string_view text2("defg");
  Anchor anchor(text1.substr(3, 4));  // "defg"
  anchor.RebaseStringViewForTesting(
      std::distance(anchor.Text().begin(), text2.begin()));
  EXPECT_EQ(anchor.Text(), text2);
  EXPECT_TRUE(verible::BoundsEqual(anchor.Text(), text2));
}

TEST(AnchorTest, RebaseStringViewFail) {
  constexpr absl::string_view text1("abcdefghij");
  constexpr absl::string_view text2("DEFG");
  Anchor anchor(text1.substr(3, 4));  // "defg"
  EXPECT_DEATH(anchor.RebaseStringViewForTesting(
                   std::distance(anchor.Text().begin(), text2.begin())),
               "Rebased string contents must match");
}

TEST(AnchorTest, EqualityNotOwned) {
  constexpr absl::string_view text1("abcd");
  constexpr absl::string_view text2("defg");
  EXPECT_EQ(Anchor(text1), Anchor(text1));
  EXPECT_EQ(Anchor(text2), Anchor(text2));
  EXPECT_NE(Anchor(text1), Anchor(text2));
  EXPECT_NE(Anchor(text2), Anchor(text1));
}

TEST(AnchorTest, EqualityOwned) {
  const Anchor anchor1(absl::make_unique<std::string>("PWNED"));
  const Anchor anchor2(absl::make_unique<std::string>("zoned"));
  EXPECT_EQ(anchor1, anchor1);
  EXPECT_EQ(anchor2, anchor2);
  EXPECT_NE(anchor1, anchor2);
  EXPECT_NE(anchor2, anchor1);

  const Anchor anchor3(absl::make_unique<std::string>("PWNED"));
  EXPECT_FALSE(verible::BoundsEqual(anchor1.Text(), anchor3.Text()));
  EXPECT_EQ(anchor1, anchor3);
  EXPECT_EQ(anchor3, anchor1);
  EXPECT_NE(anchor2, anchor3);
  EXPECT_NE(anchor3, anchor2);
}

TEST(AnchorTest, EqualityMixed) {
  const Anchor anchor1(absl::make_unique<std::string>("PWNED"));
  const Anchor anchor2(absl::string_view("PWNED"));
  EXPECT_EQ(anchor1, anchor2);
  EXPECT_EQ(anchor2, anchor1);

  const Anchor anchor3(absl::make_unique<std::string>("stoned"));
  const Anchor anchor4(absl::string_view("STONED"));
  EXPECT_NE(anchor1, anchor3);
  EXPECT_NE(anchor3, anchor1);
  EXPECT_NE(anchor1, anchor4);
  EXPECT_NE(anchor4, anchor1);
  EXPECT_NE(anchor2, anchor3);
  EXPECT_NE(anchor3, anchor2);
  EXPECT_NE(anchor2, anchor4);
  EXPECT_NE(anchor4, anchor2);
}

TEST(IndexingNodeDataTest, ConstructionNoAnchor) {
  const IndexingNodeData indexing_data(IndexingFactType::kFile);
  EXPECT_EQ(indexing_data.GetIndexingFactType(), IndexingFactType::kFile);
  EXPECT_TRUE(indexing_data.Anchors().empty());
}

TEST(IndexingNodeDataTest, ConstructionVariadicAnchors) {
  constexpr absl::string_view text1("abc");
  constexpr absl::string_view text2("xyzzy");
  {
    const IndexingNodeData indexing_data(IndexingFactType::kFile,
                                         Anchor(text1));
    EXPECT_EQ(indexing_data.GetIndexingFactType(), IndexingFactType::kFile);
    ASSERT_EQ(indexing_data.Anchors().size(), 1);
    EXPECT_TRUE(
        verible::BoundsEqual(indexing_data.Anchors().front().Text(), text1));
  }
  {
    const IndexingNodeData indexing_data(IndexingFactType::kFile, Anchor(text1),
                                         Anchor(text2));
    EXPECT_EQ(indexing_data.GetIndexingFactType(), IndexingFactType::kFile);
    ASSERT_EQ(indexing_data.Anchors().size(), 2);
    EXPECT_TRUE(verible::BoundsEqual(indexing_data.Anchors()[0].Text(), text1));
    EXPECT_TRUE(verible::BoundsEqual(indexing_data.Anchors()[1].Text(), text2));
  }
}

TEST(IndexingNodeDataTest, SwapAnchors) {
  constexpr absl::string_view text1("abc");
  constexpr absl::string_view text2("xyzzy");
  IndexingNodeData indexing_data1(IndexingFactType::kFile, Anchor(text1));
  IndexingNodeData indexing_data2(IndexingFactType::kFile, Anchor(text2));
  indexing_data1.SwapAnchors(&indexing_data2);
  ASSERT_EQ(indexing_data1.Anchors().size(), 1);
  ASSERT_EQ(indexing_data2.Anchors().size(), 1);
  EXPECT_TRUE(
      verible::BoundsEqual(indexing_data1.Anchors().front().Text(), text2));
  EXPECT_TRUE(
      verible::BoundsEqual(indexing_data2.Anchors().front().Text(), text1));
}

TEST(IndexingNodeDataTest, RebaseStringViews) {
  constexpr absl::string_view src("abcdefghij");
  constexpr absl::string_view dest("abcdefghijkl");
  IndexingNodeData indexing_data(IndexingFactType::kClass,
                                 Anchor(src.substr(1, 3)),
                                 Anchor(src.substr(5, 4)));
  indexing_data.RebaseStringViewsForTesting(
      std::distance(src.begin(), dest.begin()));
  const auto& anchors(indexing_data.Anchors());
  EXPECT_TRUE(verible::BoundsEqual(anchors[0].Text(), dest.substr(1, 3)));
  EXPECT_TRUE(verible::BoundsEqual(anchors[1].Text(), dest.substr(5, 4)));
}

TEST(IndexingNodeDataTest, Equality) {
  const IndexingNodeData data1(IndexingFactType::kFile);
  EXPECT_EQ(data1, data1);

  const IndexingNodeData data2(IndexingFactType::kClass);
  EXPECT_EQ(data2, data2);
  // different IndexingFactType
  EXPECT_NE(data1, data2);
  EXPECT_NE(data2, data1);

  const IndexingNodeData data3(IndexingFactType::kFile,
                               Anchor(absl::string_view("fgh")));
  EXPECT_EQ(data3, data3);
  // different number of anchors
  EXPECT_NE(data1, data3);
  EXPECT_NE(data3, data1);

  const IndexingNodeData data4(IndexingFactType::kFile,
                               Anchor(absl::string_view("ijk")));
  // same number of anchors, different text contents
  EXPECT_NE(data1, data4);
  EXPECT_NE(data4, data1);
}

TEST(IndexingNodeDataTest, DebugStringUsingOffsets) {
  constexpr absl::string_view text("abcdefghij");
  const IndexingNodeData data(IndexingFactType::kClass,
                              Anchor(text.substr(1, 2)),
                              Anchor(text.substr(4, 3)));
  constexpr absl::string_view expected("kClass: [{bc @1-3}, {efg @4-7}]");
  {
    std::ostringstream stream;
    data.DebugString(&stream, text);
    EXPECT_EQ(stream.str(), expected);
  }
  {
    std::ostringstream stream;
    stream << PrintableIndexingNodeData(data, text);
    EXPECT_EQ(stream.str(), expected);
  }
}

TEST(IndexingNodeDataTest, DebugStringUsingAddresses) {
  constexpr absl::string_view text("abcdefghij");
  const IndexingNodeData data(IndexingFactType::kFile,
                              Anchor(text.substr(1, 2)),
                              Anchor(text.substr(4, 3)));
  std::ostringstream stream;
  stream << data;
  EXPECT_TRUE(absl::StrContains(stream.str(), "kFile: [{bc @"));
  EXPECT_TRUE(absl::StrContains(stream.str(), "efg @"));
}

TEST(IndexingFactNodeTest, StreamPrint) {
  constexpr absl::string_view text("abcdefghij");
  typedef IndexingFactNode Node;
  const Node node(
      IndexingNodeData(IndexingFactType::kClass, Anchor(text.substr(1, 2)),
                       Anchor(text.substr(4, 3))),
      Node(IndexingNodeData(IndexingFactType::kClass,
                            Anchor(text.substr(3, 5)))));
  constexpr absl::string_view expected(
      "{ (kClass: [{bc @1-3}, {efg @4-7}])\n"
      "  { (kClass: [{defgh @3-8}]) }\n"
      "}");
  {
    std::ostringstream stream;
    stream << PrintableIndexingFactNode(node, text);
    EXPECT_EQ(stream.str(), expected);
  }
}

}  // namespace
}  // namespace kythe
}  // namespace verilog
