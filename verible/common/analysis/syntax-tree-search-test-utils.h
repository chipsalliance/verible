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

#ifndef VERIBLE_COMMON_ANALYSIS_SYNTAX_TREE_SEARCH_TEST_UTILS_H_
#define VERIBLE_COMMON_ANALYSIS_SYNTAX_TREE_SEARCH_TEST_UTILS_H_

#include <initializer_list>
#include <iosfwd>
#include <vector>

#include "absl/strings/string_view.h"
#include "verible/common/analysis/syntax-tree-search.h"
#include "verible/common/lexer/lexer-test-util.h"
#include "verible/common/text/token-info-test-util.h"

namespace verible {

// SyntaxTreeSearchTestCase is a struct for describing a chunk of text and where
// a search is expected to match.  See SynthesizedLexerTestData for original
// concept.  This has the same limitations as SynthesizedLexerTestData, such as
// the inability to express nested findings, which requires a tree
// representation of expected data.
// TODO(fangism): Upgrade to nest-able expected findings (tree matcher).
struct SyntaxTreeSearchTestCase : public SynthesizedLexerTestData {
  // Forwarding constructor to base class.
  SyntaxTreeSearchTestCase(std::initializer_list<ExpectedTokenInfo> fragments)
      : SynthesizedLexerTestData(fragments) {}

  // Compare the set of expected findings against actual findings.
  // Detailed differences are written to diffstream.
  // 'base' is the full text buffer that was analyzed, and is used to
  // calculate byte offsets in diagnostics.
  // Matches with nullptr or empty string spans are ignored.
  // Returns true if every element is an exact match to the expected set.
  // TODO(b/141875806): Take a symbol translator function to produce a
  // human-readable, language-specific enum name.
  bool ExactMatchFindings(const std::vector<TreeSearchMatch> &actual_findings,
                          absl::string_view base,
                          std::ostream *diffstream) const;
};

}  // namespace verible

#endif  // VERIBLE_COMMON_ANALYSIS_SYNTAX_TREE_SEARCH_TEST_UTILS_H_
