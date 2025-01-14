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

// TextStructureLintRule implements a lint rule that operates on an entire
// TextStructureView.  This is appropriate for rules that need access to
// **more than one** of the following forms: lines, tokens, syntax-tree,
// or for rules that can implemented efficiently enough without having to
// traverse any of the aforementioned forms in their entirety.
// If your analysis only traverses one of those forms, it is more efficient
// to use a form-specific linter/lint_rule (LineLinter, SyntaxTreeLinter,
// TokenStreamLinter) because it traverses those forms in a single-pass over
// multiple rules.
//
// Use a TextStructureLintRule if:
// * Your analysis looks at both syntax tree and comments together.
// * Your analysis looks at only the first or last line.
// * Your analysis looks at only the leftmost/rightmost syntax tree nodes.

#ifndef VERIBLE_COMMON_ANALYSIS_TEXT_STRUCTURE_LINT_RULE_H_
#define VERIBLE_COMMON_ANALYSIS_TEXT_STRUCTURE_LINT_RULE_H_

#include <string_view>

#include "verible/common/analysis/lint-rule.h"
#include "verible/common/text/text-structure.h"

namespace verible {

class TextStructureLintRule : public LintRule {
 public:
  ~TextStructureLintRule() override = default;  // not yet final

  // Analyze text structure for violations.
  virtual void Lint(const TextStructureView &text_structure,
                    std::string_view filename) = 0;
};

}  // namespace verible

#endif  // VERIBLE_COMMON_ANALYSIS_TEXT_STRUCTURE_LINT_RULE_H_
