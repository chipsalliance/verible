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

#include "verible/common/text/text-structure-test-utils.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/text-structure.h"
#include "verible/common/text/token-info.h"
#include "verible/common/text/token-stream-view.h"
#include "verible/common/text/tree-builder-test-util.h"
#include "verible/common/util/logging.h"

namespace verible {

std::string JoinLinesOfTokensIntoString(const LinesOfTokens &lines_of_tokens) {
  std::vector<absl::string_view> token_strings;
  std::vector<std::string> line_strings;
  line_strings.reserve(lines_of_tokens.size());
  // Concatenate string_views over all tokens and all lines.
  for (const auto &line : lines_of_tokens) {
    token_strings.clear();
    token_strings.resize(line.size());
    CHECK_EQ(line.back().text(), "\n");
    std::transform(line.begin(), line.end(), token_strings.begin(),
                   [](const TokenInfo &t) { return t.text(); });
    line_strings.push_back(absl::StrJoin(token_strings, ""));
  }
  return absl::StrJoin(line_strings, "");
}

TextStructureTokenized::TextStructureTokenized(
    const LinesOfTokens &lines_of_tokens)
    : TextStructure(JoinLinesOfTokensIntoString(lines_of_tokens)) {
  size_t offset = 0;
  auto &new_tokens = data_.MutableTokenStream();
  const auto joined_text = data_.Contents();
  for (const auto &line : lines_of_tokens) {
    for (const auto &token : line) {
      new_tokens.push_back(token);
      // Maintain the invariant that all tokens in a TextStructureView
      // must belong to the string owned by the TextStructure.
      new_tokens.back().RebaseStringView(
          joined_text.substr(offset, token.text().length()));
      offset += token.text().length();
    }
  }

  // Compute line-by-line partitions of token stream.
  data_.CalculateFirstTokensPerLine();
}

std::unique_ptr<TextStructureView> MakeTextStructureViewHelloWorld() {
  auto text_structure_view =
      std::make_unique<TextStructureView>("hello, world");
  TokenSequence &tokens = text_structure_view->MutableTokenStream();
  const absl::string_view text_view = text_structure_view->Contents();
  tokens.push_back(TokenInfo(0, text_view.substr(0, 5)));  // "hello"
  tokens.push_back(TokenInfo(1, text_view.substr(5, 1)));  // ","
  tokens.push_back(TokenInfo(2, text_view.substr(6, 1)));  // " "
  tokens.push_back(TokenInfo(3, text_view.substr(7, 5)));  // "world"
  TokenStreamView &view = text_structure_view->MutableTokenStreamView();
  view.push_back(tokens.begin());
  view.push_back(tokens.begin() + 1);
  view.push_back(tokens.begin() + 3);
  SymbolPtr syntax_tree =
      Node(Leaf(tokens[0]), Leaf(tokens[1]), Node(Leaf(tokens[3])));
  ConcreteSyntaxTree &mutable_tree = text_structure_view->MutableSyntaxTree();
  mutable_tree = std::move(syntax_tree);
  return text_structure_view;
}

// Return a text structure view with an empty string, and no tokens or tree
std::unique_ptr<TextStructureView> MakeTextStructureViewWithNoLeaves() {
  auto text_structure_view = std::make_unique<TextStructureView>("");
  SymbolPtr syntax_tree = Node(Node(), Node(), Node(Node(), Node()));
  ConcreteSyntaxTree &mutable_tree = text_structure_view->MutableSyntaxTree();
  mutable_tree = std::move(syntax_tree);
  return text_structure_view;
}

}  // namespace verible
