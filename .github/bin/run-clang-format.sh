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

set -u  # only use variables once assigned

FORMAT_OUT=${TMPDIR:-/tmp}/clang-format-diff.out

CLANG_FORMAT_BINARY=clang-format

${CLANG_FORMAT_BINARY} --version

# Run on all files.
find . -name "*.h" -o -name "*.cc" | grep -v 'third_party/|external_libs/' \
  | xargs -P2 ${CLANG_FORMAT_BINARY} -i

# Check if we got any diff
git diff > ${FORMAT_OUT}

if [ -s ${FORMAT_OUT} ]; then
   echo "Style not matching (see https://github.com/chipsalliance/verible/blob/master/CONTRIBUTING.md#style)"
   echo "Run"
   echo "  .github/bin/run-clang-format.sh"
   echo "-------------------------------------------------"
   echo
   cat ${FORMAT_OUT}
   exit 1
fi

exit 0
