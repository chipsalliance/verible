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

#ifndef VERIBLE_COMMON_FORMATTING_TREE_ANNOTATOR_H_
#define VERIBLE_COMMON_FORMATTING_TREE_ANNOTATOR_H_

#include <functional>
#include <vector>

#include "verible/common/formatting/format-token.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/syntax-tree-context.h"
#include "verible/common/text/token-info.h"

namespace verible {

// Parameters: left token, right token (modified),
// left token's syntax tree context,
// right token's syntax tree context.
using ContextTokenAnnotatorFunction =
    std::function<void(const PreFormatToken &, PreFormatToken *,
                       const SyntaxTreeContext &, const SyntaxTreeContext &)>;

// Applies inter-token formatting annotations, using syntactic context
// at every token.
void AnnotateFormatTokensUsingSyntaxContext(
    const Symbol *syntax_tree_root, const TokenInfo &eof_token,
    std::vector<PreFormatToken>::iterator tokens_begin,
    std::vector<PreFormatToken>::iterator tokens_end,
    const ContextTokenAnnotatorFunction &annotator);

}  // namespace verible

#endif  // VERIBLE_COMMON_FORMATTING_TREE_ANNOTATOR_H_
