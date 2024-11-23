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

#include "verible/common/util/value-saver.h"

#include "gtest/gtest.h"

namespace verible {
namespace {

TEST(ValueSaverTest, NoChange) {
  int x = 1;
  const ValueSaver<int> temp(&x);
  EXPECT_EQ(x, 1);
}

TEST(ValueSaverTest, ChangeAndRestore) {
  int x = 1;
  {
    const ValueSaver<int> temp(&x);
    x = 0;
  }
  EXPECT_EQ(x, 1);
}

TEST(ValueSaverTest, ChangeInConstructor) {
  int x = 1;
  {
    const ValueSaver<int> temp(&x, 2);
    EXPECT_EQ(x, 2);
  }
  EXPECT_EQ(x, 1);
}

TEST(ValueSaverTest, NestedScopes) {
  int x = 1;
  {
    const ValueSaver<int> temp(&x, 2);
    EXPECT_EQ(x, 2);
    {
      const ValueSaver<int> temp(&x, 5);
      EXPECT_EQ(x, 5);
    }
    EXPECT_EQ(x, 2);
  }
  EXPECT_EQ(x, 1);
}

}  // namespace
}  // namespace verible
