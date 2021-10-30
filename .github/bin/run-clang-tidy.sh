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

set -u
TMPDIR="${TMPDIR:-/tmp}"

readonly PARALLEL_COUNT=$(nproc)
readonly FILES_PER_INVOCATION=5

readonly CLANG_TIDY_SEEN_CACHE=${TMPDIR}/clang-tidy-hashes.cache
touch ${CLANG_TIDY_SEEN_CACHE}  # Just in case it is not there yet

readonly FILES_TO_PROCESS=${TMPDIR}/clang-tidy-files.list
readonly TIDY_OUT=${TMPDIR}/clang-tidy.out

readonly CLANG_TIDY=clang-tidy-12
hash ${CLANG_TIDY} || exit 2  # make sure it is installed.

echo ::group::Build compilation database

# First, build the compilation database.
time bazel build :compdb > /dev/null 2>&1

readonly EXEC_ROOT=$(bazel info execution_root)

# Fix up the __EXEC_ROOT__ to the path used by bazel.
cat bazel-bin/compile_commands.json \
  | sed "s|__EXEC_ROOT__|$EXEC_ROOT|" \
  | sed 's/-fno-canonical-system-headers//g' \
        > compile_commands.json

# The compdb doesn't include tests currently, so we exclude all test files
# for now.
# Also, exclude kythe for now, as it is somehwat noisy and should be
# addressed separately.
#
# We create a hash of each file content to only have to look at new files.
# (TODO: could the tidy result be different if an include content changes ?
#  Then we have to do g++ -E (using compilation database knowing about -I etc.)
#  or combine hashes of all mentioned #includes that are also in our list)
find . -name "*.cc" -and -not -name "*test*.cc" \
     -or -name "*.h" -and -not -name "*test*.h" \
  | grep -v "verilog/tools/kythe" \
  | xargs md5sum | sort \
  > ${CLANG_TIDY_SEEN_CACHE}.new

join -v2 ${CLANG_TIDY_SEEN_CACHE} ${CLANG_TIDY_SEEN_CACHE}.new \
  | awk '{print $2}' | sort > ${FILES_TO_PROCESS}

echo "::group::$(wc -l < ${FILES_TO_PROCESS}) files to process"
cat ${FILES_TO_PROCESS}
echo "::endgroup::"

if [ -s ${FILES_TO_PROCESS} ]; then
  echo "::group::Run ${PARALLEL_COUNT} parallel invocations of ${CLANG_TIDY} in chunks of ${FILES_PER_INVOCATION} files."

  cat ${FILES_TO_PROCESS} \
    | xargs -P${PARALLEL_COUNT} -n ${FILES_PER_INVOCATION} -- \
            ${CLANG_TIDY} --quiet 2>/dev/null \
    | sed "s|$EXEC_ROOT/||g" > ${TIDY_OUT}

  cat ${TIDY_OUT}

  echo "::endgroup::"

  if [ -s ${TIDY_OUT} ]; then
    echo "::error::There were clang-tidy warnings. Please fix"
    exit 1
  fi
else
  echo "Skipping clang-tidy run: nothing to do"
fi

# No complaints. We can cache this list now as baseline for next time.
cp ${CLANG_TIDY_SEEN_CACHE}.new ${CLANG_TIDY_SEEN_CACHE}

echo "No clang-tidy complaints.😎"
exit 0
