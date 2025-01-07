#!/usr/bin/env bash
# Copyright 2024 The Verible Authors.
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

set -u
set -e

# Which bazel and bant to use can be chosen by environment variables
BAZEL=${BAZEL:-bazel}
BANT=${BANT:-needs-to-be-compiled-locally}

if [ "${BANT}" = "needs-to-be-compiled-locally" ]; then
  # Bant not given, compile from bzlmod dep.
  ${BAZEL} build -c opt --cxxopt=-std=c++20 @bant//bant:bant >/dev/null 2>&1
  BANT=$(realpath bazel-bin/external/bant*/bant/bant)
fi

DWYU_OUT="${TMPDIR:-/tmp}/dwyu.out"

if "${BANT}" -q dwyu ... ; then
  echo "Dependencies ok"
else
  echo
  echo "^ Please run buildozer commands to fix the dependencies and amend PR"
  exit 1
fi
