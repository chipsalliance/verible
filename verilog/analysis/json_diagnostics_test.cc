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

#include "verilog/analysis/json_diagnostics.h"

#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"
#include "common/util/logging.h"
#include "gtest/gtest.h"
#include "json/json.h"
#include "verilog/analysis/verilog_analyzer.h"

namespace verilog {
namespace {

static void CheckJsonErrorItem(const Json::Value& json, const char* phase,
                               const char* text) {
  EXPECT_TRUE(json["column"].isIntegral());
  EXPECT_TRUE(json["line"].isIntegral());
  ASSERT_TRUE(json["phase"].isString());
  EXPECT_EQ(json["phase"].asString(), phase);
  ASSERT_TRUE(json["text"].isString());
  EXPECT_EQ(json["text"].asString(), text);
}

TEST(JsonDiagnosticsTest, LexError) {
  const auto analyzer_ptr = absl::make_unique<VerilogAnalyzer>(
      "module 321foo;\nendmodule\n", "<noname>");
  const auto status = ABSL_DIE_IF_NULL(analyzer_ptr)->Tokenize();
  EXPECT_FALSE(status.ok());
  EXPECT_FALSE(analyzer_ptr->LexStatus().ok());

  const Json::Value json =
      verilog::GetLinterTokenErrorsAsJson(analyzer_ptr.get());
  EXPECT_EQ(json.size(), 1);
  CheckJsonErrorItem(json[0], "lex", "321foo");
}

TEST(JsonDiagnosticsTest, ParseError) {
  const auto analyzer_ptr =
      absl::make_unique<VerilogAnalyzer>("a+", "<noname>");
  const auto status = ABSL_DIE_IF_NULL(analyzer_ptr)->Analyze();
  EXPECT_FALSE(status.ok());
  EXPECT_FALSE(analyzer_ptr->ParseStatus().ok());

  const Json::Value json =
      verilog::GetLinterTokenErrorsAsJson(analyzer_ptr.get());
  EXPECT_EQ(json.size(), 1);
  CheckJsonErrorItem(json[0], "parse", "+");
}

}  // namespace
}  // namespace verilog
