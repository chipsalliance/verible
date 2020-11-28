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

#include <iosfwd>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
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
        value_(token.text()) {}

  Anchor(const Anchor&) = default;  // TODO(fangism): delete, move-only
  Anchor(Anchor&&) = default;
  Anchor& operator=(const Anchor&) = delete;
  Anchor& operator=(Anchor&&) = default;

  // This function is for debugging only and isn't intended to be a textual
  // representation of this class.
  std::string DebugString() const;

  int StartLocation() const { return start_location_; }
  int EndLocation() const { return end_location_; }
  const std::string& Value() const { return value_; }

  bool operator==(const Anchor&) const;

 private:
  // Start locations of the current token inside the code text.
  int start_location_;

  // End locations of the current token inside the code text.
  int end_location_;

  // Value of the current token.
  std::string value_;
};

// This class is a simplified representation of CST and contains information
// that can be used for extracting indexing-facts for different indexing tools.
//
// This is intended to be an abstract layer between the parser generated CST
// and the indexing tool.
class IndexingNodeData {
 public:
  template <typename... Args>
  IndexingNodeData(IndexingFactType language_feature, Args&&... args)
      : indexing_fact_type_(language_feature) {
    AppendAnchor(std::forward<Args>(args)...);
  }

  // TODO(fangism): delete copy-ctor to make this move-only
  IndexingNodeData(const IndexingNodeData&) = default;
  IndexingNodeData(IndexingNodeData&&) = default;
  IndexingNodeData& operator=(const IndexingNodeData&) = delete;
  IndexingNodeData& operator=(IndexingNodeData&&) = delete;

  // Consume an Anchor object(s), variadically.
  template <typename... Args>
  void AppendAnchor(Anchor&& anchor, Args&&... args) {
    anchors_.emplace_back(std::move(anchor));
    AppendAnchor(std::forward<Args>(args)...);
  }

  // Swaps the anchors with the given IndexingNodeData.
  void SwapAnchors(IndexingNodeData* other) { anchors_.swap(other->anchors_); }

  // This function is for debugging only and isn't intended to be textual
  // representation of this class.
  std::ostream& DebugString(std::ostream* stream) const;

  const std::vector<Anchor>& Anchors() const { return anchors_; }
  IndexingFactType GetIndexingFactType() const { return indexing_fact_type_; }

  bool operator==(const IndexingNodeData&) const;

 private:
  // Base case for variadic AppendAnchor()
  void AppendAnchor() const {}

 private:
  // Represents which language feature this indexing fact is about.
  const IndexingFactType indexing_fact_type_;

  // Anchors representing the different tokens of this indexing fact.
  std::vector<Anchor> anchors_;
};

// human-readable form for debugging
std::ostream& operator<<(std::ostream&, const IndexingNodeData&);

// Renaming for VectorTree; IndexingFactNode is actually a VectorTree which is a
// class for constructing trees and dealing with them in a elegant manner.
using IndexingFactNode = verible::VectorTree<IndexingNodeData>;

}  // namespace kythe
}  // namespace verilog

#endif  // VERIBLE_VERILOG_TOOLS_KYTHE_INDEXING_FACTS_TREE_H_
