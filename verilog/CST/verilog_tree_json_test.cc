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

#include "verilog/CST/verilog_tree_json.h"

#include <memory>

#include "absl/strings/string_view.h"
#include "common/text/symbol.h"
#include "common/util/logging.h"
#include "gtest/gtest.h"
#include "json/value.h"
#include "verilog/analysis/verilog_analyzer.h"

namespace verilog {
namespace {

static Json::Value ParseJson(absl::string_view text) {
  Json::Value json;
  std::unique_ptr<Json::CharReader> reader(
      Json::CharReaderBuilder().newCharReader());
  reader->parse(text.begin(), text.end(), &json, nullptr);
  return json;
}

TEST(VerilogTreeJsonTest, GeneratesGoodJsonTree) {
  const auto analyzer_ptr = absl::make_unique<VerilogAnalyzer>(
      "module foo;\nendmodule\n", "fake_file.sv");
  const auto status = ABSL_DIE_IF_NULL(analyzer_ptr)->Analyze();
  EXPECT_TRUE(status.ok()) << status.message();
  const std::unique_ptr<verible::Symbol>& tree_ptr = analyzer_ptr->SyntaxTree();
  ASSERT_NE(tree_ptr, nullptr);

  const Json::Value json(verilog::ConvertVerilogTreeToJson(
      *tree_ptr, analyzer_ptr->Data().Contents()));

  const Json::Value expected_json = ParseJson(R"({
    "tag": "kDescriptionList",
    "children": [
      {
        "tag": "kModuleDeclaration",
        "children": [
         {
           "tag": "kModuleHeader",
           "children": [
             { "start": 0, "end": 6, "tag": "module" },
             null,
             { "start": 7, "end": 10, "tag": "SymbolIdentifier" },
             null,
             null,
             null,
             null,
             { "start": 10, "end": 11, "tag": ";" }
           ]
         },
         { "tag": "kModuleItemList", "children": [] },
         { "start": 12, "end": 21, "tag": "endmodule" },
         null
        ]
      }
    ]
  })");
  EXPECT_EQ(json, expected_json);
}

}  // namespace
}  // namespace verilog
