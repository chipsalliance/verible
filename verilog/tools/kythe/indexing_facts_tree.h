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

#include "absl/strings/substitute.h"
#include "common/text/token_info.h"
#include "common/util/vector_tree.h"
#include "verilog/tools/kythe/verilog_extractor_indexing_fact_type.h"

namespace verilog {
namespace kythe {

// Anchor class represents the location and value of some token.
class Anchor {
 public:
  Anchor(const verible::TokenInfo& token, absl::string_view base)
      : start_location_(token.left(base)),
        end_location_(token.right(base)),
        value_(token.text()){};

  // This function is for debugging only and isn't intended to be a textual
  // representation of this class.
  std::ostream& DebugString(std::ostream* stream) const {
    *stream << absl::Substitute(R"( {$0 @$1-$2})", value_, start_location_,
                                end_location_);
    return *stream;
  }

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

  void AppendAnchor(Anchor anchor) { anchors_.push_back(anchor); };

  void AppendAnchor(std::vector<Anchor> anchors) {
    anchors_.insert(anchors_.end(), anchors.begin(), anchors.end());
  };

  // This function is for debugging only and isn't intended to be textual
  // representation of this class.
  std::ostream& DebugString(std::ostream* stream) const {
    *stream << indexing_fact_type_;

    if (!anchors_.empty()) {
      *stream << " ";
      for (const auto& anchor : anchors_) {
        anchor.DebugString(stream);
      }
    }

    return *stream;
  }

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
