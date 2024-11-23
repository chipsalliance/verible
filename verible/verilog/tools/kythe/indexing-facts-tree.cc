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

#include <algorithm>
#include <iostream>
#include <string>
#include <tuple>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "verible/common/util/tree-operations.h"

namespace verilog {
namespace kythe {

Anchor::Anchor(const Anchor &other) = default;

std::string Anchor::DebugString() const {
  if (source_text_range_) {
    return absl::StrCat("{", Text(), " @", source_text_range_->begin, "-",
                        source_text_range_->begin + source_text_range_->length,
                        "}");
  }
  return absl::StrCat("{", Text(), "}");
}

std::ostream &operator<<(std::ostream &stream, const Anchor &anchor) {
  return stream << anchor.DebugString();
}

bool Anchor::operator==(const Anchor &rhs) const {
  if (source_text_range_) {
    if (!rhs.source_text_range_) {
      return false;
    }
    if (std::tie(source_text_range_->begin, source_text_range_->length) !=
        std::tie(rhs.source_text_range_->begin,
                 rhs.source_text_range_->length)) {
      return false;
    }
  }
  return Text() == rhs.Text();
}

std::ostream &IndexingNodeData::DebugString(std::ostream *stream) const {
  *stream << indexing_fact_type_ << ": ["
          << absl::StrJoin(anchors_.begin(), anchors_.end(), ", ",
                           [](std::string *out, const Anchor &anchor) {
                             absl::StrAppend(out, anchor.DebugString());
                           })
          << ']';
  return *stream;
}

std::ostream &operator<<(std::ostream &stream, const IndexingNodeData &data) {
  const auto &anchors(data.Anchors());
  return stream << data.GetIndexingFactType() << ": ["
                << absl::StrJoin(anchors.begin(), anchors.end(), ", ",
                                 absl::StreamFormatter())
                << ']';
}

bool IndexingNodeData::operator==(const IndexingNodeData &rhs) const {
  return indexing_fact_type_ == rhs.GetIndexingFactType() &&
         anchors_.size() == rhs.Anchors().size() &&
         std::equal(anchors_.begin(), anchors_.end(), rhs.Anchors().begin());
}

std::ostream &operator<<(std::ostream &stream,
                         const PrintableIndexingNodeData &printable_node) {
  return printable_node.data.DebugString(&stream);
}

std::ostream &operator<<(std::ostream &stream,
                         const PrintableIndexingFactNode &printable_node) {
  return PrintTree(
      printable_node.data, &stream,
      [&printable_node](std::ostream &s,
                        const IndexingNodeData &d) -> std::ostream & {
        return s << PrintableIndexingNodeData(d, printable_node.base);
      });
}

}  // namespace kythe
}  // namespace verilog
