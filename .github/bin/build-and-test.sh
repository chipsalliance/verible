#!/usr/bin/env bash
# Copyright 2020 The Verible Authors.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set -x
set -e

source ./.github/settings.sh

if [[ "${MODE}" == *-clang ]]; then
  # Baseline in case we don't find a specific version below
  export CXX=clang++
  export CC=clang

  # clang versions supported. Starting with 13, we
  # get some warnings in absl, so let's not go beyond
  # 12 for now.
  for version in 12 11 10 ; do
    if command -v clang++-${version}; then
      export CXX=clang++-${version}
      export CC=clang-${version}
      break
    fi
  done
fi

# Make sure we don't have cc_library rules that use exceptions but do not
# declare copts = ["-fexceptions"] in the rule. We want to make it as simple
# as possible to compile without exceptions.
BAZEL_OPTS="${BAZEL_OPTS} --cxxopt=-fno-exceptions"

# Avoid expensive language feature. Maybe should consider to put into .bazelrc
BAZEL_OPTS="${BAZEL_OPTS} --cxxopt=-fno-rtti"

# Turn warnings to 11. And fail compliation if we encounter one.
BAZEL_OPTS="${BAZEL_OPTS} --cxxopt=-Werror"  # Always want bail on warning

# The following warning only reports with clang++; it is ignored by gcc
BAZEL_OPTS="${BAZEL_OPTS} --cxxopt=-Wunreachable-code"

# If parameter given and the MODE allows choosing, we build the target
# as provided, otherwise all. This allows manual invocation of interesting
# targets.
CHOSEN_TARGETS=${@:-//...}

case "$MODE" in
  test|test-clang)
    bazel test --keep_going --cache_test_results=no --test_output=errors ${BAZEL_OPTS} ${BAZEL_TEST_OPTS} ${CHOSEN_TARGETS}
    ;;

  asan|asan-clang)
    if [[ "${MODE}" == "asan" ]]; then
      # Some gcc 12 issue with regexp it seems.
      BAZEL_OPTS="${BAZEL_OPTS} --cxxopt=-Wno-maybe-uninitialized"
    fi
    bazel test --config=asan --cache_test_results=no --test_output=errors ${BAZEL_OPTS} ${BAZEL_TEST_OPTS} -c fastbuild ${CHOSEN_TARGETS}
    ;;

  coverage)
    bazel coverage \
          --combined_report=lcov \
          --coverage_report_generator=@bazel_tools//tools/test/CoverageOutputGenerator/java/com/google/devtools/coverageoutputgenerator:Main \
          ${CHOSEN_TARGETS}
    # output will be in bazel-out/_coverage/_coverage_report.dat
    ;;

  compile|compile-clang|clean)
    bazel build -c opt --keep_going ${BAZEL_OPTS} :install-binaries
    ;;

  compile-static|compile-static-clang)
    bazel build -c opt --keep_going --config=create_static_linked_executables ${BAZEL_OPTS} :install-binaries
    ;;

  test-c++20|test-c++20-clang)
    # Compile with C++ 20 to make sure to be compatible with the next version.
    if [[ ${MODE} == "test-c++20" ]]; then
      # Assignment of 1-char strings: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=105329
      BAZEL_OPTS="${BAZEL_OPTS} --cxxopt=-Wno-restrict --cxxopt=-Wno-missing-requires"
    fi
    bazel test --keep_going --test_output=errors ${BAZEL_OPTS} --cxxopt=-std=c++20 -- ${CHOSEN_TARGETS}
    ;;

  test-c++23|test-c++23-clang)
    # Same; c++23
    if [[ ${MODE} == "test-c++23" ]]; then
      # Assignment of 1-char strings: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=105329
      BAZEL_OPTS="${BAZEL_OPTS} --cxxopt=-Wno-restrict --cxxopt=-Wno-missing-requires"
    fi
    bazel test --keep_going --test_output=errors ${BAZEL_OPTS} --cxxopt=-std=c++2b -- ${CHOSEN_TARGETS}
    ;;

  smoke-test)
    $(dirname $0)/smoke-test.sh
    ;;

  smoke-test-analyzer)
    SMOKE_LOGGING_DIR=/tmp/error-logs/ $(dirname $0)/smoke-test.sh
    python3 $(dirname $0)/error-log-analyzer.py /tmp/error-logs/ --verible-path $(dirname $0)/../../
    cat sta.md >> $GITHUB_STEP_SUMMARY
    ;;

  *)
    echo "$0: Unknown value in MODE environment variable: $MODE"
    exit 1
    ;;
esac

# Shutdown to make sure all files in the cache are flushed.
bazel shutdown
