// Copyright 2017-2021 The Verible Authors.
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
// LowRISC format style.

#include "common/util/logging.h"
#include "verilog/formatting/style_compliance_report.h"
#include "verilog/formatting/lowrisc_format_style.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace verible {
namespace {

TEST(StyleComplianceReportTest, SphinxConfiguration) {
  StyleComplianceReport report;
  report.SetProjectName("project_name");
  report.SetCopyrights("copyrights");
  report.SetAuthors("authors");

  const auto& conf_str = report.BuildConfiguration();

  absl::string_view expected_conf =
      "import sphinx_rtd_theme\n"
      "\n"
      "project = 'project_name'\n"
      "copyright = 'copyrights'\n"
      "author = 'authors'\n"
      "\n"
      "exclude_patterns = []\n"
      "\n"
      "extensions = [ \"sphinx_rtd_theme\", ]\n"
      "\n"
      "html_theme = 'sphinx_rtd_theme'\n";

  EXPECT_EQ(conf_str, expected_conf);
}

TEST(StyleComplianceReportTest, DefaultSphinxConfiguration) {
  StyleComplianceReport report;

  const auto& conf_str = report.BuildConfiguration();

  absl::string_view expected_conf =
      "import sphinx_rtd_theme\n"
      "\n"
      "project = 'LowRISC style compliance report'\n"
      "copyright = '2017-2021, The Verible Authors'\n"
      "author = 'The Verible Authors'\n"
      "\n"
      "exclude_patterns = []\n"
      "\n"
      "extensions = [ \"sphinx_rtd_theme\", ]\n"
      "\n"
      "html_theme = 'sphinx_rtd_theme'\n";

  EXPECT_EQ(conf_str, expected_conf);
}

TEST(StyleComplianceReportTest, SphinxConfigurationChangeAuthors) {
  StyleComplianceReport report;
  report.SetProjectName("project_name");
  report.SetCopyrights("copyrights");
  report.SetAuthors("authors");

  absl::string_view expected_conf =
      "import sphinx_rtd_theme\n"
      "\n"
      "project = 'project_name'\n"
      "copyright = 'copyrights'\n"
      "author = 'authors'\n"
      "\n"
      "exclude_patterns = []\n"
      "\n"
      "extensions = [ \"sphinx_rtd_theme\", ]\n"
      "\n"
      "html_theme = 'sphinx_rtd_theme'\n";

  EXPECT_EQ(report.BuildConfiguration(), expected_conf);

  report.SetAuthors("different_authors");
  absl::string_view expected_conf_with_other_authors =
      "import sphinx_rtd_theme\n"
      "\n"
      "project = 'project_name'\n"
      "copyright = 'copyrights'\n"
      "author = 'different_authors'\n"
      "\n"
      "exclude_patterns = []\n"
      "\n"
      "extensions = [ \"sphinx_rtd_theme\", ]\n"
      "\n"
      "html_theme = 'sphinx_rtd_theme'\n";

  EXPECT_EQ(report.BuildConfiguration(),
            expected_conf_with_other_authors);
}

TEST(StyleComplianceReportTest, SphinxConfigurationChangeCopyrights) {
  StyleComplianceReport report;
  report.SetProjectName("project_name");
  report.SetCopyrights("copyrights");
  report.SetAuthors("authors");

  absl::string_view expected_conf =
      "import sphinx_rtd_theme\n"
      "\n"
      "project = 'project_name'\n"
      "copyright = 'copyrights'\n"
      "author = 'authors'\n"
      "\n"
      "exclude_patterns = []\n"
      "\n"
      "extensions = [ \"sphinx_rtd_theme\", ]\n"
      "\n"
      "html_theme = 'sphinx_rtd_theme'\n";

  EXPECT_EQ(report.BuildConfiguration(), expected_conf);

  report.SetCopyrights("different_copyrights");
  absl::string_view expected_conf_with_changed_copyrights =
      "import sphinx_rtd_theme\n"
      "\n"
      "project = 'project_name'\n"
      "copyright = 'different_copyrights'\n"
      "author = 'authors'\n"
      "\n"
      "exclude_patterns = []\n"
      "\n"
      "extensions = [ \"sphinx_rtd_theme\", ]\n"
      "\n"
      "html_theme = 'sphinx_rtd_theme'\n";

  EXPECT_EQ(report.BuildConfiguration(),
            expected_conf_with_changed_copyrights);
}

TEST(StyleComplianceReportTest, SphinxConfigurationChangeProjectName) {
  StyleComplianceReport report;
  report.SetProjectName("project_name");
  report.SetCopyrights("copyrights");
  report.SetAuthors("authors");

  absl::string_view expected_conf =
      "import sphinx_rtd_theme\n"
      "\n"
      "project = 'project_name'\n"
      "copyright = 'copyrights'\n"
      "author = 'authors'\n"
      "\n"
      "exclude_patterns = []\n"
      "\n"
      "extensions = [ \"sphinx_rtd_theme\", ]\n"
      "\n"
      "html_theme = 'sphinx_rtd_theme'\n";

  EXPECT_EQ(report.BuildConfiguration(), expected_conf);

  report.SetProjectName("different_project_name");
  absl::string_view expected_conf_with_changed_project_name =
      "import sphinx_rtd_theme\n"
      "\n"
      "project = 'different_project_name'\n"
      "copyright = 'copyrights'\n"
      "author = 'authors'\n"
      "\n"
      "exclude_patterns = []\n"
      "\n"
      "extensions = [ \"sphinx_rtd_theme\", ]\n"
      "\n"
      "html_theme = 'sphinx_rtd_theme'\n";

  EXPECT_EQ(report.BuildConfiguration(),
            expected_conf_with_changed_project_name);
}

TEST(StyleComplianceReportTest, TestHeader) {
  StyleComplianceReport report;

  //auto itr = report.begin();
  //EXPECT_EQ(itr, report.end());

  absl::string_view expected_header =
      ".. |hr| raw:: html\n"
      "\n"
      "    <hr />\n"
      "\n"
      "LowRISC style compliance report\n"
      "===============================\n"
      ".. toctree::\n"
      "   :maxdepth: 1\n"
      "   :caption: Contents:\n"
      "\n";

  // Just header
  //EXPECT_EQ(*itr, expected_header);

  EXPECT_EQ(report.BuildHeader(), expected_header);
}

//TEST(StyleComplianceReportTest, EmptyTestSuite) {
//  StyleComplianceReport report;
//
//  auto itr = report.begin();
//  EXPECT_EQ(itr, report.end());
//
//  absl::string_view expected_header =
//      ".. |hr| raw:: html\n"
//      "\n"
//      "    <hr />\n"
//      "\n"
//      "LowRISC style compliance report\n"
//      "===============================\n"
//      ".. toctree::\n"
//      "   :maxdepth: 1\n"
//      "   :caption: Contents:\n"
//      "\n";
//
//  // Just header
//  EXPECT_EQ(*itr, expected_header);
//}

// FIXME(ldk): Move to common/util/array_size.h
template<typename T, std::size_t N>
constexpr std::size_t ArraySize(T (&)[N]) noexcept {
  return N;
}

TEST(StyleComplianceReportTest, EmptyDescription) {
  StyleComplianceTestCase compliance_test_case("<internal>", "", "");

  const auto desc_map = compliance_test_case.GetDescription();
  EXPECT_EQ(desc_map.size(), 0);
}

TEST(StyleComplianceReportTest, DescriptionWithTitleOnly) {
  StyleComplianceTestCase compliance_test_case("<internal>", "Test title", "");

  const auto desc_map = compliance_test_case.GetDescription();
  EXPECT_EQ(desc_map.size(), 1);

  const auto itr = desc_map.find("title");
  EXPECT_NE(itr, desc_map.end());
  EXPECT_EQ(itr->second, "Test title");
}

TEST(StyleComplianceReportTest, DescriptionWithTitleAndTagsSameLine) {
  StyleComplianceTestCase compliance_test_case(
      "<internal>",
      "Test Title\n"
      "\n"
      "tags: test_tag1 test_tag2\n",
      "");


  const auto desc_map = compliance_test_case.GetDescription();
  EXPECT_EQ(desc_map.size(), 2);

  auto itr = desc_map.find("title");
  EXPECT_NE(itr, desc_map.end());
  EXPECT_EQ(itr->second, "Test Title");

  itr = desc_map.find("tags");
  EXPECT_NE(itr, desc_map.end());
  EXPECT_EQ(itr->second, "test_tag1 test_tag2");
}

TEST(StyleComplianceReportTest, DescriptionWithTitleAndTagsNextLine) {
  StyleComplianceTestCase compliance_test_case(
      "<internal>",
      "Test Title\n"
      "\n"
      "tags:\n"
      "test_tag1 test_tag2\n",
      "");

  const auto desc_map = compliance_test_case.GetDescription();
  EXPECT_EQ(desc_map.size(), 2);

  auto itr = desc_map.find("title");
  EXPECT_NE(itr, desc_map.end());
  EXPECT_EQ(itr->second, "Test Title");

  itr = desc_map.find("tags");
  EXPECT_NE(itr, desc_map.end());
  EXPECT_EQ(itr->second, "test_tag1 test_tag2");
}

TEST(StyleComplianceReportTest, DescriptionWithTitleAndMultiLineTag) {
  StyleComplianceTestCase compliance_test_case(
      "<internal>",
      "Test Title\n"
      "\n"
      "tags: test_tag1\n"
      "      test_tag2\n"
      "      test_tag3\n",
      "");

  const auto desc_map = compliance_test_case.GetDescription();
  EXPECT_EQ(desc_map.size(), 2);

  auto itr = desc_map.find("title");
  EXPECT_NE(itr, desc_map.end());
  EXPECT_EQ(itr->second, "Test Title");

  itr = desc_map.find("tags");
  EXPECT_NE(itr, desc_map.end());
  EXPECT_EQ(itr->second, "test_tag1\n"
                         "test_tag2\n"
                         "test_tag3");
}

TEST(StyleComplianceReportTest, DescriptionWithTitleAndMissingTag) {
  StyleComplianceTestCase compliance_test_case(
      "<internal>",
      "Test Title\n"
      "\n"
      "tags: test_tag\n"
      "\n"
      "ignored_tag\n",
      "");

  const auto desc_map = compliance_test_case.GetDescription();
  EXPECT_EQ(desc_map.size(), 2);

  auto itr = desc_map.find("title");
  EXPECT_NE(itr, desc_map.end());
  EXPECT_EQ(itr->second, "Test Title");

  itr = desc_map.find("info");
  EXPECT_EQ(itr, desc_map.end());

  itr = desc_map.find("unknown");
  EXPECT_EQ(itr, desc_map.end());

  itr = desc_map.find("tags");
  EXPECT_NE(itr, desc_map.end());
  EXPECT_EQ(itr->second, "test_tag");
}

TEST(StyleComplianceReportTest, DescriptionWithTitleAndInfo) {
  StyleComplianceTestCase compliance_test_case(
      "<internal>",
      "Test Title\n"
      "\n"
      "Info text\n"
      "\n"
      "tags: test_tag\n",
      "");

  const auto desc_map = compliance_test_case.GetDescription();
  EXPECT_EQ(desc_map.size(), 3);

  auto itr = desc_map.find("title");
  EXPECT_NE(itr, desc_map.end());
  EXPECT_EQ(itr->second, "Test Title");

  itr = desc_map.find("info");
  EXPECT_NE(itr, desc_map.end());
  EXPECT_EQ(itr->second, "Info text");

  itr = desc_map.find("unknown");
  EXPECT_EQ(itr, desc_map.end());

  itr = desc_map.find("tags");
  EXPECT_NE(itr, desc_map.end());
  EXPECT_EQ(itr->second, "test_tag");
}

TEST(StyleComplianceReportTest, DescriptionWithTitleAndInfoAndIgnored) {
  StyleComplianceTestCase compliance_test_case(
      "<internal>",
      "Test Title\n"
      "\n"
      "Info text\n"
      "\n"
      "tags: test_tag\n"
      "\n"
      "ignored text\n",
      "");

  const auto desc_map = compliance_test_case.GetDescription();
  EXPECT_EQ(desc_map.size(), 3);

  auto itr = desc_map.find("title");
  EXPECT_NE(itr, desc_map.end());
  EXPECT_EQ(itr->second, "Test Title");

  itr = desc_map.find("info");
  EXPECT_NE(itr, desc_map.end());
  EXPECT_EQ(itr->second, "Info text");

  EXPECT_EQ(desc_map.find("unknown"), desc_map.end());

  itr = desc_map.find("tags");
  EXPECT_NE(itr, desc_map.end());
  EXPECT_EQ(itr->second, "test_tag");
}

TEST(StyleComplianceReportTest, TestDefaultFormatStyleFromDescription) {
  StyleComplianceTestCase compliance_test_case("<internal>", "Title\n", "");

  const auto desc_map = compliance_test_case.GetDescription();
  EXPECT_EQ(desc_map.size(), 1);  // just test title

  const auto& style = compliance_test_case.GetStyle();
  EXPECT_EQ(style.StyleName(), "default");
}

TEST(StyleComplianceReportTest, TestFormatStyleFromDescription) {
  StyleComplianceTestCase compliance_test_case(
      "<internal>",
      "Title\n"
      "\n"
      "style: lowrisc\n",
      "");

  const auto desc_map = compliance_test_case.GetDescription();
  EXPECT_EQ(desc_map.size(), 2);  // title + style

  const auto& style = compliance_test_case.GetStyle();
  EXPECT_EQ(style.StyleName(), "lowrisc");

  // default column limit
  EXPECT_EQ(style.column_limit,
            verilog::formatter::LowRISCFormatStyle().column_limit);
}

TEST(StyleComplianceReportTest, TestColumnLimitOverride) {
  StyleComplianceTestCase compliance_test_case(
      "<internal>",
      "Title\n"
      "\n"
      "style: lowrisc column_limit=40\n",
      "");

  const auto desc_map = compliance_test_case.GetDescription();
  EXPECT_EQ(desc_map.size(), 2);  // title + style

  const auto& style = compliance_test_case.GetStyle();
  EXPECT_EQ(style.StyleName(), "lowrisc");
  EXPECT_EQ(style.column_limit, 40);
}

TEST(StyleComplianceReportTest, TestShouldFailTag) {
  StyleComplianceTestCase compliance_test_case(
      "<internal>",
      "test_title\n"
      "\n"
      "should_fail: true",
      "");

  const auto desc_map = compliance_test_case.GetDescription();
  EXPECT_EQ(desc_map.size(), 2);
  EXPECT_TRUE(compliance_test_case.ShouldFail());
}

TEST(StyleComplianceReportTest, TestShouldFailTagDefaultValue) {
  StyleComplianceTestCase compliance_test_case(
      "<internal>",
      "test_title\n",
      "");

  const auto desc_map = compliance_test_case.GetDescription();
  EXPECT_EQ(desc_map.size(), 1);
  EXPECT_FALSE(compliance_test_case.ShouldFail());
}

TEST(StyleComplianceReportTest, TestSimpleFormatting) {
  StyleComplianceTestCase test_case(
      "<internal>",
      "test_title",
      "module m;\n"
      "endmodule\n");

  EXPECT_TRUE(test_case.Format());
  EXPECT_FALSE(test_case.ShouldFail());
  EXPECT_TRUE(test_case.AsExpected());
}

TEST(StyleComplianceReportTest, TestSimpleFailingFormatting) {
  StyleComplianceTestCase test_case(
      "<internal>",
      "test_title",
      "module m;endmodule\n");

  EXPECT_TRUE(test_case.Format());
  EXPECT_FALSE(test_case.ShouldFail());
  EXPECT_FALSE(test_case.AsExpected());
}

// delete this test case?
//TEST(StyleComplianceReportTest, TestIgnoredFailingFormatting) {
//  StyleComplianceTestCase test_case(
//      "<internal>",
//      "test_title\n"
//      "\n"
//      "should_fail: true\n",
//      "module m;endmodule\n");
//
//  EXPECT_TRUE(test_case.Format());
//  EXPECT_TRUE(test_case.ShouldFail());
//  EXPECT_FALSE(test_case.AsExpected());
//}

}  // namespace
}  // namespace verible
