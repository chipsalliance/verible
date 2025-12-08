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

// Test MacroDefinition and its supporting structs.

#include "verible/common/text/macro-definition.h"

#include <vector>

#include "absl/status/status.h"
#include "gtest/gtest.h"
#include "verible/common/text/token-info.h"
#include "verible/common/util/container-util.h"

namespace verible {
namespace {

using verible::container::FindOrNull;

// Fake token enumerations that would come from parser.tab.hh.
enum FakeTokenEnum {
  FakeIdEnum = 1,
  FakeIntEnum = 2,
  FakeDefineEnum = 3,
  FakeUnlexedTextEnum = 4,
  FakeOtherEnum = 5,
};

// Test default constructed MacroParameterInfo
TEST(MacroParameterInfoTest, Constructor) {
  const MacroParameterInfo param_info;
  EXPECT_FALSE(param_info.HasDefaultText());
  EXPECT_TRUE(param_info.name.isEOF());
  EXPECT_TRUE(param_info.default_value.isEOF());
}

// Test named MacroParameterInfo without default value.
TEST(MacroParameterInfoTest, WithoutDefault) {
  const TokenInfo param_name(FakeIdEnum, "name");
  const MacroParameterInfo param_info{param_name};
  EXPECT_FALSE(param_info.HasDefaultText());
  EXPECT_EQ(param_info.name, param_name);
  EXPECT_TRUE(param_info.default_value.isEOF());
}

// Test named MacroParameterInfo with default value.
TEST(MacroParameterInfoTest, WithDefault) {
  const TokenInfo param_name(FakeIdEnum, "name");
  const TokenInfo param_default(FakeIntEnum, "0");
  const MacroParameterInfo param_info{param_name, param_default};
  EXPECT_TRUE(param_info.HasDefaultText());
  EXPECT_EQ(param_info.name, param_name);
  EXPECT_EQ(param_info.default_value, param_default);
}

// Test default constructed MacroCall.
TEST(MacroCallTest, Constructor) {
  const MacroCall macro_call;
  EXPECT_TRUE(macro_call.macro_name.isEOF());
  EXPECT_FALSE(macro_call.has_parameters);
  EXPECT_EQ(macro_call.positional_arguments.size(), 0);
}

// Test default constructed MacroDefinition.
TEST(MacroDefinitionTest, Constructor) {
  const TokenInfo def_header(FakeDefineEnum, "`define");
  const TokenInfo macro_name(FakeIdEnum, "FF");
  const MacroDefinition macro(def_header, macro_name);
  EXPECT_EQ(macro.Name(), macro_name.text());
  EXPECT_FALSE(macro.IsCallable());
}

// Tests attaching definition text to MacroDefinition.
TEST(MacroDefinitionTest, DefinitionText) {
  const TokenInfo def_header(FakeDefineEnum, "`define");
  const TokenInfo macro_name(FakeIdEnum, "FF");
  const TokenInfo body(FakeUnlexedTextEnum, "foo + bar");
  MacroDefinition macro(def_header, macro_name);
  macro.SetDefinitionText(body);
  EXPECT_EQ(macro.DefinitionText(), body);
}

// Tests a MacroDefinition with no parameters.
TEST(MacroDefinitionTest, CallableNoArgs) {
  const TokenInfo def_header(FakeDefineEnum, "`define");
  const TokenInfo macro_name(FakeIdEnum, "FF");
  MacroDefinition macro(def_header, macro_name);
  macro.SetCallable();
  EXPECT_TRUE(macro.IsCallable());
}

// Tests a MacroDefinition with one parameter.
TEST(MacroDefinitionTest, CallableOneArg) {
  const TokenInfo def_header(FakeDefineEnum, "`define");
  const TokenInfo macro_name(FakeIdEnum, "FF");
  MacroDefinition macro(def_header, macro_name);
  const TokenInfo param_name(FakeIdEnum, "clk");
  const MacroParameterInfo param{param_name};
  const bool appended = macro.AppendParameter(param);
  EXPECT_TRUE(appended);
  EXPECT_TRUE(macro.IsCallable());
}

// Tests a MacroDefinition with one parameter with default value.
TEST(MacroDefinitionTest, CallableOneArgDefault) {
  const TokenInfo def_header(FakeDefineEnum, "`define");
  const TokenInfo macro_name(FakeIdEnum, "FF");
  MacroDefinition macro(def_header, macro_name);
  const TokenInfo param_name(FakeIdEnum, "clk");
  const TokenInfo param_default(FakeIdEnum, "CLK");
  const MacroParameterInfo param{param_name, param_default};
  const bool appended = macro.AppendParameter(param);
  EXPECT_TRUE(appended);
  EXPECT_TRUE(macro.IsCallable());
}

// Tests that duplicate parameter names are rejected.
TEST(MacroDefinitionTest, CallableRepeatedArgRejected) {
  const TokenInfo def_header(FakeDefineEnum, "`define");
  const TokenInfo macro_name(FakeIdEnum, "FF");
  MacroDefinition macro(def_header, macro_name);
  {
    const TokenInfo param_name(FakeIdEnum, "clk");
    const MacroParameterInfo param{param_name};
    const bool appended = macro.AppendParameter(param);
    EXPECT_TRUE(appended);
    EXPECT_TRUE(macro.IsCallable());
  }
  {
    const TokenInfo param_name(FakeIdEnum, "clk");
    const MacroParameterInfo param{param_name};
    const bool appended = macro.AppendParameter(param);
    EXPECT_FALSE(appended);
    EXPECT_TRUE(macro.IsCallable());
  }
}

// Tests that non-conflicting parameters are accepted.
TEST(MacroDefinitionTest, CallableTwoArgs) {
  const TokenInfo def_header(FakeDefineEnum, "`define");
  const TokenInfo macro_name(FakeIdEnum, "FF");
  MacroDefinition macro(def_header, macro_name);
  {
    const TokenInfo param_name(FakeIdEnum, "clk");
    const MacroParameterInfo param{param_name};
    const bool appended = macro.AppendParameter(param);
    EXPECT_TRUE(appended);
    EXPECT_TRUE(macro.IsCallable());
  }
  {
    const TokenInfo param_name(FakeIdEnum, "data");
    const MacroParameterInfo param{param_name};
    const bool appended = macro.AppendParameter(param);
    EXPECT_TRUE(appended);
    EXPECT_TRUE(macro.IsCallable());
  }
}

// Tests creating a token substitution map with parameterless macro.
TEST(MacroDefinitionTest, PopulateSubstitutionMapNonCallable) {
  const TokenInfo def_header(FakeDefineEnum, "`define");
  const TokenInfo macro_name(FakeIdEnum, "FF");
  MacroDefinition macro(def_header, macro_name);
  std::vector<TokenInfo> call_args;
  MacroDefinition::substitution_map_type sub_map;
  const auto status = macro.PopulateSubstitutionMap(call_args, &sub_map);
  EXPECT_TRUE(status.ok());
  auto not_param = FindOrNull(sub_map, "dock");
  EXPECT_EQ(not_param, nullptr);
}

// Tests creating a token substitution map with a 1-parameter macro.
TEST(MacroDefinitionTest, PopulateSubstitutionMapOneParam) {
  const TokenInfo def_header(FakeDefineEnum, "`define");
  const TokenInfo macro_name(FakeIdEnum, "FF");
  MacroDefinition macro(def_header, macro_name);
  {
    const TokenInfo param_name(FakeIdEnum, "clk");
    const MacroParameterInfo param{param_name};
    const bool appended = macro.AppendParameter(param);
    EXPECT_TRUE(appended);
  }
  std::vector<TokenInfo> call_args;
  const TokenInfo actual1(FakeIntEnum, "99");
  call_args.push_back(actual1);
  MacroDefinition::substitution_map_type sub_map;
  {
    const auto status = macro.PopulateSubstitutionMap(call_args, &sub_map);
    EXPECT_TRUE(status.ok());
    auto not_param = FindOrNull(sub_map, "dock");
    EXPECT_EQ(not_param, nullptr);
    auto expect_param = FindOrNull(sub_map, "clk");
    EXPECT_EQ(*expect_param, actual1);
  }
  // checking substitution
  {
    const TokenInfo num(FakeIntEnum, "732");
    const TokenInfo repl = MacroDefinition::SubstituteText(sub_map, num);
    EXPECT_EQ(repl, num);  // no substitution
  }
  {
    const TokenInfo num(FakeIdEnum, "clk");
    const TokenInfo repl = MacroDefinition::SubstituteText(sub_map, num);
    EXPECT_EQ(repl, actual1);  // substituted
  }
  {
    const TokenInfo num(FakeIdEnum, "rst");
    const TokenInfo repl = MacroDefinition::SubstituteText(sub_map, num);
    EXPECT_EQ(repl, num);  // not substituted
  }
  {
    const TokenInfo num(FakeOtherEnum, "clk");
    const TokenInfo repl = MacroDefinition::SubstituteText(sub_map, num);
    EXPECT_EQ(repl, actual1);  // substituted
  }
  {
    const TokenInfo num(FakeOtherEnum, "clk");
    const TokenInfo repl =
        MacroDefinition::SubstituteText(sub_map, num, FakeIdEnum);
    EXPECT_EQ(repl, num);  // not substituted
  }
  {
    const TokenInfo num(FakeOtherEnum, "rst");
    const TokenInfo repl =
        MacroDefinition::SubstituteText(sub_map, num, FakeIdEnum);
    EXPECT_EQ(repl, num);  // not substituted
  }
}

// Tests creating a token substitution map with a 2-parameter macro.
TEST(MacroDefinitionTest, PopulateSubstitutionMapTwoParams) {
  const TokenInfo def_header(FakeDefineEnum, "`define");
  const TokenInfo macro_name(FakeIdEnum, "FF");
  MacroDefinition macro(def_header, macro_name);
  {
    const TokenInfo param_name(FakeIdEnum, "clk");
    const MacroParameterInfo param{param_name};
    const bool appended = macro.AppendParameter(param);
    EXPECT_TRUE(appended);
  }
  {
    const TokenInfo param_name(FakeIdEnum, "rstn");
    const MacroParameterInfo param{param_name};
    const bool appended = macro.AppendParameter(param);
    EXPECT_TRUE(appended);
  }
  std::vector<TokenInfo> call_args;
  const TokenInfo actual1(FakeIntEnum, "99");
  const TokenInfo actual2(FakeIdEnum, "_rst_");
  call_args.push_back(actual1);
  call_args.push_back(actual2);
  MacroDefinition::substitution_map_type sub_map;
  const auto status = macro.PopulateSubstitutionMap(call_args, &sub_map);
  EXPECT_TRUE(status.ok());
  auto not_param = FindOrNull(sub_map, "dock");
  EXPECT_EQ(not_param, nullptr);
  auto expect_param1 = FindOrNull(sub_map, "clk");
  EXPECT_EQ(*expect_param1, actual1);
  auto expect_param2 = FindOrNull(sub_map, "rstn");
  EXPECT_EQ(*expect_param2, actual2);
}

// Tests failed substitution map creation due to mismatch on parameters.
TEST(MacroDefinitionTest, PopulateSubstitutionMapBadParam) {
  const TokenInfo def_header(FakeDefineEnum, "`define");
  const TokenInfo macro_name(FakeIdEnum, "FF");
  MacroDefinition macro(def_header, macro_name);
  std::vector<TokenInfo> call_args;
  const TokenInfo actual1(FakeIntEnum, "99");
  call_args.push_back(actual1);
  MacroDefinition::substitution_map_type sub_map;
  const auto status = macro.PopulateSubstitutionMap(call_args, &sub_map);
  EXPECT_FALSE(status.ok());
}

// Tests creating a token substitution map with a 1-parameter macro with default
// value.
TEST(MacroDefinitionTest, PopulateSubstitutionMapOneParamDefault) {
  const TokenInfo def_header(FakeDefineEnum, "`define");
  const TokenInfo macro_name(FakeIdEnum, "FF");
  MacroDefinition macro(def_header, macro_name);
  const TokenInfo param_default(FakeIdEnum, "ticker");
  {
    const TokenInfo param_name(FakeIdEnum, "clk");
    const MacroParameterInfo param{param_name, param_default};
    const bool appended = macro.AppendParameter(param);
    EXPECT_TRUE(appended);
  }
  std::vector<TokenInfo> call_args;
  const TokenInfo actual1(FakeIntEnum, "");  // blank argument
  call_args.push_back(actual1);
  MacroDefinition::substitution_map_type sub_map;
  const auto status = macro.PopulateSubstitutionMap(call_args, &sub_map);
  EXPECT_TRUE(status.ok());
  auto not_param = FindOrNull(sub_map, "dock");
  EXPECT_EQ(not_param, nullptr);
  auto expect_param = FindOrNull(sub_map, "clk");
  EXPECT_EQ(*expect_param, param_default);
}

}  // namespace
}  // namespace verible
