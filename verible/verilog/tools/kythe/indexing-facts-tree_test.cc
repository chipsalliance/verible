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

#include "verible/verilog/tools/kythe/indexing-facts-tree.h"

#include <sstream>
#include <string>

#include "absl/strings/string_view.h"
#include "gtest/gtest.h"
#include "verible/common/util/range.h"
#include "verible/verilog/tools/kythe/verilog-extractor-indexing-fact-type.h"

namespace verilog {
namespace kythe {
namespace {

class TestAnchor : public Anchor {
 public:
  // Forward all constructors
  template <typename... Args>
  explicit TestAnchor(Args &&...args) : Anchor(std::forward<Args>(args)...) {}
};

TEST(AnchorTest, DebugStringUsingOffsets) {
  constexpr absl::string_view text("abcdefghij");
  const Anchor anchor(text.substr(4, 3), /*begin=*/4, /*length=*/3);
  const std::string debug_string(anchor.DebugString());
  EXPECT_EQ(debug_string, "{efg @4-7}");
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
  const Anchor anchor1("PWNED");
  const Anchor anchor2("zoned");
  EXPECT_EQ(anchor1, anchor1);
  EXPECT_EQ(anchor2, anchor2);
  EXPECT_NE(anchor1, anchor2);
  EXPECT_NE(anchor2, anchor1);

  const Anchor anchor3("PWNED");
  EXPECT_FALSE(verible::BoundsEqual(anchor1.Text(), anchor3.Text()));
  EXPECT_EQ(anchor1, anchor3);
  EXPECT_EQ(anchor3, anchor1);
  EXPECT_NE(anchor2, anchor3);
  EXPECT_NE(anchor3, anchor2);
}

TEST(AnchorTest, EqualityMixed) {
  const Anchor anchor1("PWNED");
  const Anchor anchor2(absl::string_view("PWNED"));
  EXPECT_EQ(anchor1, anchor2);
  EXPECT_EQ(anchor2, anchor1);

  const Anchor anchor3("stoned");
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
    EXPECT_EQ(indexing_data.Anchors().front().Text(), text1);
  }
  {
    const IndexingNodeData indexing_data(IndexingFactType::kFile, Anchor(text1),
                                         Anchor(text2));
    EXPECT_EQ(indexing_data.GetIndexingFactType(), IndexingFactType::kFile);
    ASSERT_EQ(indexing_data.Anchors().size(), 2);
    EXPECT_EQ(indexing_data.Anchors()[0].Text(), text1);
    EXPECT_EQ(indexing_data.Anchors()[1].Text(), text2);
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
  EXPECT_EQ(indexing_data1.Anchors().front().Text(), text2);
  EXPECT_EQ(indexing_data2.Anchors().front().Text(), text1);
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
  const IndexingNodeData data(
      IndexingFactType::kClass,
      Anchor(text.substr(1, 2), /*begin=*/1, /*length=*/2),
      Anchor(text.substr(4, 3), /*begin=*/4, /*length=*/3));
  constexpr absl::string_view expected("kClass: [{bc @1-3}, {efg @4-7}]");
  {
    std::ostringstream stream;
    data.DebugString(&stream);
    EXPECT_EQ(stream.str(), expected);
  }
  {
    std::ostringstream stream;
    stream << PrintableIndexingNodeData(data, text);
    EXPECT_EQ(stream.str(), expected);
  }
}

TEST(IndexingFactNodeTest, StreamPrint) {
  constexpr absl::string_view text("abcdefghij");
  using Node = IndexingFactNode;
  const Node node(
      IndexingNodeData(IndexingFactType::kClass,
                       Anchor(text.substr(1, 2), /*begin=*/1, /*length=*/2),
                       Anchor(text.substr(4, 3), /*begin=*/4, /*length=*/3)),
      Node(IndexingNodeData(
          IndexingFactType::kClass,
          Anchor(text.substr(3, 5), /*begin=*/3, /*length=*/5))));
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
