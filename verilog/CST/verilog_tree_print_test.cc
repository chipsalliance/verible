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

#include "gtest/gtest.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "common/text/symbol.h"
#include "verilog/analysis/verilog_analyzer.h"

namespace verilog {
namespace {

// Tests printing a Verilog syntax tree.
// This does not compare against any expected output string because it is prone
// to change.
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
  const std::string tree_output = stream.str();
  EXPECT_FALSE(tree_output.empty());
  EXPECT_NE(tree_output.find("\"module\""), std::string::npos);
  EXPECT_NE(tree_output.find("\"endmodule\""), std::string::npos);
}

}  // namespace
}  // namespace verilog
