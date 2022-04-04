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

# WARNING: All values set in this file need to be plain strings as they have to
# pass through things like Docker files which don't support bash arrays and
# similar functionality.

[[ "${BASH_SOURCE[0]}" != "${0}" ]] && SOURCED=1 || SOURCED=0

if [ $SOURCED -ne 1 ]; then
        echo "settings.sh should be sourced, not run."
        exit 1
fi

export GIT_VERSION=${GIT_VERSION:-$(git describe --match=v*)}

export BAZEL_CXXOPTS="-std=c++17"

# Reduce the verbosity of progress output on CI
export BAZEL_OPTS="-c opt --show_progress_rate_limit=10.0"

export BAZEL_VERSION=4.0.0
# Without following environment variable set, Bazel updates itself to the
# latest version
export USE_BAZEL_VERSION=$BAZEL_VERSION

# TODO(b/171989992): revert to using release version after upgrading system
#   libraries/image (need: GLIBCXX_3.4.26, CXXABI_1.3.11, GLIBC_2.29).
# KYTHE_VERSION=master
export KYTHE_VERSION=v0.0.52
