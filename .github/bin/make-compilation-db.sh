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

BANT=${BANT:-bant}

if [ ! -e bazel-bin ]; then
  echo "Before creating compilation DB, need to run bazel build first"
  exit 1
fi

if command -v ${BANT} >/dev/null; then
  ${BANT} compile-flags > compile_flags.txt

  # Bant does not see yet the flex dependency inside the toolchain
  for d in bazel-out/../../../external/*flex*/src/FlexLexer.h ; do
    echo "-I$(dirname $d)" >> compile_flags.txt
  done
else
  echo "To create compilation DB, need to have http://bant.build/ installed or provided in BANT environment variable."
  exit 1
fi
