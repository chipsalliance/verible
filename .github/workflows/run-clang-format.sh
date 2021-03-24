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
FILE_LIST_OUT=${TMPDIR:-/tmp}/file-list.$$.txt
FORMAT_OUT=${TMPDIR:-/tmp}/clang-format-diff.$$.out

function cleanup {
  rm -f ${FORMAT_OUT} ${FORMAT_LIST_OUT}
}
trap cleanup EXIT

# Run on all the files that are affected

# Finding all affected files and list them for inspection in the log output
git diff --name-only --diff-filter=AM -r origin/master | grep '\(\.cc\|\.h\)$' | tee "${FILE_LIST_OUT}"

for f in $(cat "${FILE_LIST_OUT}") ; do
  cp ${f} ${f}.before-clang-format
  strace -eopen clang-format --verbose -i --style=file ${f} 2> /dev/null
  diff -u ${f}.before-clang-format ${f} >> ${FORMAT_OUT}
  echo "Exit code diff $?"
done

if [ -s "${FORMAT_OUT}" ]; then
   echo "Style not matching (see https://github.com/google/verible/blob/master/CONTRIBUTING.md#style)"
   echo "On your *.h, *.cc files, please run clang-format -i --style=file <your changed files>"
   cat "${FORMAT_OUT}"
   exit 1
fi

exit 0
