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

#include "verible/verilog/analysis/checkers/package-filename-rule.h"

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "verible/common/analysis/lint-rule-status.h"
#include "verible/common/analysis/syntax-tree-search.h"
#include "verible/common/text/config-utils.h"
#include "verible/common/text/text-structure.h"
#include "verible/common/text/token-info.h"
#include "verible/common/util/file-util.h"
#include "verible/verilog/CST/package.h"
#include "verible/verilog/analysis/descriptions.h"
#include "verible/verilog/analysis/lint-rule-registry.h"

namespace verilog {
namespace analysis {

using verible::LintRuleStatus;
using verible::TextStructureView;

// Register the lint rule
VERILOG_REGISTER_LINT_RULE(PackageFilenameRule);

static constexpr std::string_view kOptionalSuffix = "_pkg";

static constexpr std::string_view kMessage =
    "Package declaration name must match the file name "
    "(ignoring optional \"_pkg\" file name suffix).  ";

const LintRuleDescriptor &PackageFilenameRule::GetDescriptor() {
  static const LintRuleDescriptor d{
      .name = "package-filename",
      .topic = "file-names",
      .desc =
          "Checks that the package name matches the filename. Depending on "
          "configuration, it is also allowed to replace underscore with dashes "
          "in filenames.",
      .param = {{"allow-dash-for-underscore", "false",
                 "Allow dashes in the filename corresponding to the "
                 "underscores in the package"}},
  };
  return d;
}

void PackageFilenameRule::Lint(const TextStructureView &text_structure,
                               std::string_view filename) {
  if (verible::file::IsStdin(filename)) {
    return;
  }

  const auto &tree = text_structure.SyntaxTree();
  if (tree == nullptr) return;

  // Find all package declarations.
  auto package_matches = FindAllPackageDeclarations(*tree);

  // See if names match the stem of the filename.
  //
  // Note:  package name | filename   | allowed ?
  //        -------------+------------+-----------
  //        foo          | foo.sv     | yes
  //        foo_bar      | foo_bar.sv | yes
  //        foo_bar      | foo-bar.sv | yes, if allow-dash-for-underscore
  //        foo          | foo_pkg.sv | yes
  //        foo          | foo-pkg.sv | yes, iff allow-dash-for-underscore
  //        foo_pkg      | foo_pkg.sv | yes
  //        foo_pkg      | foo.sv     | NO.
  const std::string_view basename =
      verible::file::Basename(verible::file::Stem(filename));
  std::vector<std::string_view> basename_components =
      absl::StrSplit(basename, '.');
  if (basename_components.empty() || basename_components[0].empty()) return;
  std::string unitname(basename_components[0].begin(),
                       basename_components[0].end());

  if (allow_dash_for_underscore_) {
    // If we allow for dashes, let's first convert them back to underscore.
    std::replace(unitname.begin(), unitname.end(), '-', '_');
  }

  // Report a violation on every package declaration, potentially.
  for (const auto &package_match : package_matches) {
    const verible::TokenInfo *package_name_token =
        GetPackageNameToken(*package_match.match);
    if (!package_name_token) continue;
    std::string_view package_id = package_name_token->text();
    auto package_id_plus_suffix = absl::StrCat(package_id, kOptionalSuffix);
    if ((package_id != unitname) && (package_id_plus_suffix != unitname)) {
      violations_.insert(verible::LintViolation(
          *package_name_token,
          absl::StrCat(kMessage, "declaration: \"", package_id,
                       "\" vs. basename(file): \"", unitname, "\"")));
    }
  }
}

LintRuleStatus PackageFilenameRule::Report() const {
  return LintRuleStatus(violations_, GetDescriptor());
}

absl::Status PackageFilenameRule::Configure(std::string_view configuration) {
  using verible::config::SetBool;
  return verible::ParseNameValues(
      configuration,
      {{"allow-dash-for-underscore", SetBool(&allow_dash_for_underscore_)}});
}
}  // namespace analysis
}  // namespace verilog
