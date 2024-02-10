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

#ifndef VERIBLE_COMMON_LSP_JSON_RPC_DISPATCHER_H
#define VERIBLE_COMMON_LSP_JSON_RPC_DISPATCHER_H

#include <functional>
#include <map>
#include <string>
#include <unordered_map>
#include <utility>

#include "absl/strings/string_view.h"
#include "nlohmann/json.hpp"

namespace verible {
namespace lsp {
// A Dispatcher that is fed JSON as string, parses them to json objects and
// dispatches the contained method call to pre-registered handlers.
// Results of RPCCallHandlers are wrapped in a json rpc response object
// and written out to the provided write function.
//
// This implements the JSON RPC specification [1].
//
// All receiving (call to DispatchMessage()) and writing of response (WriteFun)
// is abstracted out to make the dispatcher agnostic of the transport layer.
//
// The RPCHandlers take and return json objects, but since nlohmann::json
// provides ways to auto-convert objects to json, it is possible to
// register properly typed handlers. To create the boilerplate for custom
// types and their conversion, simplest is to use a code generator such
// as jcxxgen [2].
//
// With that, you then can register fully typed handlers with seamless
// conversion
// dispatcer.AddRequestHandler("MyMethod",
//                             [](const MyParamType &p) -> MyResponseType {
//                               return doSomething(p);
//                             });
//
// [1]: https://www.jsonrpc.org/specification
// [2]: https://github.com/hzeller/jcxxgen
class JsonRpcDispatcher {
 public:
  // Magic constants defined in https://www.jsonrpc.org/specification
  static constexpr int kParseError = -32700;
  static constexpr int kMethodNotFound = -32601;
  static constexpr int kInternalError = -32603;

  // A notification receives a request, but does not return anything
  using RPCNotification = std::function<void(const nlohmann::json &r)>;

  // A RPC call receives a request and returns a response.
  // If we ever have a meaningful set of error conditions to convey, maybe
  // change this to absl::StatusOr<nlohmann::json> as return value.
  using RPCCallHandler = std::function<nlohmann::json(const nlohmann::json &)>;

  // A function of type WriteFun is called by the dispatcher to send the
  // string-formatted json response. The user of the JsonRpcDispatcher then
  // can wire that to the underlying transport.
  using WriteFun = std::function<void(absl::string_view response)>;

  // Some statistical counters of method calls or exceptions encountered.
  using StatsMap = std::map<std::string, int>;

  // Responses are written using the "out" write function.
  explicit JsonRpcDispatcher(WriteFun out) : write_fun_(std::move(out)) {}
  JsonRpcDispatcher(const JsonRpcDispatcher &) = delete;

  // Add a request handler for RPC calls that receive data and send a response.
  // Returns successful registration, false if that name is already registered.
  bool AddRequestHandler(const std::string &method_name,
                         const RPCCallHandler &fun) {
    return handlers_.insert({method_name, fun}).second;
  }

  // Add a request handler for RPC Notifications, that are receive-only events.
  // Returns successful registration, false if that name is already registered.
  bool AddNotificationHandler(const std::string &method_name,
                              const RPCNotification &fun) {
    return notifications_.insert({method_name, fun}).second;
  }

  // Dispatch incoming message, a string view with json data.
  // Call this with the content of exactly one message.
  // If this is an RPC call, response will call WriteFun.
  void DispatchMessage(absl::string_view data);

  // Send a notification to the client side. Parameters will be wrapped
  // in a JSON-RPC message and pushed out to the WriteFun
  void SendNotification(const std::string &method,
                        const nlohmann::json &notification_params);

  // Get some human-readable statistical counters of methods called
  // and exception messages encountered.
  const StatsMap &GetStatCounters() const { return statistic_counters_; }

  // Number of exceptions that have been dealt with and turned into error
  // messages or ignored depending on the context.
  // The counters returned by GetStatsCounters() will report counts by
  // exception message.
  int exception_count() const { return exception_count_; }

 private:
  bool CallNotification(const nlohmann::json &req, const std::string &method);
  bool CallRequestHandler(const nlohmann::json &req, const std::string &method);
  void SendReply(const nlohmann::json &response);

  static nlohmann::json CreateError(const nlohmann::json &request, int code,
                                    absl::string_view message);
  static nlohmann::json MakeResponse(const nlohmann::json &request,
                                     const nlohmann::json &call_result);

  const WriteFun write_fun_;

  std::unordered_map<std::string, RPCCallHandler> handlers_;
  std::unordered_map<std::string, RPCNotification> notifications_;
  int exception_count_ = 0;
  StatsMap statistic_counters_;
};
}  // namespace lsp
}  // namespace verible
#endif  // VERIBLE_COMMON_LSP_JSON_RPC_DISPATCHER_H
