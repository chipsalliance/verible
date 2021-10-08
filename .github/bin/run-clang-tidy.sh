#!/bin/bash
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

TIDY_OUT=${TMPDIR:-/tmp}/clang-tidy.out

CLANG_TIDY=clang-tidy-12

hash ${CLANG_TIDY} || exit 2  # make sure it is installed.

# First, build the compilation database.
bazel build :compdb > /dev/null 2>&1

EXEC_ROOT=$(bazel info execution_root)

# Fix up the __EXEC_ROOT__ to the path used by bazel.
cat bazel-bin/compile_commands.json \
  | sed "s|__EXEC_ROOT__|$EXEC_ROOT|" \
  | sed 's/-fno-canonical-system-headers//g' \
        > compile_commands.json

# The compdb doesn't include tests currently, so we exclude all test files
# for now.
# Also, exclude kythe for now, as it is somehwat noisy and should be
# addressed separately.
find . -name "*.cc" -and -not -name "*test*.cc" \
     -or -name "*.h" -and -not -name "*test*.h" \
  | grep -v "verilog/tools/kythe" \
  | xargs -P$(nproc) -n 5 -- \
          ${CLANG_TIDY} --quiet 2>/dev/null \
  | sed "s|$EXEC_ROOT/||g" > ${TIDY_OUT}


cat ${TIDY_OUT}

if [ -s ${TIDY_OUT} ]; then
   echo "There were clang-tidy warnings. Please fix"
   exit 1
fi

echo "No clang-tidy complaints.😎"
exit 0
