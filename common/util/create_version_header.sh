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

# The file where bazel stores name/value pairs
BAZEL_VOLATILE=bazel-out/volatile-status.txt

if [ -r "$BAZEL_VOLATILE" ]; then
  (while read NAME VALUE ; do
     case $NAME in
       # Only select the ones we're interested in.
       GIT_* | BUILD_TIMESTAMP)
         echo "#define VERIBLE_${NAME} ${VALUE}"
         ;;
     esac
   done) < "$BAZEL_VOLATILE"
fi
