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

#include "verilog/CST/verilog_tree_print.h"

#include <memory>
#include <sstream>  // IWYU pragma: keep  // for ostringstream
#include <string>

#include "absl/memory/memory.h"
#include "common/text/symbol.h"
#include "gtest/gtest.h"
#include "verilog/analysis/verilog_analyzer.h"

namespace verilog {
namespace {

// Tests printing a Verilog syntax tree.
TEST(VerilogTreePrintTest, Prints) {
  const char input[] = "module foo;\nendmodule\n";
  const auto analyzer =
      absl::make_unique<VerilogAnalyzer>(input, "fake_file.sv");
  const auto status = analyzer->Analyze();
  EXPECT_TRUE(status.ok());
  std::ostringstream stream;
  EXPECT_TRUE(stream.str().empty());
  const std::unique_ptr<verible::Symbol>& tree_ptr = analyzer->SyntaxTree();
  ASSERT_NE(tree_ptr, nullptr);

  PrettyPrintVerilogTree(*tree_ptr, analyzer->Data().Contents(), &stream);

  const std::string tree_output(stream.str());
  EXPECT_EQ(tree_output, R"(Node @0 (tag: kDescriptionList) {
  Node @0 (tag: kModuleDeclaration) {
    Node @0 (tag: kModuleHeader) {
      Leaf @0 (#"module" @0-6: "module")
      Leaf @2 (#SymbolIdentifier @7-10: "foo")
      Leaf @7 (#';' @10-11: ";")
    }
    Node @1 (tag: kModuleItemList) {
    }
    Leaf @2 (#"endmodule" @12-21: "endmodule")
  }
}
)");
}

}  // namespace
}  // namespace verilog
