// Copyright 2021 The Verible Authors.
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

#include <exception>

#include "absl/strings/match.h"
#include "gtest/gtest.h"
#include "nlohmann/json.hpp"
#include "verible/common/lsp/jcxxgen-testfile.h"

namespace verible {

TEST(jcxxgen_test, DefaultValueTest) {
  const verible::test::BaseObject obj;

  // No defaults set: naturally constructed
  EXPECT_TRUE(obj.string_value.empty());
  EXPECT_TRUE(obj.string_value_optional.empty());
  EXPECT_EQ(obj.integer_value, 0);
  EXPECT_TRUE(obj.generic_object.is_null());

  // Defaults provided.
  EXPECT_EQ(obj.string_value_with_default, "Hello World");
  EXPECT_EQ(obj.string_value_optional_with_default, "Optional World");
  EXPECT_EQ(obj.integer_value_with_default, 42);
  EXPECT_EQ(obj.struct_value.a, 123);
  EXPECT_EQ(obj.struct_value.b, "foo");

  // Nothing deserialized, so all the has_* return false
  EXPECT_FALSE(obj.has_string_value_optional);
  EXPECT_FALSE(obj.has_string_value_optional_with_default);
  EXPECT_FALSE(obj.has_integer_value_optional);
}

TEST(jcxxgen_test, DeserializeFromJson) {
  const nlohmann::json json_value{
      // Values in the BaseObject
      {"string_value", "abc"},
      {"string_value_optional",
       nullptr},  // null-object is valid optional value
      {"string_value_with_default", "ghi"},

      {"integer_value", 987},
      {"integer_value_optional", nullptr},
      {"integer_value_with_default", 654},

      {"bool_value", true},
      {"struct_value",
       {
           {"a", 321},
           {"b", "bar"},
       }},

      // Values in the derived object
      {"additional_integer_value", 999},
  };
  const verible::test::DerivedObject obj = json_value;
  EXPECT_EQ(obj.string_value, "abc");
  EXPECT_FALSE(obj.has_string_value_optional);
  EXPECT_EQ(obj.string_value_with_default, "ghi");

  EXPECT_EQ(obj.integer_value, 987);
  EXPECT_FALSE(obj.has_integer_value_optional);
  EXPECT_EQ(obj.integer_value_with_default, 654);

  EXPECT_EQ(obj.bool_value, true);

  EXPECT_EQ(obj.struct_value.a, 321);
  EXPECT_EQ(obj.struct_value.b, "bar");

  EXPECT_EQ(obj.additional_integer_value, 999);
}

TEST(jcxxgen_test, DeserializeFromJsonMissingRequiredFieldsThrowException) {
  const nlohmann::json json_value{
      {"string_value", "abc"},
      // Missing optional "string_value_optional" will not be complained about.
      {"string_value_with_default", "def"},

      // integer_value is first value missing
  };

  try {
    const verible::test::BaseObject obj = json_value;
  } catch (const std::exception &e) {
    EXPECT_TRUE(absl::StrContains(e.what(), "integer_value")) << e.what();
  }
}

TEST(jcxxgen_test, SerializeToJson) {
  const verible::test::BaseObject obj{
      .string_value = "a",
      .integer_value = 99,
      .bool_value = false,
      .struct_value{
          .a = 88,
          .b = "baz",
      },
  };
  const nlohmann::json serialized = obj;

  // Roundtrip back with deserialization.
  verible::test::BaseObject obj_copy = serialized;
  EXPECT_EQ(obj.string_value, obj_copy.string_value);
  EXPECT_EQ(obj.integer_value, obj_copy.integer_value);
  EXPECT_EQ(obj.struct_value.a, obj_copy.struct_value.a);
}
}  // namespace verible
