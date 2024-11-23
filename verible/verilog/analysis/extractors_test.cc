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

#include "verible/verilog/analysis/extractors.h"

#include <set>
#include <string>
#include <utility>

#include "absl/strings/string_view.h"
#include "gtest/gtest.h"
#include "verible/verilog/preprocessor/verilog-preprocess.h"

#undef EXPECT_OK
#define EXPECT_OK(value) EXPECT_TRUE((value).ok())

#undef ASSERT_OK
#define ASSERT_OK(value) ASSERT_TRUE((value).ok())

namespace verilog {
namespace analysis {
namespace {
static constexpr VerilogPreprocess::Config kDefaultPreprocess;

TEST(CollectInterfaceNamesTest, NonModuleTests) {
  const std::pair<absl::string_view, std::set<std::string>> kTestCases[] = {
      {"", {}},
      {"class cls;\nendclass", {}},
      {"function f;\nendfunction", {}},
      {"package pkg;\nendpackage", {}},
  };
  for (const auto &test : kTestCases) {
    std::set<std::string> preserved;
    EXPECT_OK(
        CollectInterfaceNames(test.first, &preserved, kDefaultPreprocess));
    EXPECT_EQ(preserved, test.second);
  }
}

TEST(CollectInterfaceNamesTest, MinimalistModuleTests) {
  const std::pair<absl::string_view, std::set<std::string>> kTestCases[] = {
      {"module mod;\nendmodule", {"mod"}},
      {"module mod2(input foo);\nendmodule", {"mod2", "foo"}},
      {"module top\nimport pkg::*;\n(input a);\nendmodule",
       {"top", "pkg", "a"}},
      {"module mod #(parameter N = 0);\nendmodule: mod", {"mod", "N"}},
      {"module a;\nendmodule\nmodule b;\nendmodule", {"a", "b"}},
  };
  for (const auto &test : kTestCases) {
    std::set<std::string> preserved;
    EXPECT_OK(
        CollectInterfaceNames(test.first, &preserved, kDefaultPreprocess));
    EXPECT_EQ(preserved, test.second);
  }
}

// Tests that serveral instances with many identifiers are working as expected
TEST(CollectInterfaceNamesTest, BiggerModuleTests) {
  const std::pair<absl::string_view, std::set<std::string>> kTestCases[] = {
      {"interface TheBus(input clk);\n"
       "  logic [7:0] addr, wdata, rdata;\n"
       "  logic       write_en;\n"
       "endinterface\n"
       "module ram_model(clk, we, addr, rdata, wdata);\n"
       "  input        clk, we;\n"
       "  input  [7:0] addr, wdata;\n"
       "  output [7:0] rdata;\n"
       "  reg [7:0] mem [256];\n"
       "  assign rdata = mem[addr];\n"
       "  always @(posedge clk)\n"
       "    if (we)\n"
       "      mem[bus.addr] = wdata;\n"
       "endmodule\n"
       "module ram_wrapper(TheBus bus);\n"
       "  ram_model u_ram(\n"
       "    .clk(bus.clk),\n"
       "    .we(bus.write_en),\n"
       "    .addr(bus.addr),\n"
       "    .rdata(bus.rdata),\n"
       "    .wdata(bus.wdata)\n"
       "  );\n"
       "endmodule\n",
       {"ram_model", "clk", "we", "addr", "rdata", "wdata", "ram_wrapper",
        "TheBus", "bus"}},
  };
  for (const auto &test : kTestCases) {
    std::set<std::string> preserved;
    EXPECT_OK(
        CollectInterfaceNames(test.first, &preserved, kDefaultPreprocess));
    EXPECT_EQ(preserved, test.second);
  }
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
