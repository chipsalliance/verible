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

#include <string>

#include "gtest/gtest.h"
#include "common/analysis/linter_test_utils.h"
#include "common/analysis/text_structure_linter_test_utils.h"
#include "common/text/symbol.h"
#include "verilog/analysis/verilog_analyzer.h"
#include "verilog/parser/verilog_token_enum.h"

namespace verilog {
namespace analysis {
namespace {

using verible::LintTestCase;
using verible::RunLintTestCases;

constexpr int kToken = SymbolIdentifier;

// Test that no violations are found with an empty filename.
TEST(PackageFilenameRuleTest, BlankFilename) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {""},
      {"package m; endpackage"},
      {"class c; endclass"},
  };
  const std::string filename = "";
  RunLintTestCases<VerilogAnalyzer, PackageFilenameRule>(kTestCases, filename);
}

// Test that as every package that doesn't match begets a violation.
TEST(PackageFilenameRuleTest, PackageMatchesFilename) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {""},
      {"package m; endpackage"},
      {"package ", {kToken, "n"}, "; endpackage\npackage m; endpackage"},
      {"package m; endpackage\npackage ", {kToken, "n"}, "; endpackage"},
      {"package m; endpackage\n"
       "package ",
       {kToken, "n"},
       "; endpackage\n"
       "package ",
       {kToken, "o"},
       "; endpackage"},
  };
  const std::string filename = "/path/to/m.sv";
  RunLintTestCases<VerilogAnalyzer, PackageFilenameRule>(kTestCases, filename);
}

// Test that violations are reporter for every mismatch against absolute path.
TEST(PackageFilenameRuleTest, NoPackageMatchesFilenameAbsPath) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {""},
      {"package ", {kToken, "m"}, "; endpackage"},
      {"package ",
       {kToken, "m"},
       "; endpackage\npackage ",
       {kToken, "n"},
       "; endpackage"},
  };
  const std::string filename = "/path/to/q.sv";
  RunLintTestCases<VerilogAnalyzer, PackageFilenameRule>(kTestCases, filename);
}

// Test that some violations are found checked against a relative filename.
TEST(PackageFilenameRuleTest, NoPackageMatchesFilenameRelPath) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {""},
      {"package ", {kToken, "m"}, "; endpackage"},
      {"package ",
       {kToken, "m"},
       "; endpackage\npackage ",
       {kToken, "n"},
       "; endpackage"},
  };
  const std::string filename = "path/to/r.sv";
  RunLintTestCases<VerilogAnalyzer, PackageFilenameRule>(kTestCases, filename);
}

// Test that optional _pkg suffixes on declaration are forgiven.
TEST(PackageFilenameRuleTest, PackageMatchesOptionalDeclarationSuffix) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {"package q; endpackage"},
      {"package q_pkg; endpackage"},
  };
  const std::string filename = "/path/to/q.sv";
  RunLintTestCases<VerilogAnalyzer, PackageFilenameRule>(kTestCases, filename);
}

// Test that optional _pkg suffixes on filename are forgiven.
TEST(PackageFilenameRuleTest, PackageMatchesOptionalFileSuffix) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {"package q; endpackage"},
      {"package q_pkg; endpackage"},
  };
  const std::string filename = "/path/to/q_pkg.sv";
  RunLintTestCases<VerilogAnalyzer, PackageFilenameRule>(kTestCases, filename);
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
