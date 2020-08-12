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

#include <utility>

#include "common/text/token_info.h"
#include "common/util/vector_tree.h"
#include "verilog/tools/kythe/verilog_extractor_indexing_fact_type.h"

namespace verilog {
namespace kythe {

// TODO(MinaToma): Investigate this and think to replace it with TokenInfo.
// Anchor class represents the location and value of some token.
class Anchor {
 public:
  Anchor(absl::string_view value, int startLocation, int endLocation)
      : start_location_(startLocation),
        end_location_(std::max(0, endLocation)),
        value_(value) {}

  Anchor(const verible::TokenInfo& token, absl::string_view base)
      : start_location_(token.left(base)),
        end_location_(token.right(base)),
        value_(token.text()){};

  // This function is for debugging only and isn't intended to be a textual
  // representation of this class.
  std::string DebugString() const;

  int StartLocation() const { return start_location_; }
  int EndLocation() const { return end_location_; }
  std::string Value() const { return value_; }

 private:
  // Start and end locations of the current token inside the code text.
  int start_location_, end_location_;

  // Value of the current token.
  std::string value_;
};

// This class is a simplified representation of CST and contains information
// that can be used for extracting indexing-facts for different indexing tools.
//
// This is intended to be an abstract layer between the parser and the indexing
// tool.
class IndexingNodeData {
 public:
  explicit IndexingNodeData(IndexingFactType language_feature)
      : indexing_fact_type_(language_feature) {}

  IndexingNodeData(std::vector<Anchor> anchor,
                   IndexingFactType language_feature)
      : anchors_(std::move(anchor)), indexing_fact_type_(language_feature) {}

  void AppendAnchor(const Anchor& anchor) { anchors_.push_back(anchor); };

  void AppendAnchor(std::vector<Anchor> anchors) {
    anchors_.insert(anchors_.end(), anchors.begin(), anchors.end());
  };

  // This function is for debugging only and isn't intended to be textual
  // representation of this class.
  std::ostream& DebugString(std::ostream* stream) const;

  const std::vector<Anchor>& Anchors() const { return anchors_; }
  IndexingFactType GetIndexingFactType() const { return indexing_fact_type_; }

 private:
  // Anchors representing the different tokens of this indexing fact.
  std::vector<Anchor> anchors_;

  // Represents which language feature this indexing fact is about.
  IndexingFactType indexing_fact_type_;
};

// human-readable form for debugging
std::ostream& operator<<(std::ostream&, const IndexingNodeData&);

using IndexingFactNode = verible::VectorTree<IndexingNodeData>;

}  // namespace kythe
}  // namespace verilog

#endif  // VERIBLE_VERILOG_TOOLS_KYTHE_INDEXING_FACTS_TREE_H_
