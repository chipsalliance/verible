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

#include "verible/common/lsp/json-rpc-dispatcher.h"

#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

#include "gtest/gtest.h"
#include "nlohmann/json.hpp"

using nlohmann::json;

namespace verible {
namespace lsp {
TEST(JsonRpcDispatcherTest, Call_GarbledInputRequest) {
  int write_fun_called = 0;

  // If the input can't even be parsed, it is reported back to the client
  JsonRpcDispatcher dispatcher([&](std::string_view s) {
    const json j = json::parse(s);
    EXPECT_TRUE(j.find("error") != j.end());
    EXPECT_EQ(j["error"]["code"], JsonRpcDispatcher::kParseError) << s;
    ++write_fun_called;
  });

  dispatcher.DispatchMessage("This is not even close to Json");

  EXPECT_EQ(write_fun_called, 1);  // Complain unparseable input.
  EXPECT_EQ(dispatcher.exception_count(), 1);
}

TEST(JsonRpcDispatcherTest, Call_MissingMethodInRequest) {
  // If the request does not contain a method name, it is malformed.
  int write_fun_called = 0;
  int notification_fun_called = 0;

  JsonRpcDispatcher dispatcher([&](std::string_view s) {
    const json j = json::parse(s);
    EXPECT_TRUE(j.find("error") != j.end());
    EXPECT_EQ(j["error"]["code"], JsonRpcDispatcher::kMethodNotFound) << s;
    ++write_fun_called;
  });
  dispatcher.AddNotificationHandler(
      "foo", [&](const json &j) { ++notification_fun_called; });

  dispatcher.DispatchMessage(
      R"({"jsonrpc":"2.0","params":{"hello": "world"}})");

  EXPECT_EQ(notification_fun_called, 0);
  EXPECT_EQ(write_fun_called, 1);  // Complain about missing method.
  EXPECT_EQ(dispatcher.exception_count(), 0);
}

TEST(JsonRpcDispatcherTest, CallNotification) {
  int write_fun_called = 0;
  int notification_fun_called = 0;

  JsonRpcDispatcher dispatcher([&](std::string_view s) {
    std::cerr << s;
    ++write_fun_called;
  });
  const bool registered =
      dispatcher.AddNotificationHandler("foo", [&](const json &j) {
        EXPECT_EQ(j, json::parse(R"({ "hello": "world"})"));
        ++notification_fun_called;
      });
  EXPECT_TRUE(registered);

  // Registration for method with that name only works once.
  EXPECT_FALSE(dispatcher.AddNotificationHandler("foo", [](const json &j) {}));

  dispatcher.DispatchMessage(
      R"({"jsonrpc":"2.0","method":"foo","params":{"hello": "world"}})");

  EXPECT_EQ(notification_fun_called, 1);
  EXPECT_EQ(write_fun_called, 0);  // Notifications don't have responses.
  EXPECT_EQ(dispatcher.exception_count(), 0);
}

TEST(JsonRpcDispatcherTest, CallNotification_WithoutParamsShouldBeBenign) {
  int notification_fun_called = 0;

  JsonRpcDispatcher dispatcher([&](std::string_view s) { std::cerr << s; });
  const bool registered =
      dispatcher.AddNotificationHandler("foo", [&](const json &j) {
        EXPECT_TRUE(j.empty());
        ++notification_fun_called;
      });
  EXPECT_TRUE(registered);

  // A message that does not contain a parameter should work fine.
  dispatcher.DispatchMessage(R"({"jsonrpc":"2.0","method":"foo"})");

  EXPECT_EQ(notification_fun_called, 1);
  EXPECT_EQ(dispatcher.exception_count(), 0);
}

TEST(JsonRpcDispatcherTest, CallNotification_NotReportInternalError) {
  int write_fun_called = 0;
  int notification_fun_called = 0;

  JsonRpcDispatcher dispatcher([&](std::string_view s) { ++write_fun_called; });

  // This method does not complete but throws an exception.
  dispatcher.AddNotificationHandler("foo", [&](const json &j) -> json {
    ++notification_fun_called;
    throw std::runtime_error("Okay, Houston, we've had a problem here");
  });

  dispatcher.DispatchMessage(
      R"({"jsonrpc":"2.0","method":"foo","params":{"hello":"world"}})");

  EXPECT_EQ(notification_fun_called, 1);
  EXPECT_EQ(write_fun_called, 0);  // Notification issues never sent back.
  EXPECT_EQ(dispatcher.exception_count(), 1);
}

TEST(JsonRpcDispatcherTest, CallNotification_MissingMethodImplemented) {
  // A notification whose method is not registered must be silently ignored.
  // No response with error.
  int write_fun_called = 0;

  JsonRpcDispatcher dispatcher([&](std::string_view s) {  //
    ++write_fun_called;
  });

  dispatcher.DispatchMessage(
      R"({"jsonrpc":"2.0","method":"foo","params":{"hello": "world"}})");

  EXPECT_EQ(write_fun_called, 0);
  EXPECT_EQ(dispatcher.exception_count(), 0);
}

TEST(JsonRpcDispatcherTest, CallRpcHandler) {
  int write_fun_called = 0;
  int rpc_fun_called = 0;

  JsonRpcDispatcher dispatcher([&](std::string_view s) {
    const json j = json::parse(s);
    EXPECT_EQ(std::string(j["result"]["some"]), "response");
    EXPECT_TRUE(j.find("error") == j.end());
    ++write_fun_called;
  });
  const bool registered =
      dispatcher.AddRequestHandler("foo", [&](const json &j) -> json {
        EXPECT_EQ(j, json::parse(R"({ "hello":"world"})"));
        ++rpc_fun_called;
        return json::parse(R"({ "some": "response"})");
      });
  EXPECT_TRUE(registered);

  // Registration with already registered name should fail.
  EXPECT_FALSE(dispatcher.AddRequestHandler(
      "foo", [](const json &j) -> json { return nullptr; }));

  dispatcher.DispatchMessage(
      R"({"jsonrpc":"2.0","id":1,"method":"foo","params":{"hello":"world"}})");

  EXPECT_EQ(rpc_fun_called, 1);
  EXPECT_EQ(write_fun_called, 1);
  EXPECT_EQ(dispatcher.exception_count(), 0);
}

TEST(JsonRpcDispatcherTest, CallRpcHandler_WithoutParamsShouldBeBenign) {
  int write_fun_called = 0;
  int rpc_fun_called = 0;

  JsonRpcDispatcher dispatcher([&](std::string_view s) {
    const json j = json::parse(s);
    EXPECT_EQ(std::string(j["result"]["some"]), "response");
    EXPECT_TRUE(j.find("error") == j.end());
    ++write_fun_called;
  });
  const bool registered =
      dispatcher.AddRequestHandler("foo", [&](const json &j) -> json {
        EXPECT_TRUE(j.empty());
        ++rpc_fun_called;
        return json::parse(R"({ "some": "response"})");
      });
  EXPECT_TRUE(registered);

  // Not providing a parameter object shall be interpreted as no parameters
  dispatcher.DispatchMessage(R"({"jsonrpc":"2.0","id":1,"method":"foo"})");

  EXPECT_EQ(rpc_fun_called, 1);
  EXPECT_EQ(write_fun_called, 1);
  EXPECT_EQ(dispatcher.exception_count(), 0);
}

TEST(JsonRpcDispatcherTest, CallRpcHandler_ReportInternalError) {
  int write_fun_called = 0;
  int rpc_fun_called = 0;

  JsonRpcDispatcher dispatcher([&](std::string_view s) {
    const json j = json::parse(s);
    EXPECT_TRUE(j.find("error") != j.end());
    EXPECT_EQ(j["error"]["code"], JsonRpcDispatcher::kInternalError) << s;
    ++write_fun_called;
  });

  // This method does not complete but throws an exception.
  dispatcher.AddRequestHandler("foo", [&](const json &j) -> json {
    ++rpc_fun_called;
    throw std::runtime_error("Okay, Houston, we've had a problem here");
  });

  dispatcher.DispatchMessage(
      R"({"jsonrpc":"2.0","id":1,"method":"foo","params":{"hello":"world"}})");

  EXPECT_EQ(rpc_fun_called, 1);
  EXPECT_EQ(write_fun_called, 1);
  EXPECT_EQ(dispatcher.exception_count(), 1);
}

TEST(JsonRpcDispatcherTest, CallRpcHandler_MissingMethodImplemented) {
  int write_fun_called = 0;

  JsonRpcDispatcher dispatcher([&](std::string_view s) {
    const json j = json::parse(s);
    EXPECT_TRUE(j.find("error") != j.end());
    EXPECT_EQ(j["error"]["code"], JsonRpcDispatcher::kMethodNotFound) << s;
    ++write_fun_called;
  });

  dispatcher.DispatchMessage(
      R"({"jsonrpc":"2.0","id":1,"method":"foo","params":{"hello":"world"}})");

  EXPECT_EQ(write_fun_called, 1);  // Reported error.
  EXPECT_EQ(dispatcher.exception_count(), 0);
}

TEST(JsonRpcDispatcherTest, SendNotificationToClient) {
  int write_fun_called = 0;
  JsonRpcDispatcher dispatcher([&](std::string_view s) {
    const json j = json::parse(s);
    EXPECT_EQ(j["method"], "greeting_method");
    EXPECT_EQ(j["params"], "Hi, y'all");
    ++write_fun_called;
  });

  const json params = "Hi, y'all";
  dispatcher.SendNotification("greeting_method", params);
  EXPECT_EQ(1, write_fun_called);
}
}  // namespace lsp
}  // namespace verible
