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
#include <iosfwd>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "common/text/token_info.h"
#include "common/util/vector_tree.h"
#include "verilog/tools/kythe/verilog_extractor_indexing_fact_type.h"

namespace verilog {
namespace kythe {

// Anchor class represents the location and value of some token.
class Anchor {
  // Disable implicit conversions when passing values to overloaded
  // function/constructor.  This work by virtue of the language only allowing
  // one implicit conversion at most.
  // e.g. some_function(DisableConversion<absl::string_view>)
  // accepts only string_views, while disabling string_view's implicit
  // conversion from std::string (and others).
  template <class T>
  struct DisableConversion {
    /* implicit */ DisableConversion(T v) : value(v) {}  // NOLINT

    operator T() const { return value; }  // NOLINT

    T value;
  };

 public:
  // Constructor based on string_view text that is a subtring of memory owned
  // elsewhere.  Disable implicit conversions from std::string (and others) to
  // absl::string_view to reduce the chances of accidentally holding onto memory
  // owned by a temporary/unnamed std::string that is destroyed immediately
  // after use. This construction should be used the vast majority of the time.
  // Alternatively, explicitly delete constructors from converted types:
  //   explicit Anchor(std::string&&) = delete;
  //   explicit Anchor(const std::string&&) = delete;
  explicit Anchor(DisableConversion<absl::string_view> value) : view_(value) {}

  // Delegates construction to use only the string_view spanned by a TokenInfo.
  // Recall the TokenInfo's string point to substrings of memory owned
  // elsewhere.
  explicit Anchor(const verible::TokenInfo& token) : Anchor(token.text()) {}

  // This constructor assumes ownership over the passed in string (which shall
  // not be nullptr).  This is suitable for rare occasions that warrant a
  // locally generated string that does not point to already owned memory.
  explicit Anchor(std::unique_ptr<std::string> owned_string)
      : owned_string_(std::move(owned_string)), view_(*owned_string_) {}

  Anchor(const Anchor&);  // TODO(fangism): delete, move-only
  Anchor(Anchor&&) = default;
  Anchor& operator=(const Anchor&) = delete;
  Anchor& operator=(Anchor&&) = delete;

  // This function is for debugging only and isn't intended to be a textual
  // representation of this class.
  // 'base' is the superstring of which Anchor's text is a substring.
  std::string DebugString(absl::string_view base) const;

  absl::string_view Text() const { return view_; }

  // Redirects all non-owned string_view to point into a different copy of the
  // same text, located 'delta' away.  This is useful for testing, when source
  // text is copied to a different location. For owned strings (if
  // this->OwnsMemory()), this does nothing.
  void RebaseStringViewForTesting(std::ptrdiff_t delta);

  bool operator==(const Anchor&) const;
  bool operator!=(const Anchor& other) const { return !(*this == other); }

 protected:
  // Returns true if the text represented is owned by this object.
  bool OwnsMemory() const { return owned_string_ != nullptr; }

 private:
  // The majority of times, this is null when 'view_' points to text that is
  // owned elsewhere (and outlives these objects).
  // This pointer should only be used in rare circumstances that require a newly
  // constructed string (e.g. generated name).
  // Size: 1 pointer
  std::unique_ptr<std::string> owned_string_;

  // Most of the time, points to a substring of text that belongs to a file's
  // contents, without actually copying text.
  // In rare circumstances, this points to generated text belonging to
  // 'owned_string_'.
  // Size: 2 pointers (range bounds)
  absl::string_view view_;
};

std::ostream& operator<<(std::ostream&, const Anchor&);

// This class is a simplified representation of CST and contains information
// that can be used for extracting indexing-facts for different indexing tools.
//
// This is intended to be an abstract layer between the parser generated CST
// and the indexing tool.
class IndexingNodeData {
 public:
  template <typename... Args>
  /* implicit */ IndexingNodeData(IndexingFactType language_feature,  // NOLINT
                                  Args&&... args)
      : indexing_fact_type_(language_feature) {
    AppendAnchor(std::forward<Args>(args)...);
  }

  IndexingNodeData(const IndexingNodeData&) = default;
  IndexingNodeData& operator=(IndexingNodeData&&) = default;

  // TODO(fangism): delete copy-ctor to make this move-only
  IndexingNodeData(IndexingNodeData&&) = default;
  IndexingNodeData& operator=(const IndexingNodeData&) = delete;

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
  std::ostream& DebugString(std::ostream* stream, absl::string_view base) const;

  const std::vector<Anchor>& Anchors() const { return anchors_; }
  IndexingFactType GetIndexingFactType() const { return indexing_fact_type_; }

  // Redirects all non-owned string_views to point into a different copy of the
  // same text, located 'delta' away.  This is useful for testing, when source
  // text is copied to a different location.
  void RebaseStringViewsForTesting(std::ptrdiff_t delta);

  bool operator==(const IndexingNodeData&) const;
  bool operator!=(const IndexingNodeData& other) const {
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
std::ostream& operator<<(std::ostream&, const IndexingNodeData&);

// Pairs together IndexingNodeData and string_view to be a printable object.
struct PrintableIndexingNodeData {
  const IndexingNodeData& data;
  // The superstring of which all string_views in this subtree is a substring.
  const absl::string_view base;

  PrintableIndexingNodeData(const IndexingNodeData& data,
                            absl::string_view base)
      : data(data), base(base) {}
};

// Human-readable form for debugging, showing in-file byte offsets of
// string_views.
std::ostream& operator<<(std::ostream&, const PrintableIndexingNodeData&);

// Renaming for VectorTree; IndexingFactNode is actually a VectorTree which is a
// class for constructing trees and dealing with them in a elegant manner.
using IndexingFactNode = verible::VectorTree<IndexingNodeData>;

struct PrintableIndexingFactNode {
  const IndexingFactNode& data;
  // The superstring of which all string_views in this subtree is a substring.
  const absl::string_view base;

  PrintableIndexingFactNode(const IndexingFactNode& data,
                            absl::string_view base)
      : data(data), base(base) {}
};

// Human-readable form for debugging.
std::ostream& operator<<(std::ostream&, const PrintableIndexingFactNode&);

}  // namespace kythe
}  // namespace verilog

#endif  // VERIBLE_VERILOG_TOOLS_KYTHE_INDEXING_FACTS_TREE_H_
