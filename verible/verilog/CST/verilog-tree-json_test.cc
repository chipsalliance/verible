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

#include "verible/verilog/CST/verilog-tree-json.h"

#include <memory>

#include "gtest/gtest.h"
#include "nlohmann/json.hpp"
#include "verible/common/text/symbol.h"
#include "verible/common/util/logging.h"
#include "verible/verilog/analysis/verilog-analyzer.h"

namespace verilog {
namespace {

using nlohmann::json;

TEST(VerilogTreeJsonTest, GeneratesGoodJsonTree) {
  const auto analyzer_ptr = std::make_unique<VerilogAnalyzer>(
      "module foo;\nendmodule\n", "fake_file.sv");
  const auto status = ABSL_DIE_IF_NULL(analyzer_ptr)->Analyze();
  EXPECT_TRUE(status.ok()) << status.message();
  const verible::SymbolPtr &tree_ptr = analyzer_ptr->SyntaxTree();
  ASSERT_NE(tree_ptr, nullptr);

  const json tree_json(verilog::ConvertVerilogTreeToJson(
      *tree_ptr, analyzer_ptr->Data().Contents()));

  const json expected_json = json::parse(R"({
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
             { "start": 7, "end": 10, "tag": "SymbolIdentifier", "text": "foo" },
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
  EXPECT_EQ(tree_json, expected_json);
}

}  // namespace
}  // namespace verilog
