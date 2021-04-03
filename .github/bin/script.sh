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

source ./.github/settings.sh

# Make sure we don't have cc_library rules that use exceptions but do not
# declare copts = ["-fexceptions"] in the rule. We want to make it as simple
# as possible to compile without exceptions.
# ... and unfortunately we have to disable this for now as the external
# kythe dependency does use exceptions.
#BAZEL_OPTS="${BAZEL_OPTS} --cxxopt=-fno-exceptions"

# Turn warnings to 11. And fail compliation if we encounter one.
BAZEL_OPTS="${BAZEL_OPTS} --cxxopt=-Werror"  # Always want bail on warning
BAZEL_OPTS="${BAZEL_OPTS} --cxxopt=-Wall --cxxopt=-Wextra"

# -- now disable some of the warnings that happen, so that the compile finishes.

# Status-quo of warnings happening in our code-base. These are benign.
BAZEL_OPTS="${BAZEL_OPTS} --cxxopt=-Wno-unused-parameter"
BAZEL_OPTS="${BAZEL_OPTS} --cxxopt=-Wno-missing-field-initializers"

# Warnings in our code-base, that we might consider removing.
BAZEL_OPTS="${BAZEL_OPTS} --cxxopt=-Wno-redundant-move"

# Warnings that come from other external parts that we compile.
# Ideally, we would separate them out to ignore only there, while we keep
# tight warnings on for 'our' code-base.
# TODO(hzeller): figure out if bazel allows for that; WORKSPACE file ?
BAZEL_OPTS="${BAZEL_OPTS} --cxxopt=-Wno-implicit-fallthrough"     # gflags
BAZEL_OPTS="${BAZEL_OPTS} --cxxopt=-Wno-cast-function-type"       # gflags
BAZEL_OPTS="${BAZEL_OPTS} --cxxopt=-Wno-sign-compare"             # glog
BAZEL_OPTS="${BAZEL_OPTS} --cxxopt=-Wno-stringop-truncation"      # memcache
BAZEL_OPTS="${BAZEL_OPTS} --cxxopt=-Wno-int-in-bool-context"      # memcache
BAZEL_OPTS="${BAZEL_OPTS} --cxxopt=-Wno-deprecated-declarations"  # jsconcpp
BAZEL_OPTS="${BAZEL_OPTS} --cxxopt=-Wno-unused-but-set-variable"  # kythe
BAZEL_OPTS="${BAZEL_OPTS} --cxxopt=-Wno-array-bounds"             # kythe

case "$MODE" in
test)
    bazel test $BAZEL_OPTS //...
    ;;

compile|clean)
    bazel build $BAZEL_OPTS //...
    ;;

*)
    echo "script.sh: Unknown mode $MODE"
    exit 1
    ;;
esac
