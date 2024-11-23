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

#include <chrono>  // IWYU pragma: keep  for chrono_literals
#include <future>
#include <vector>

#include "absl/time/clock.h"
#include "absl/time/time.h"

using namespace std::chrono_literals;

#include "gtest/gtest.h"

namespace verible {
namespace {

// Use up some time to make it more likely to tickle the actual thread
// exeuction.
static void PretendWork(int ms) { absl::SleepFor(absl::Milliseconds(ms)); }

TEST(ThreadPoolTest, SynchronousExecutionIfThreadCountZero) {
  ThreadPool pool(0);
  std::future<int> foo_ture = pool.ExecAsync<int>([]() -> int {
    PretendWork(200);
    return 42;
  });

  EXPECT_EQ(std::future_status::ready, foo_ture.wait_for(1ms))  // NOLINT
      << "Must be available immediately after return";
  EXPECT_EQ(foo_ture.get(), 42);
}

TEST(ThreadPoolTest, WorkIsCompleted) {
  constexpr int kLoops = 100;
  ThreadPool pool(3);

  std::vector<std::future<int>> results;
  results.reserve(kLoops);
  for (int i = 0; i < kLoops; ++i) {
    results.emplace_back(pool.ExecAsync<int>([i]() -> int {
      PretendWork(10);
      return i;
    }));
  }

  // Can't easily make a blackbox test that verifies that the functions are
  // even executed in different threads, but at least let's verify that all
  // of them finish with the expected result.
  for (int i = 0; i < kLoops; ++i) {
    EXPECT_EQ(results[i].get(), i);
  }
}

TEST(ThreadPoolTest, ExceptionsArePropagated) {
  constexpr int kLoops = 10;
  ThreadPool pool(3);

  std::vector<std::future<int>> results;
  results.reserve(10);
  for (int i = 0; i < 10; ++i) {
    results.emplace_back(pool.ExecAsync<int>([]() -> int {
      throw 1;
      return 0;
    }));
  }

  int exception_count = 0;
  for (int i = 0; i < kLoops; ++i) {
    try {
      results[i].get();
    } catch (...) {
      ++exception_count;
    }
  }
  EXPECT_EQ(exception_count, kLoops);
}

}  // namespace
}  // namespace verible
