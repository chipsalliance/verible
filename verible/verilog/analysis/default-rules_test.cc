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

#include "verible/verilog/analysis/default-rules.h"

#include "gtest/gtest.h"
#include "verible/verilog/analysis/lint-rule-registry.h"

namespace verilog {
namespace analysis {
namespace {

// Test that rules in the default set are all registered.
TEST(DefaultRuleTest, AllDefaultValid) {
  for (const auto &rule_id : kDefaultRuleSet) {
    EXPECT_TRUE(IsRegisteredLintRule(rule_id))
        << "Registry is missing rule_id: " << rule_id;
  }
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
