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

#ifndef COMMON_LSP_FILE_EVENT_DISPATCHER_H_
#define COMMON_LSP_FILE_EVENT_DISPATCHER_H_

#include <functional>
#include <list>
#include <map>

namespace verible {
namespace lsp {
class FileEventDispatcher {
 public:
  explicit FileEventDispatcher(unsigned idle_ms = 50);
  ~FileEventDispatcher();

  // Handlers for events from this multiplexer.
  // Returns true if we want to continue to be called in the future or false
  // if we wish to be taken out of the multiplexer.
  typedef std::function<bool()> Handler;

  // These can only be set before Loop() is called or from a
  // running handler itself.
  // Returns false if that filedescriptor is already registered.
  bool RunOnReadable(int fd, const Handler &handler);

  // Handler called regularly every idle_ms in case there's nothing to do.
  void RunOnIdle(const Handler &handler);

  // Run the main loop. Blocks while there is still a filedescriptor
  // registered.
  void Loop();

 protected:
  // Run a single cycle resulting in exactly one call of a handler function.
  // This means that one of these happened:
  //   (1) The next file descriptor became ready and its Handler is called
  //   (2) We encountered a timeout and the idle-Handler has been called.
  //   (3) Signal received or select() issue. Returns false in this case.
  //
  // This is broken out to make it simple to test steps in unit tests.
  bool SingleEvent(unsigned timeout_ms);

 private:
  typedef std::map<int, Handler> HandlerMap;

  const unsigned idle_ms_;
  HandlerMap read_handlers_;
  std::list<Handler> idle_handlers_;
};
}  // namespace lsp
}  // namespace verible
#endif  // COMMON_LSP_FILE_EVENT_DISPATCHER
