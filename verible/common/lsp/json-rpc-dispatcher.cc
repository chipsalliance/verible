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

#include <exception>
#include <sstream>
#include <string>
#include <string_view>

#include "nlohmann/json.hpp"
#include "verible/common/util/logging.h"

namespace verible {
namespace lsp {
void JsonRpcDispatcher::DispatchMessage(std::string_view data) {
  nlohmann::json request;
  try {
    request = nlohmann::json::parse(data);
  } catch (const std::exception &e) {
    ++statistic_counters_[e.what()];
    ++exception_count_;
    SendReply(CreateError(request, kParseError, e.what()));
    return;
  }

  if (request.find("method") == request.end()) {
    SendReply(
        CreateError(request, kMethodNotFound, "Method required in request"));
    ++statistic_counters_["Request without method"];
    return;
  }
  const std::string &method = request["method"];

  // Direct dispatch, later maybe send to an executor that returns futures ?
  const bool is_notification = (request.find("id") == request.end());
  VLOG(1) << "Got " << (is_notification ? "notification" : "method call")
          << " '" << method << "'; req-size: " << data.size();
  bool handled = false;
  if (is_notification) {
    handled = CallNotification(request, method);
  } else {
    handled = CallRequestHandler(request, method);
  }
  ++statistic_counters_[method + (handled ? "" : " (unhandled)") +
                        (is_notification ? "  ev" : " RPC")];
}

// Methods/Notifications without parameters can also send nothing for "params".
// Make sure we handle that gracefully. (e.g. "shutdown" method call).
static const nlohmann::json &ExtractParams(const nlohmann::json &request) {
  static const nlohmann::json empty_params = nlohmann::json::object();
  auto found = request.find("params");
  return found != request.end() ? *found : empty_params;
}

bool JsonRpcDispatcher::CallNotification(const nlohmann::json &req,
                                         const std::string &method) {
  const auto &found = notifications_.find(method);
  if (found == notifications_.end()) {
    LOG(INFO) << "Ignoring notification '" << method << "'";
    return false;
  }
  const auto &fun_to_call = found->second;
  try {
    fun_to_call(ExtractParams(req));
    return true;
  } catch (const std::exception &e) {
    ++exception_count_;
    ++statistic_counters_[method + " : " + e.what()];
    LOG(ERROR) << "Notification error for '" << method << "' :" << e.what();
  }
  return false;
}

bool JsonRpcDispatcher::CallRequestHandler(const nlohmann::json &req,
                                           const std::string &method) {
  const auto &found = handlers_.find(method);
  if (found == handlers_.end()) {
    SendReply(CreateError(req, kMethodNotFound,
                          "method '" + method + "' not found."));
    LOG(ERROR) << "Unhandled method '" << method << "'";
    return false;
  }
  const auto &fun_to_call = found->second;
  try {
    SendReply(MakeResponse(req, fun_to_call(ExtractParams(req))));
    return true;
  } catch (const std::exception &e) {
    ++exception_count_;
    ++statistic_counters_[method + " : " + e.what()];
    SendReply(CreateError(req, kInternalError, e.what()));
    LOG(ERROR) << "Method error for '" << method << "' :" << e.what();
  }
  return false;
}

void JsonRpcDispatcher::SendNotification(const std::string &method,
                                         const nlohmann::json &notification) {
  nlohmann::json result = {{"jsonrpc", "2.0"}};
  result["method"] = method;
  result["params"] = notification;
  SendReply(result);
}

/*static*/ nlohmann::json JsonRpcDispatcher::CreateError(
    const nlohmann::json &request, int code, std::string_view message) {
  nlohmann::json result = {
      {"jsonrpc", "2.0"},
  };
  result["error"] = {{"code", code}};
  if (!message.empty()) {
    result["error"]["message"] = message;
  }

  if (request.find("id") != request.end()) {
    result["id"] = request["id"];
  }

  return result;
}

/*static*/ nlohmann::json JsonRpcDispatcher::MakeResponse(
    const nlohmann::json &request, const nlohmann::json &call_result) {
  nlohmann::json result = {
      {"jsonrpc", "2.0"},
  };
  result["id"] = request["id"];
  result["result"] = call_result;
  return result;
}

void JsonRpcDispatcher::SendReply(const nlohmann::json &response) {
  std::stringstream out_bytes;
  out_bytes << response << "\n";
  write_fun_(out_bytes.str());
}
}  // namespace lsp
}  // namespace verible
