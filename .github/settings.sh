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

# WARNING: All values set in this file need to be plain strings as they have to
# pass through things like Docker files which don't support bash arrays and
# similar functionality.

[[ "${BASH_SOURCE[0]}" != "${0}" ]] && SOURCED=1 || SOURCED=0

if [ $SOURCED -ne 1 ]; then
        echo "settings.sh should be sourced, not run."
        exit 1
fi

export BAZEL_VERSION=3.7.0


# TODO(b/171679296): re-enable c++11 support
#   by downgrading kythe build requirements.
export BAZEL_CXXOPTS="-std=c++17"

# Reduce the verbosity of progress output on CI
export BAZEL_OPTS="-c opt --show_progress_rate_limit=10.0"
