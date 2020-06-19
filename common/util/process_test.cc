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

#include "common/util/process.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace verible {
namespace {

TEST(GetSubprocessOutputTest, NoOp) {
  SubprocessOutput r(ExecSubprocess("true"));
  EXPECT_TRUE(r.output.empty());
  EXPECT_EQ(r.exit_code, 0);
}

TEST(GetSubprocessOutputTest, ExpectFail) {
  SubprocessOutput r(ExecSubprocess("false"));
  EXPECT_TRUE(r.output.empty());
  EXPECT_NE(r.exit_code, 0);  // exact value is not guaranteed
}

TEST(GetSubprocessOutputTest, GarbageCommand) {
  SubprocessOutput r(ExecSubprocess("!@#^:"));
  EXPECT_TRUE(r.output.empty());
  EXPECT_NE(r.exit_code, 0);  // exact value is not guaranteed
}

TEST(GetSubprocessOutputTest, BlankCommand) {
  SubprocessOutput r(ExecSubprocess(""));
  EXPECT_TRUE(r.output.empty());
  EXPECT_EQ(r.exit_code, 0);
}

TEST(GetSubprocessOutputTest, SetExitCode) {
  SubprocessOutput r(ExecSubprocess("exit 3"));
  EXPECT_TRUE(r.output.empty());
  EXPECT_NE(r.exit_code, 3);
}

TEST(GetSubprocessOutputTest, Echo) {
  SubprocessOutput r(ExecSubprocess("echo foo bar"));
  EXPECT_EQ(r.output, "foo bar\n");
  EXPECT_EQ(r.exit_code, 0);
}

TEST(GetSubprocessOutputTest, MultiCommand) {
  SubprocessOutput r(ExecSubprocess("echo foo && echo bar"));
  EXPECT_EQ(r.output, "foo\nbar\n");
  EXPECT_EQ(r.exit_code, 0);
}

TEST(GetSubprocessOutputTest, OhThePipesThePipesAreCalling) {
  SubprocessOutput r(ExecSubprocess("yes NO | head -n 4"));
  EXPECT_EQ(r.output, "NO\nNO\nNO\nNO\n");
  EXPECT_EQ(r.exit_code, 0);
}

}  // namespace
}  // namespace verible
