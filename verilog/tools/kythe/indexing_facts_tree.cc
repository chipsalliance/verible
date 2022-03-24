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

#include <algorithm>
#include <iostream>

#include "absl/memory/memory.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "common/util/range.h"
#include "common/util/tree_operations.h"

namespace verilog {
namespace kythe {

Anchor::Anchor(const Anchor& other)
    : owned_string_(other.owned_string_
                        ? absl::make_unique<std::string>(*other.owned_string_)
                        : nullptr),
      view_(owned_string_ ? *owned_string_ : other.view_) {}

std::string Anchor::DebugString(absl::string_view base) const {
  const std::ptrdiff_t start_location =
      std::distance(base.begin(), Text().begin());
  const std::ptrdiff_t end_location = std::distance(base.begin(), Text().end());
  return absl::StrCat("{", Text(), " @", start_location, "-", end_location,
                      "}");
}

std::ostream& operator<<(std::ostream& stream, const Anchor& anchor) {
  const absl::string_view text(anchor.Text());
  return stream << "{" << text << " @" << static_cast<const void*>(text.data())
                << "+" << text.length() << "}";
}

void Anchor::RebaseStringViewForTesting(std::ptrdiff_t delta) {
  if (OwnsMemory()) {
    return;  // owned memory should never be rebased
  }

  const absl::string_view rebased(view_.data() + delta, view_.length());
  CHECK_EQ(rebased, view_) << "Rebased string contents must match the source.";
  view_ = rebased;
}

bool Anchor::operator==(const Anchor& rhs) const {
  // If either strings are owned, compare their contents only, not their ranges.
  if (OwnsMemory() || rhs.OwnsMemory()) return Text() == rhs.Text();
  // Otherwise, ranges must be exactly equal.
  return verible::BoundsEqual(Text(), rhs.Text());
}

std::ostream& IndexingNodeData::DebugString(std::ostream* stream,
                                            absl::string_view base) const {
  *stream << indexing_fact_type_ << ": ["
          << absl::StrJoin(anchors_.begin(), anchors_.end(), ", ",
                           [&base](std::string* out, const Anchor& anchor) {
                             absl::StrAppend(out, anchor.DebugString(base));
                           })
          << ']';
  return *stream;
}

std::ostream& operator<<(std::ostream& stream, const IndexingNodeData& data) {
  const auto& anchors(data.Anchors());
  return stream << data.GetIndexingFactType() << ": ["
                << absl::StrJoin(anchors.begin(), anchors.end(), ", ",
                                 absl::StreamFormatter())
                << ']';
}

void IndexingNodeData::RebaseStringViewsForTesting(std::ptrdiff_t delta) {
  VLOG(3) << __FUNCTION__;
  switch (indexing_fact_type_) {
    // The following types have string memory that belongs outside of source
    // code, and should never be rebased.
    case IndexingFactType::kFile:
    case IndexingFactType::kFileList:
      VLOG(3) << "end of " << __FUNCTION__ << " (skipped)";
      return;
    default:
      break;
  }

  for (auto& anchor : anchors_) {
    anchor.RebaseStringViewForTesting(delta);
  }
  VLOG(3) << "end of " << __FUNCTION__;
}

bool IndexingNodeData::operator==(const IndexingNodeData& rhs) const {
  return indexing_fact_type_ == rhs.GetIndexingFactType() &&
         anchors_.size() == rhs.Anchors().size() &&
         std::equal(anchors_.begin(), anchors_.end(), rhs.Anchors().begin());
}

std::ostream& operator<<(std::ostream& stream,
                         const PrintableIndexingNodeData& printable_node) {
  return printable_node.data.DebugString(&stream, printable_node.base);
}

std::ostream& operator<<(std::ostream& stream,
                         const PrintableIndexingFactNode& printable_node) {
  return PrintTree(
      printable_node.data, &stream,
      [&printable_node](std::ostream& s,
                        const IndexingNodeData& d) -> std::ostream& {
        return s << PrintableIndexingNodeData(d, printable_node.base);
      });
}

}  // namespace kythe
}  // namespace verilog
