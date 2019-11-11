// Copyright 2017-2019 The Verible Authors.
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

#include "verilog/analysis/checkers/package_filename_rule.h"

#include <memory>
#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/strings/strip.h"
#include "common/analysis/citation.h"
#include "common/analysis/lint_rule_status.h"
#include "common/analysis/syntax_tree_search.h"
#include "common/text/text_structure.h"
#include "common/text/token_info.h"
#include "common/util/file_util.h"
#include "common/util/statusor.h"
#include "verilog/CST/package.h"
#include "verilog/analysis/descriptions.h"
#include "verilog/analysis/lint_rule_registry.h"

namespace verilog {
namespace analysis {

using verible::GetStyleGuideCitation;
using verible::LintRuleStatus;
using verible::TextStructureView;

// Register the lint rule
VERILOG_REGISTER_LINT_RULE(PackageFilenameRule);

static const char optional_suffix[] = "_pkg";

absl::string_view PackageFilenameRule::Name() { return "package-filename"; }
const char PackageFilenameRule::kTopic[] = "file-names";
const char PackageFilenameRule::kMessage[] =
    "Package declaration name must match the file name "
    "(ignoring optional \"_pkg\" suffix).  ";

std::string PackageFilenameRule::GetDescription(
    DescriptionType description_type) {
  return absl::StrCat("Checks that the package name matches the filename. See ",
                      GetStyleGuideCitation(kTopic), ".");
}

void PackageFilenameRule::Lint(const TextStructureView& text_structure,
                               absl::string_view filename) {
  const auto& tree = text_structure.SyntaxTree();
  if (tree == nullptr) return;

  // Find all package declarations.
  auto package_matches = FindAllPackageDeclarations(*tree);

  // See if names match the stem of the filename.
  const absl::string_view basename =
      verible::file::Basename(verible::file::Stem(filename));
  const absl::string_view file_stem =
      absl::StripSuffix(basename, optional_suffix);
  if (file_stem.empty()) return;

  // Report a violation on every package declaration, potentially.
  for (const auto& package_match : package_matches) {
    const verible::TokenInfo package_name_token =
        GetPackageNameToken(*package_match.match).ValueOrDie();
    absl::string_view package_id = package_name_token.text;
    auto package_stem = absl::StripSuffix(package_id, optional_suffix);
    if (package_stem != file_stem) {
      violations_.push_back(verible::LintViolation(
          package_name_token,
          absl::StrCat(kMessage, "declaration: \"", package_id,
                       "\" vs. basename(file): \"", basename, "\"")));
    }
    // TODO(jonmayer): Also reject package foo_pkg vs. foo.sv case after
    // clarifying the style guide.
  }
}

LintRuleStatus PackageFilenameRule::Report() const {
  return LintRuleStatus(violations_, Name(), GetStyleGuideCitation(kTopic));
}

}  // namespace analysis
}  // namespace verilog
