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

#ifndef VERIBLE_VERILOG_TOOLS_KYTHE_INDEXING_FACTS_TREE_H_
#define VERIBLE_VERILOG_TOOLS_KYTHE_INDEXING_FACTS_TREE_H_

#include <algorithm>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <sstream>  // IWYU pragma: keep  // for ostringstream
#include <string>   // for string, allocator, etc
#include <utility>
#include <vector>

#include "absl/strings/substitute.h"
#include "common/text/token_info.h"
#include "verilog/tools/kythe/verilog_extractor_indexing_fact_type.h"

// Anchor class represents the location and value of some token.
class Anchor {
 public:
  Anchor(const verible::TokenInfo& token, absl::string_view base)
      : start_location_(token.left(base)),
        end_location_(token.right(base)),
        value_(token.text()){};

  // This function is for debugging only and isn't intended to be a textual
  // representation of this class.
  std::string DebugString() {
    return absl::Substitute(
        R"({"StartLocation": $0,"EndLocation": $1,"Value": "$2"})",
        start_location_, end_location_, value_);
  }

 private:
  // Start and end locations of the current token inside the code text.
  int start_location_, end_location_;

  // Value of the current token.
  absl::string_view value_;
};

// This class is a simplified representation of CST and contains information
// that can be used for extracting indexing-facts for different indexing tools.
//
// This is intended to be an abstract layer between the parser and the indexing
// tool.
class IndexingFactNode {
 public:
  explicit IndexingFactNode(IndexingFactType language_feature)
      : indexing_fact_type_(language_feature) {}

  IndexingFactNode(std::vector<Anchor> anchor,
                   IndexingFactType language_feature)
      : anchors_(std::move(anchor)), indexing_fact_type_(language_feature) {}

  std::vector<IndexingFactNode> Children() { return children_; };

  void AppendChild(IndexingFactNode indexing_fact_node) {
    children_.push_back(std::move(indexing_fact_node));
  };

  void AppendChild(std::vector<IndexingFactNode> children) {
    children_.insert(children_.end(), children.begin(), children.end());
  };

  void AppendAnchor(Anchor anchor) { anchors_.push_back(std::move(anchor)); };

  void AppendAnchor(std::vector<Anchor> anchors) {
    anchors_.insert(anchors_.end(), anchors.begin(), anchors.end());
  };

  // This function is for debugging only and isn't intended to be textual
  // representation of this class.
  std::string DebugString() {
    std::string debug_string;
    debug_string = R"({"IndexingFactType": ")" +
                   LanguageFeatureEnumToString(indexing_fact_type_) + "\",\n";

    debug_string += "\"Anchor\": [\n";
    for (auto& anchor : anchors_) {
      debug_string += anchor.DebugString();
      if (&anchor != &anchors_.back()) {
        debug_string += ",\n";
      }
    }
    debug_string += "],\n";

    debug_string += "\"Children\": [\n";
    for (auto& child : children_) {
      debug_string += child.DebugString();
      if (&child != &children_.back()) {
        debug_string += ",\n";
      }
    }
    debug_string += "]\n";
    debug_string += "}\n";

    return debug_string;
  }

 private:
  // Anchors representing the different tokens of this indexing fact.
  std::vector<Anchor> anchors_;

  // Represents which language feature this indexing fact is about.
  IndexingFactType indexing_fact_type_;

  // Other indexing facts which have a parent-child relation with this indexing
  // fact.
  std::vector<IndexingFactNode> children_;
};

#endif  // VERIBLE_VERILOG_TOOLS_KYTHE_INDEXING_FACTS_TREE_H_
