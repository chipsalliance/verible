// Copyright 2017-2023 The Verible Authors.
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

#ifndef VERIBLE_COMMON_UTIL_THREAD_POOL_H
#define VERIBLE_COMMON_UTIL_THREAD_POOL_H

#include <condition_variable>
#include <deque>
#include <exception>
#include <functional>
#include <future>
#include <mutex>
#include <thread>
#include <vector>

namespace verible {
// Simple thread-pool.
// Passing in functions, returning futures.
//
// Why not use std::async() ? That standard is so generic and vaguely
// specified that in practice there is no implementation of a policy that
// provides a thread-pool behavior with a guaranteed upper bound of cores used
// on all platforms.
class ThreadPool {
 public:
  // Create thread pool with "thread_count" threads.
  // If that count is zero, functions will be executed synchronously.
  explicit ThreadPool(int thread_count);

  // Exit ASAP and leave remaining work in queue unfinished.
  ~ThreadPool();

  // Add a function returning T, that is to be executed asynchronously.
  // Return a std::future<T> with the eventual result.
  //
  // As a special case: if initialized with no threads, the function is
  // executed synchronously.
  template <class T>
  [[nodiscard]] std::future<T> ExecAsync(const std::function<T()> &f) {
    auto *p = new std::promise<T>();
    std::future<T> future_result = p->get_future();
    // NOLINT, as clang-tidy assumes memory leak where is none.
    auto promise_fulfiller = [p, f]() {  // NOLINT
      try {
        p->set_value(f());
      } catch (...) {
        p->set_exception(std::current_exception());
      }
      delete p;
    };
    EnqueueWork(promise_fulfiller);
    return future_result;
  }

 private:
  void Runner();
  void CancelAllWork();
  void EnqueueWork(const std::function<void()> &work);

  std::vector<std::thread *> threads_;
  std::mutex lock_;
  std::condition_variable cv_;
  std::deque<std::function<void()>> work_queue_;
  bool exiting_ = false;
};

}  // namespace verible
#endif  // VERIBLE_COMMON_UTIL_THREAD_POOL_H
