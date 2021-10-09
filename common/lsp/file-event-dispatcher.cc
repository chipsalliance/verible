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

#include "common/lsp/file-event-dispatcher.h"

#ifndef _WIN32
#include <sys/select.h>
#else
// TODO: this implementation doesn't seem to work on windows with regular
// file descriptors as the posix subsystem is very spotty
// implmented there. Winsock only seems to deal with sockets, not with
// any file descriptor (error returned by select() is WSAENOTSOCK).
//
// If someone with access to a windows machine and knowledge about how
// these things can work on that platform, please provide a PR.
//
// We might also need to sidestep that by using a library such as libevent
// that already has worked around all these issues and implement
// FileEventDispatcher with that. But it would be another dependency.
#include <winsock2.h>

#include <iostream>
#endif

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>

namespace verible {
namespace lsp {
FileEventDispatcher::FileEventDispatcher(unsigned idle_ms) : idle_ms_(idle_ms) {
#ifdef _WIN32
  // Windows-specific init to be able to use select()
  const WORD requestedVersion = MAKEWORD(2, 0);
  WSADATA initdata;
  WSAStartup(requestedVersion, &initdata);
#endif
}

FileEventDispatcher::~FileEventDispatcher() {
#ifdef _WIN32
  WSACleanup();
#endif
}

bool FileEventDispatcher::RunOnReadable(int fd, const Handler &handler) {
  return read_handlers_.insert({fd, handler}).second;
}

void FileEventDispatcher::RunOnIdle(const Handler &handler) {
  idle_handlers_.push_back(handler);
}

static void CallHandlers(
    fd_set *to_call_fd_set, int available_fds,
    std::map<int, FileEventDispatcher::Handler> *handlers) {
  for (auto it = handlers->begin(); available_fds && it != handlers->end();) {
    bool keep_handler = true;
    if (FD_ISSET(it->first, to_call_fd_set)) {
      --available_fds;
      keep_handler = it->second();
    }
    // NB: modifying container while iterating. Safe for std::map.
    it = keep_handler ? std::next(it) : handlers->erase(it);
  }
}

bool FileEventDispatcher::SingleEvent(unsigned int timeout_ms) {
  fd_set read_fds;

  struct timeval timeout;
  timeout.tv_sec = timeout_ms / 1000;
  timeout.tv_usec = (timeout_ms % 1000) * 1000;

  int maxfd = -1;
  FD_ZERO(&read_fds);

  for (const auto &it : read_handlers_) {
    maxfd = std::max(maxfd, it.first);
    FD_SET(it.first, &read_fds);
  }

  if (maxfd < 0) {
    // file descriptors only can be registred from within handlers
    // or before running the Loop(). So if no descriptors are left,
    // there is no chance for any to re-appear, so we can exit.
    return false;
  }

  int fds_ready = select(maxfd + 1, &read_fds, nullptr, nullptr, &timeout);
  if (fds_ready < 0) {
#ifdef _WIN32
    std::cerr << "s=" << fds_ready << "; e=" << WSAGetLastError() << std::endl;
#else
    perror("select() failed");
#endif
    return false;
  }

  if (fds_ready == 0) {  // No FDs ready: timeout situation.
    for (auto it = idle_handlers_.begin(); it != idle_handlers_.end(); /**/) {
      const bool keep_handler = (*it)();
      it = keep_handler ? std::next(it) : idle_handlers_.erase(it);
    }
    return true;
  }

  CallHandlers(&read_fds, fds_ready, &read_handlers_);

  return true;
}

void FileEventDispatcher::Loop() {
  const unsigned timeout = idle_ms_;

  while (SingleEvent(timeout)) {
    // Intentional empty body.
  }
}
}  // namespace lsp
}  // namespace verible
