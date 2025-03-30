#!/usr/bin/env bash
# Copyright 2025 The Verible Authors.
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

# Print path to a bant binary. Can be provided by an environment variable
# or built from our dependency.

BAZEL=${BAZEL:-bazel}
BANT=${BANT:-needs-to-be-compiled-locally}

# Bant not given, compile from bzlmod dep.
if [ "${BANT}" = "needs-to-be-compiled-locally" ]; then
  "${BAZEL}" build -c opt --cxxopt=-std=c++20 @bant//bant:bant 2>/dev/null
  BANT=$(realpath bazel-bin/external/bant*/bant/bant | head -1)
fi

echo $BANT
