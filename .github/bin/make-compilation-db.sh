#!/usr/bin/env bash
# Copyright 2021 The Verible Authors.
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
set -o pipefail

readonly EXEC_ROOT=$(bazel info execution_root)

# First, build the compilation database baseline with placeholders for exec-root
bazel build :compdb > /dev/null 2>&1

# Fix up the __EXEC_ROOT__ to the path used by bazel and put the resulting
# db with the expected name in the root directory of the project.
cat bazel-bin/compile_commands.json \
  | sed "s|__EXEC_ROOT__|$EXEC_ROOT|" \
  | sed 's/-fno-canonical-system-headers//g' \
        > compile_commands.json
