#!/bin/bash
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

COMPILER=clang

source ./.github/settings.sh

# Make sure we don't have cc_library rules that use exceptions but do not
# declare copts = ["-fexceptions"] in the rule. We want to make it as simple
# as possible to compile without exceptions.
BAZEL_OPTS="${BAZEL_OPTS} --cxxopt=-fno-exceptions"

# Turn warnings to 11. And fail compliation if we encounter one.
BAZEL_OPTS="${BAZEL_OPTS} --cxxopt=-Werror"  # Always want bail on warning
BAZEL_OPTS="${BAZEL_OPTS} --cxxopt=-Wall --cxxopt=-Wextra"

# Warning options are different between compilers. Don't complain about the
# others' compiler warnings.
BAZEL_OPTS="${BAZEL_OPTS} --cxxopt=-Wno-unknown-warning-option"

# -- now disable some of the warnings that happen, so that the compile finishes.

# Status-quo of warnings happening in our code-base. These are benign.
BAZEL_OPTS="${BAZEL_OPTS} --cxxopt=-Wno-unused-parameter"
BAZEL_OPTS="${BAZEL_OPTS} --cxxopt=-Wno-missing-field-initializers"

# Warnings in our code-base, that we might consider removing.
BAZEL_OPTS="${BAZEL_OPTS} --cxxopt=-Wno-redundant-move"

# This is generated (only on CI?) when compiling storage.pb.cc;
# some memset is out of range ?
BAZEL_OPTS="${BAZEL_OPTS} --cxxopt=-Wno-array-bounds"

# Warnings that come from other external parts that we compile.
# Ideally, we would separate them out to ignore only there, while we keep
# tight warnings on for 'our' code-base.
# TODO(hzeller): Remove after https://github.com/google/verible/issues/747
#                is figure out
BAZEL_OPTS="${BAZEL_OPTS} --cxxopt=-Wno-implicit-fallthrough"     # gflags
BAZEL_OPTS="${BAZEL_OPTS} --cxxopt=-Wno-cast-function-type"       # gflags
BAZEL_OPTS="${BAZEL_OPTS} --cxxopt=-Wno-deprecated-declarations"  # jsconcpp

# Layering check (only works with clang) provides a simple check that
# cc targets depend on everything the header inclusion implies.
# Currently disabled, it seems to get hang up on gtest/ gmock/ includes.
# BAZEL_OPTS="${BAZEL_OPTS} --features=layering_check"

export CC="${COMPILER}"
export CXX="${COMPILER}++"
if [ "$COMPILER" == "gcc" ] ; then
  export CXX="g++"
fi

case "$MODE" in
test)
    bazel test --keep_going --test_output=errors $BAZEL_OPTS //...
    ;;

compile|clean)
    bazel build --keep_going $BAZEL_OPTS //...
    ;;

*)
    echo "$0: Unknown value in MODE environment variable: $MODE"
    exit 1
    ;;
esac
