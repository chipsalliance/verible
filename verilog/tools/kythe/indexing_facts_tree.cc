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

#include <iostream>

#include "absl/strings/str_join.h"
#include "absl/strings/substitute.h"
#include "verilog/tools/kythe/indexing_facts_tree.h"

namespace verilog {
namespace kythe {

std::string Anchor::DebugString() const {
  return absl::Substitute(R"( {$0 @$1-$2})", value_, start_location_,
                          end_location_);
}

bool Anchor::operator==(const Anchor& rhs) const {
  return start_location_ == rhs.StartLocation() &&
         end_location_ == rhs.EndLocation() && value_ == rhs.Value();
}

std::ostream& IndexingNodeData::DebugString(std::ostream* stream) const {
  *stream << indexing_fact_type_;
  *stream << absl::StrJoin(anchors_.begin(), anchors_.end(), " ",
                           [](std::string* out, const Anchor& anchor) {
                             absl::StrAppend(out, anchor.DebugString());
                           });
  return *stream;
}
bool IndexingNodeData::operator==(const IndexingNodeData& rhs) const {
  return indexing_fact_type_ == rhs.GetIndexingFactType() &&
         anchors_.size() == rhs.Anchors().size() &&
         std::equal(anchors_.begin(), anchors_.end(), rhs.Anchors().begin());
}

std::ostream& operator<<(std::ostream& stream,
                         const IndexingNodeData& indexing_node_data) {
  return indexing_node_data.DebugString(&stream);
}

}  // namespace kythe
}  // namespace verilog
