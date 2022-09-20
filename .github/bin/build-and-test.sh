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

bazel --version

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

# Turn warnings to 11. And fail compliation if we encounter one.
BAZEL_OPTS="${BAZEL_OPTS} --cxxopt=-Werror"  # Always want bail on warning
BAZEL_OPTS="${BAZEL_OPTS} --cxxopt=-W --cxxopt=-Wall --cxxopt=-Wextra"

# The following warning only reports with clang++; it is ignored by gcc
BAZEL_OPTS="${BAZEL_OPTS} --cxxopt=-Wunreachable-code"

# -- now disable some of the warnings that happen, so that the compile finishes.

# Status-quo of warnings happening in our code-base. These are benign.
BAZEL_OPTS="${BAZEL_OPTS} --cxxopt=-Wno-unused-parameter"
BAZEL_OPTS="${BAZEL_OPTS} --cxxopt=-Wno-missing-field-initializers"

# Warnings in our code-base, that we might consider removing.
BAZEL_OPTS="${BAZEL_OPTS} --cxxopt=-Wno-redundant-move"

# If compiled with c++20 compatible compilers, it complains about extending
# std::iterator. Can be removed once
# https://github.com/chipsalliance/verible/issues/1400 is fixed.
BAZEL_OPTS="${BAZEL_OPTS} --cxxopt=-Wno-deprecated-declarations"

# Warnings that come from other external parts that we compile.
# Ideally, we would separate them out to ignore only there, while we keep
# tight warnings on for 'our' code-base.
# TODO(hzeller): Remove after
#            https://github.com/chipsalliance/verible/issues/747 is figured out
if [[ "${CXX}" == clang* ]]; then
  # -- only recognized by clang
  # Don't rely on implicit template type deduction
  BAZEL_OPTS="${BAZEL_OPTS} --cxxopt=-Wctad-maybe-unsupported"
  BAZEL_OPTS="${BAZEL_OPTS} --cxxopt=-Wstring-conversion"
  BAZEL_OPTS="${BAZEL_OPTS} --cxxopt=-Wno-unused-function"  # Protobuffer issue
fi

# If parameter given and the MODE allows choosing, we build the target
# as provided, otherwise all. This allows manual invocation of interesting
# targets.
CHOSEN_TARGET=${1:-//...}

case "$MODE" in
  test|test-clang)
    bazel test --keep_going --cache_test_results=no --test_output=errors $BAZEL_OPTS "$CHOSEN_TARGET"
    ;;

  asan|asan-clang)
    bazel test --config=asan --cache_test_results=no --test_output=errors $BAZEL_OPTS -c fastbuild "$CHOSEN_TARGET"
    ;;

  coverage)
    bazel coverage \
          --combined_report=lcov \
          --coverage_report_generator=@bazel_tools//tools/test/CoverageOutputGenerator/java/com/google/devtools/coverageoutputgenerator:Main \
          "$CHOSEN_TARGET"
    # output will be in bazel-out/_coverage/_coverage_report.dat
    ;;

  compile|compile-clang|clean)
    bazel build --keep_going $BAZEL_OPTS :install-binaries
    ;;

  smoke-test)
    $(dirname $0)/smoke-test.sh
    ;;

  *)
    echo "$0: Unknown value in MODE environment variable: $MODE"
    exit 1
    ;;
esac

# Shutdown to make sure all files in the cache are flushed.
bazel shutdown
