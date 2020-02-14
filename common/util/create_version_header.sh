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

GIT_HASH="$(grep GIT_HASH bazel-out/volatile-status.txt | cut -f2 -d' ')"
TS_INT=$(grep BUILD_TIMESTAMP bazel-out/volatile-status.txt | cut -f2 -d' ')
TS_STRING="$(date +"%Y-%m-%d %H:%M UTC" -u -d @$TS_INT)"

test -z "$GIT_HASH" || echo "#define VERIBLE_GIT_HASH $GIT_HASH"
test -z "$TS_STRING" || echo "#define VERIBLE_BUILD_TIMESTAMP \"$TS_STRING\""
