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
export TAG=${TAG:-$(git rev-parse --short "$GITHUB_SHA")}

# TODO(b/171679296): re-enable c++11 support
#   by downgrading kythe build requirements.
BAZEL_CXXOPTS=(--cxxopt=-std=c++17)

# Reduce log noise.
BAZEL_OPTS=(--show_progress_rate_limit=10.0)

case "$MODE" in
test)
    bazel test -c opt "${BAZEL_OPTS[@]}" "${BAZEL_CXXOPTS[@]}" //...
    ;;

compile)
    bazel build -c opt "${BAZEL_OPTS[@]}" "${BAZEL_CXXOPTS[@]}" //...
    ;;

bin)
    cd releasing
    ./docker-generate.sh ${OS}-${OS_VERSION}
    ./docker-run.sh ${OS}-${OS_VERSION}
    mkdir -p /tmp/releases
    cp out/*.tar.gz /tmp/releases/
    ;;

*)
    echo "script.sh: Unknown mode $MODE"
    exit 1
    ;;
esac
