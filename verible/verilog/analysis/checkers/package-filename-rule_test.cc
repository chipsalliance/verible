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

#include <initializer_list>
#include <string>
#include <string_view>

#include "gtest/gtest.h"
#include "verible/common/analysis/linter-test-utils.h"
#include "verible/common/analysis/text-structure-linter-test-utils.h"
#include "verible/verilog/analysis/verilog-analyzer.h"
#include "verible/verilog/parser/verilog-token-enum.h"

namespace verilog {
namespace analysis {
namespace {

using verible::LintTestCase;
using verible::RunConfiguredLintTestCases;
using verible::RunLintTestCases;

constexpr int kToken = SymbolIdentifier;

// Test that no violations are found with an empty filename.
TEST(PackageFilenameRuleTest, BlankFilename) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {""},
      {"package m; endpackage"},
      {"class c; endclass"},
  };
  const std::string filename;
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
      {"package ", {kToken, "m_pkg"}, "; endpackage"},
  };
  const std::string filename = "/path/to/m.sv";
  RunLintTestCases<VerilogAnalyzer, PackageFilenameRule>(kTestCases, filename);
}

// Test that as every package that doesn't match begets a violation.
TEST(PackageFilenameRuleTest, PackagePlusPkgMatchesFilename) {
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
      {"package m_pkg; endpackage"},
  };
  const std::string filename = "/path/to/m_pkg.sv";
  RunLintTestCases<VerilogAnalyzer, PackageFilenameRule>(kTestCases, filename);
}

// Test that we correctly discard everything in the filename after the first
// dot.
TEST(PackageFilenameRuleTest, UnitNameIsFilenameBeforeTheFirstDot) {
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
      {"package m_pkg; endpackage"},
  };
  const std::string filename = "/path/to/m_pkg.sv";
  RunLintTestCases<VerilogAnalyzer, PackageFilenameRule>(kTestCases, filename);
  const std::string filename2 = "/path/to/m_pkg.this.is.junk.sv";
  RunLintTestCases<VerilogAnalyzer, PackageFilenameRule>(kTestCases, filename2);
}

TEST(PackageFilenameRuleTest, LegalPackagesForFooPkgSv) {
  const std::initializer_list<LintTestCase> kTestCasesForFooPkgSv = {
      {"package foo; endpackage"},
      {"package foo_pkg; endpackage"},
  };
  const std::string foo_pkg_sv = "/path/to/foo_pkg.sv";
  RunLintTestCases<VerilogAnalyzer, PackageFilenameRule>(kTestCasesForFooPkgSv,
                                                         foo_pkg_sv);
}

TEST(PackageFilenameRuleTest, LegalPackagesForFooSv) {
  const std::initializer_list<LintTestCase> kTestCasesForFooSv = {
      {"package foo; endpackage"},
      {"package ", {kToken, "foo_pkg"}, "; endpackage"},
  };
  const std::string foo_sv = "/path/to/foo.sv";
  RunLintTestCases<VerilogAnalyzer, PackageFilenameRule>(kTestCasesForFooSv,
                                                         foo_sv);
}

TEST(PackageFilenameRuleTest, LegalPackagesForFooPkgPkgSv) {
  // It is weird, but legal, to put package "foo_pkg" in "foo_pkg_pkg.sv".
  const std::initializer_list<LintTestCase> kTestCasesForFooPkgPkgSv = {
      {"package ", {kToken, "foo"}, "; endpackage"},
      {"package foo_pkg; endpackage"},
      {"package foo_pkg_pkg; endpackage"},
      {"package ", {kToken, "foo_pkg_pkg_pkg"}, "; endpackage"},
  };
  const std::string foo_pkg_pkg_sv = "/path/to/foo_pkg_pkg.sv";
  RunLintTestCases<VerilogAnalyzer, PackageFilenameRule>(
      kTestCasesForFooPkgPkgSv, foo_pkg_pkg_sv);
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

// Test that optional _pkg suffixes on declaration are illegal.
TEST(PackageFilenameRuleTest, PackageMatchesOptionalDeclarationSuffix) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {"package q; endpackage"},
      {"package ", {kToken, "q_pkg"}, "; endpackage"},
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

TEST(PackageFilenameRuleTest, DashAllowedWhenConfigured) {
  const std::initializer_list<LintTestCase> kOkCases = {
      {"package foo_bar; endpackage"},
  };
  const std::initializer_list<LintTestCase> kComplaintCases = {
      {"package ", {kToken, "foo_bar"}, "; endpackage"},
  };

  const std::string f_with_underscore = "/path/to/foo_bar.sv";
  const std::string f_with_dash = "/path/to/foo-bar.sv";
  const std::string f_with_dash_pkg = "/path/to/foo-bar-pkg.sv";

  {
    // With dashes not allowed, we only accept the underscore name
    constexpr std::string_view config = "allow-dash-for-underscore:off";
    RunConfiguredLintTestCases<VerilogAnalyzer, PackageFilenameRule>(
        kOkCases, config, f_with_underscore);
    RunConfiguredLintTestCases<VerilogAnalyzer, PackageFilenameRule>(
        kComplaintCases, config, f_with_dash);
    RunConfiguredLintTestCases<VerilogAnalyzer, PackageFilenameRule>(
        kComplaintCases, config, f_with_dash_pkg);
  }

  {
    // ... But with dashes allowed, dashes are also an ok case.
    constexpr std::string_view config = "allow-dash-for-underscore:on";
    // With dashes not allowed, we only accept the underscore name
    RunConfiguredLintTestCases<VerilogAnalyzer, PackageFilenameRule>(
        kOkCases, config, f_with_underscore);
    RunConfiguredLintTestCases<VerilogAnalyzer, PackageFilenameRule>(
        kOkCases, config, f_with_dash);
    RunConfiguredLintTestCases<VerilogAnalyzer, PackageFilenameRule>(
        kOkCases, config, f_with_dash_pkg);
  }
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
