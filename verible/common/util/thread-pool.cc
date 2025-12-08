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

#include "verible/common/util/thread-pool.h"

#include <functional>
#include <mutex>
#include <thread>

namespace verible {
ThreadPool::ThreadPool(int thread_count) {
  for (int i = 0; i < thread_count; ++i) {
    threads_.push_back(new std::thread(&ThreadPool::Runner, this));
  }
}

ThreadPool::~ThreadPool() {
  CancelAllWork();
  for (auto *t : threads_) {
    t->join();
    delete t;
  }
}

void ThreadPool::Runner() {
  std::function<void()> process_work_item;
  for (;;) {
    {
      std::unique_lock<std::mutex> l(lock_);
      cv_.wait(l, [this]() { return !work_queue_.empty() || exiting_; });
      if (exiting_) return;
      process_work_item = work_queue_.front();
      work_queue_.pop_front();
    }
    process_work_item();
  }
}

void ThreadPool::EnqueueWork(const std::function<void()> &work) {
  if (threads_.empty()) {
    work();  // synchronous execution
    return;
  }

  {
    std::unique_lock<std::mutex> l(lock_);
    work_queue_.emplace_back(work);
  }
  cv_.notify_one();
}

void ThreadPool::CancelAllWork() {
  {
    std::unique_lock<std::mutex> l(lock_);
    exiting_ = true;
  }
  cv_.notify_all();
}
}  // namespace verible
