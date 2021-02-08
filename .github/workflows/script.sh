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

case "$MODE" in
test)
    bazel test $BAZEL_OPTS //...
    ;;

compile|clean)
    bazel build $BAZEL_OPTS //...
    ;;

bin)
    cd releasing
    ./docker-run.sh ${OS}-${OS_VERSION}
    mkdir -p /tmp/releases
    cp out/*.tar.gz /tmp/releases/
    ;;

*)
    echo "script.sh: Unknown mode $MODE"
    exit 1
    ;;
esac
