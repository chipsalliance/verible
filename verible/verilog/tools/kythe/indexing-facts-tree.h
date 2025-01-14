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

#include <cstddef>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "verible/common/text/token-info.h"
#include "verible/common/util/vector-tree.h"
#include "verible/verilog/tools/kythe/verilog-extractor-indexing-fact-type.h"

namespace verilog {
namespace kythe {

// Position of the Anchor in the original text. Required to be able to reference
// Anchor's content in the original string (as Anchor is owning its content --
// string_view is not applicable).
struct AnchorRange {
  const size_t begin;
  const size_t length;

  AnchorRange(size_t b, size_t l) : begin(b), length(l) {}
};

// Anchor class represents the location and value of some token.
class Anchor {
 public:
  explicit Anchor(std::string_view value) : content_(value) {}

  explicit Anchor(std::string_view value, size_t begin, size_t length)
      : content_(value) {
    source_text_range_.emplace(begin, length);
  }

  // Delegates construction to use only the string_view spanned by a TokenInfo.
  // Recall the TokenInfo's string point to substrings of memory owned
  // elsewhere.
  explicit Anchor(const verible::TokenInfo &token,
                  std::string_view source_content)
      : content_(token.text()) {
    const int token_left = token.left(source_content);
    const int token_right = token.right(source_content);
    source_text_range_.emplace(token_left, token_right - token_left);
  }

  Anchor(const Anchor &);  // TODO(fangism): delete, move-only
  Anchor(Anchor &&) = default;
  Anchor &operator=(const Anchor &) = delete;
  Anchor &operator=(Anchor &&) = delete;

  // Returns human readable view of this Anchor.
  std::string DebugString() const;

  std::string_view Text() const { return content_; }

  // Returns the location of the Anchor's content in the original string.
  const std::optional<AnchorRange> &SourceTextRange() const {
    return source_text_range_;
  }

  bool operator==(const Anchor &) const;
  bool operator!=(const Anchor &other) const { return !(*this == other); }

 private:
  // Substring of the original text that corresponds to this Anchor.
  std::string content_;

  std::optional<AnchorRange> source_text_range_;
};

std::ostream &operator<<(std::ostream &, const Anchor &);

// This class is a simplified representation of CST and contains information
// that can be used for extracting indexing-facts for different indexing tools.
//
// This is intended to be an abstract layer between the parser generated CST
// and the indexing tool.
class IndexingNodeData {
 public:
  template <typename... Args>
  /* implicit */ IndexingNodeData(IndexingFactType language_feature,  // NOLINT
                                  Args &&...args)
      : indexing_fact_type_(language_feature) {
    AppendAnchor(std::forward<Args>(args)...);
  }

  IndexingNodeData(const IndexingNodeData &) = default;
  IndexingNodeData &operator=(IndexingNodeData &&) = default;

  // TODO(fangism): delete copy-ctor to make this move-only
  IndexingNodeData(IndexingNodeData &&) = default;
  IndexingNodeData &operator=(const IndexingNodeData &) = delete;

  // Consume an Anchor object(s), variadically.
  template <typename... Args>
  void AppendAnchor(Anchor &&anchor, Args &&...args) {
    anchors_.emplace_back(std::move(anchor));
    AppendAnchor(std::forward<Args>(args)...);
  }

  // Swaps the anchors with the given IndexingNodeData.
  void SwapAnchors(IndexingNodeData *other) { anchors_.swap(other->anchors_); }

  // Returns human readable view of this node.
  std::ostream &DebugString(std::ostream *stream) const;

  const std::vector<Anchor> &Anchors() const { return anchors_; }
  IndexingFactType GetIndexingFactType() const { return indexing_fact_type_; }

  // Redirects all non-owned string_views to point into a different copy of the
  // same text, located 'delta' away.  This is useful for testing, when source
  // text is copied to a different location.
  void RebaseStringViewsForTesting(std::ptrdiff_t delta);

  bool operator==(const IndexingNodeData &) const;
  bool operator!=(const IndexingNodeData &other) const {
    return !(*this == other);
  }

 private:
  // Base case for variadic AppendAnchor()
  void AppendAnchor() const {}

 private:
  // Represents which language feature this indexing fact is about.
  IndexingFactType indexing_fact_type_;

  // Anchors representing the different tokens of this indexing fact.
  std::vector<Anchor> anchors_;
};

// Without a base string_view, this displays the base-address and length of each
// Anchor's string_view.  See PrintableIndexingNodeData for a more readable
// alternative using byte-offsets.
std::ostream &operator<<(std::ostream &, const IndexingNodeData &);

// Pairs together IndexingNodeData and string_view to be a printable object.
struct PrintableIndexingNodeData {
  const IndexingNodeData &data;
  // The superstring of which all string_views in this subtree is a substring.
  const std::string_view base;

  PrintableIndexingNodeData(const IndexingNodeData &data, std::string_view base)
      : data(data), base(base) {}
};

// Human-readable form for debugging, showing in-file byte offsets of
// string_views.
std::ostream &operator<<(std::ostream &, const PrintableIndexingNodeData &);

// Renaming for VectorTree; IndexingFactNode is actually a VectorTree which is a
// class for constructing trees and dealing with them in a elegant manner.
using IndexingFactNode = verible::VectorTree<IndexingNodeData>;

struct PrintableIndexingFactNode {
  const IndexingFactNode &data;
  // The superstring of which all string_views in this subtree is a substring.
  const std::string_view base;

  PrintableIndexingFactNode(const IndexingFactNode &data, std::string_view base)
      : data(data), base(base) {}
};

// Human-readable form for debugging.
std::ostream &operator<<(std::ostream &, const PrintableIndexingFactNode &);

}  // namespace kythe
}  // namespace verilog

#endif  // VERIBLE_VERILOG_TOOLS_KYTHE_INDEXING_FACTS_TREE_H_
