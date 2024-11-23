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

#include "verible/verilog/analysis/json-diagnostics.h"

#include <memory>
#include <string>

#include "gtest/gtest.h"
#include "nlohmann/json.hpp"
#include "verible/common/util/logging.h"
#include "verible/verilog/analysis/verilog-analyzer.h"

namespace verilog {
namespace {

static void CheckJsonErrorItem(const nlohmann::json &json, const char *phase,
                               const char *text) {
  EXPECT_TRUE(json["column"].is_number());
  EXPECT_TRUE(json["line"].is_number());
  ASSERT_TRUE(json["phase"].is_string());
  EXPECT_EQ(json["phase"].get<std::string>(), phase);
  EXPECT_EQ(json["text"].get<std::string>(), text);
}

TEST(JsonDiagnosticsTest, LexError) {
  const auto analyzer_ptr = std::make_unique<VerilogAnalyzer>(
      "module 321foo;\nendmodule\n", "<noname>");
  const auto status = ABSL_DIE_IF_NULL(analyzer_ptr)->Tokenize();
  EXPECT_FALSE(status.ok());
  EXPECT_FALSE(analyzer_ptr->LexStatus().ok());

  const auto json = verilog::GetLinterTokenErrorsAsJson(analyzer_ptr.get(), 1);
  ASSERT_EQ(json.size(), 1);
  CheckJsonErrorItem(json[0], "lex", "321foo");
}

TEST(JsonDiagnosticsTest, ParseError) {
  const auto analyzer_ptr = std::make_unique<VerilogAnalyzer>(
      "module 1;endmodule\n"
      "module 2;endmodule\n",
      "<noname>");
  const auto status = ABSL_DIE_IF_NULL(analyzer_ptr)->Analyze();
  EXPECT_FALSE(status.ok());
  EXPECT_FALSE(analyzer_ptr->ParseStatus().ok());

  auto json = verilog::GetLinterTokenErrorsAsJson(analyzer_ptr.get(), 1);
  ASSERT_EQ(json.size(), 1);
  CheckJsonErrorItem(json[0], "parse", "1");

  json = verilog::GetLinterTokenErrorsAsJson(analyzer_ptr.get(), 2);
  ASSERT_EQ(json.size(), 2);
  CheckJsonErrorItem(json[0], "parse", "1");
  CheckJsonErrorItem(json[1], "parse", "endmodule");
}

}  // namespace
}  // namespace verilog
