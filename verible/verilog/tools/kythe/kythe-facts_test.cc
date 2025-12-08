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

#include "verible/verilog/tools/kythe/kythe-facts.h"

#include <sstream>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace verilog {
namespace kythe {
namespace {

using ::testing::ElementsAre;

TEST(SignatureTest, EmptyToString) {
  const Signature s;
  EXPECT_EQ(s.ToString(), "");
  EXPECT_THAT(s.Names(), ElementsAre(""));
}

TEST(SignatureTest, ToString) {
  const Signature s("foobar");
  EXPECT_EQ(s.ToString(), "foobar#");
  EXPECT_THAT(s.Names(), ElementsAre("foobar"));
}

TEST(SignatureTest, WithParentToString) {
  const Signature s1("foobar");
  const Signature s2(s1, "baz");
  EXPECT_EQ(s2.ToString(), "foobar#baz#");
  EXPECT_THAT(s2.Names(), ElementsAre("foobar", "baz"));
}

TEST(SignatureTest, Equality) {
  const Signature s1("aaa");
  const Signature s2("bbb");
  EXPECT_EQ(s1, s1);
  EXPECT_EQ(s2, s2);
  EXPECT_NE(s1, s2);
  EXPECT_NE(s2, s1);
  {
    const Signature s3(s2, "ccc");
    EXPECT_NE(s1, s3);
    EXPECT_NE(s3, s1);
    EXPECT_NE(s2, s3);
    EXPECT_NE(s3, s2);
  }
}

TEST(VNameTest, DefaultCtor) {
  const VName vname;
  std::ostringstream stream;
  stream << vname;
  EXPECT_EQ(stream.str(),
            R"({
  "signature": "",
  "path": "",
  "language": "verilog",
  "root": "",
  "corpus": ""
})");
}

TEST(VNameTest, FilledCtor) {
  const Signature s("s");
  const VName vname{.path = "/path/to/nowhere.lol",
                    .root = "root",
                    .signature = s,
                    .corpus = "http://corpus.code/corpus/"};
  {
    std::ostringstream stream;
    stream << vname;
    EXPECT_EQ(stream.str(), R"({
  "signature": "s#",
  "path": "/path/to/nowhere.lol",
  "language": "verilog",
  "root": "root",
  "corpus": "http://corpus.code/corpus/"
})");
    EXPECT_EQ(vname, vname);
  }
  {
    const VName vname2;
    EXPECT_FALSE(vname == vname2);
    EXPECT_FALSE(vname2 == vname);
  }
}

TEST(FactTest, FormatJSON) {
  const Signature s("sss");
  const VName v{.path = "/path", .root = "", .signature = s, .corpus = ""};
  const Fact fact(v, "FactName", "FactValue");
  std::ostringstream stream;
  stream << fact;
  EXPECT_EQ(stream.str(), R"({
  "source": {
    "signature": "sss#",
    "path": "/path",
    "language": "verilog",
    "root": "",
    "corpus": ""
  },
  "fact_name": "FactName",
  "fact_value": "FactValue"
})");
}

TEST(FactTest, Equality) {
  const Signature s("sss");
  const VName v{.path = "/path", .root = "", .signature = s, .corpus = ""};
  const Fact fact1(v, "FactName", "FactValueA");
  const Fact fact2(v, "FactName", "FactValueB");
  EXPECT_EQ(fact1, fact1);
  EXPECT_EQ(fact2, fact2);
  EXPECT_NE(fact1, fact2);
  EXPECT_NE(fact2, fact1);
}

TEST(EdgeTest, FormatJSON) {
  const Signature s1("sss"), s2("ttt");
  const VName v1{.path = "/path", .root = "", .signature = s1, .corpus = ""};
  const VName v2{.path = "/path", .root = "", .signature = s2, .corpus = ""};
  const Edge edge(v1, "EdgeName", v2);
  std::ostringstream stream;
  stream << edge;
  EXPECT_EQ(stream.str(), R"({
  "source": {
    "signature": "sss#",
    "path": "/path",
    "language": "verilog",
    "root": "",
    "corpus": ""
  },
  "edge_kind": "EdgeName",
  "target": {
    "signature": "ttt#",
    "path": "/path",
    "language": "verilog",
    "root": "",
    "corpus": ""
  },
  "fact_name": "/"
})");
}

TEST(EdgeTest, Equality) {
  const Signature s1("sss"), s2("ttt");
  const VName v1{.path = "/path", .root = "", .signature = s1, .corpus = ""};
  const VName v2{.path = "/path", .root = "", .signature = s2, .corpus = ""};
  const Edge edge1(v1, "EdgeName", v2), edge2(v2, "Reverse", v1);
  EXPECT_EQ(edge1, edge1);
  EXPECT_EQ(edge2, edge2);
  EXPECT_NE(edge1, edge2);
  EXPECT_NE(edge2, edge1);
}

}  // namespace
}  // namespace kythe
}  // namespace verilog
