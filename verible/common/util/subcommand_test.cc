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

#include "verible/common/util/subcommand.h"

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/match.h"
#include "absl/strings/string_view.h"
#include "gtest/gtest.h"

namespace verible {
namespace {

TEST(SubcommandRegistryTest, NoNewCommands) {
  SubcommandRegistry registry;
  const std::string command_list = registry.ListCommands();
  EXPECT_TRUE(absl::StrContains(command_list, "help"));
  // Make sure the internal 'error' command is not shown in help.
  EXPECT_FALSE(absl::StrContains(command_list, "error"));
}

TEST(SubcommandRegistryTest, HelpNoArgsTest) {
  SubcommandRegistry registry;
  // Test built-in help command.
  const SubcommandEntry &help(registry.GetSubcommandEntry("help"));
  std::istringstream ins;
  std::ostringstream outs, errs;
  std::vector<absl::string_view> args;
  const SubcommandArgsRange range(args.begin(), args.end());
  const auto status = help.main(range, ins, outs, errs);
  EXPECT_TRUE(status.ok()) << "Unexpected error:\n" << status.message();
  EXPECT_TRUE(outs.str().empty()) << outs.str();
  EXPECT_TRUE(absl::StrContains(errs.str(), "available commands:"));
}

TEST(SubcommandRegistryTest, HelpHelpCommand) {
  SubcommandRegistry registry;
  // Test built-in help command.
  const SubcommandEntry &help(registry.GetSubcommandEntry("help"));
  std::istringstream ins;
  std::ostringstream outs, errs;
  std::vector<absl::string_view> args{"help"};
  const SubcommandArgsRange range(args.begin(), args.end());

  const auto status = help.main(range, ins, outs, errs);
  EXPECT_TRUE(status.ok()) << "Unexpected error:\n" << status.message();
  EXPECT_TRUE(outs.str().empty()) << outs.str();
  EXPECT_TRUE(absl::StrContains(errs.str(), "Prints command help."));
}

static const SubcommandEntry fizz_func{
    [](const SubcommandArgsRange, std::istream &, std::ostream &outs,
       std::ostream &) {
      outs << 42;
      return absl::OkStatus();
    },
    "fizz does almost nothing",
    true,  // public
};

TEST(SubcommandRegistryTest, RegisterCommandPublicOk) {
  SubcommandRegistry registry;
  {
    const auto status = registry.RegisterCommand("fizz", fizz_func);
    EXPECT_TRUE(status.ok()) << "Unexpected error:\n" << status.message();
    EXPECT_TRUE(absl::StrContains(registry.ListCommands(), "fizz"));
  }

  std::vector<absl::string_view> args;
  const SubcommandArgsRange range(args.begin(), args.end());
  const SubcommandEntry &fizz_entry(registry.GetSubcommandEntry("fizz"));

  std::istringstream ins;
  std::ostringstream outs, errs;
  const auto status = fizz_entry.main(range, ins, outs, errs);
  EXPECT_TRUE(status.ok()) << "Unexpected error:\n" << status.message();
  EXPECT_EQ(outs.str(), "42");
}

TEST(SubcommandRegistryTest, ShowRegisteredCommandsOnWrongCommandRequest) {
  SubcommandRegistry registry;
  {
    const auto status = registry.RegisterCommand("fizz", fizz_func);
    EXPECT_TRUE(absl::StrContains(registry.ListCommands(), "fizz"));
  }

  const SubcommandEntry &cmd(registry.GetSubcommandEntry("wrong_command"));

  std::vector<absl::string_view> args{"foo", "bar"};
  const SubcommandArgsRange range(args.begin(), args.end());
  std::istringstream ins;
  std::ostringstream outs, errs;
  const auto status = cmd.main(range, ins, outs, errs);
  EXPECT_FALSE(status.ok()) << status.message();
  EXPECT_TRUE(absl::StrContains(errs.str(), "available commands"));
  EXPECT_TRUE(absl::StrContains(errs.str(), "fizz"));
}

static const SubcommandEntry buzz_func{
    [](const SubcommandArgsRange, std::istream &, std::ostream &outs,
       std::ostream &) {
      outs << 99;
      return absl::OkStatus();
    },
    "buzz does almost nothing",
    false,  // hidden
};

TEST(SubcommandRegistryTest, RegisterCommandHiddenOk) {
  SubcommandRegistry registry;
  {
    const auto status = registry.RegisterCommand("buzz", buzz_func);
    EXPECT_TRUE(status.ok()) << "Unexpected error:\n" << status.message();
    EXPECT_FALSE(absl::StrContains(registry.ListCommands(), "buzz"));
  }

  std::vector<absl::string_view> args;
  const SubcommandArgsRange range(args.begin(), args.end());
  const SubcommandEntry &buzz_entry(registry.GetSubcommandEntry("buzz"));

  std::istringstream ins;
  std::ostringstream outs, errs;
  const auto status = buzz_entry.main(range, ins, outs, errs);
  EXPECT_TRUE(status.ok()) << "Unexpected error:\n" << status.message();
  EXPECT_EQ(outs.str(), "99");
}

TEST(SubcommandRegistryTest, RegisterCommandErrorAlreadyRegistered) {
  SubcommandRegistry registry;
  // "help" is already a registered command
  const auto status = registry.RegisterCommand("help", fizz_func);
  EXPECT_FALSE(status.ok());
  EXPECT_TRUE(
      absl::StrContains(status.message(), "has already been registered"));
}

TEST(SubcommandRegistryTest, RegisterCommandMultipleCommands) {
  SubcommandRegistry registry;
  {
    {
      const auto status = registry.RegisterCommand("buzz", buzz_func);
      EXPECT_TRUE(status.ok()) << "Unexpected error:\n" << status.message();
    }
    {
      const auto status = registry.RegisterCommand("fizz", fizz_func);
      EXPECT_TRUE(status.ok()) << "Unexpected error:\n" << status.message();
    }
    const std::string command_list = registry.ListCommands();
    EXPECT_TRUE(absl::StrContains(command_list, "fizz"));
    EXPECT_FALSE(absl::StrContains(command_list, "buzz"));
  }

  // Run each subcommand once.
  std::vector<absl::string_view> args;
  const SubcommandArgsRange range(args.begin(), args.end());
  {
    const SubcommandEntry &fizz_entry(registry.GetSubcommandEntry("fizz"));
    std::istringstream ins;
    std::ostringstream outs, errs;
    const auto status = fizz_entry.main(range, ins, outs, errs);
    EXPECT_TRUE(status.ok()) << "Unexpected error:\n" << status.message();
    EXPECT_EQ(outs.str(), "42");
  }
  {
    const SubcommandEntry &buzz_entry(registry.GetSubcommandEntry("buzz"));
    std::istringstream ins;
    std::ostringstream outs, errs;
    const auto status = buzz_entry.main(range, ins, outs, errs);
    EXPECT_TRUE(status.ok()) << "Unexpected error:\n" << status.message();
    EXPECT_EQ(outs.str(), "99");
  }
}

}  // namespace
}  // namespace verible
