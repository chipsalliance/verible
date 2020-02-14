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

# Invoke bazel with --workspace_status_command=bazel/build-version.sh to
# get this invoked and populate bazel-out/volatile-status.sh
GIT_DATE="$(git log -n1 --format='"%cs"')"
GIT_DESCRIBE="$(git describe)"

test -z "$GIT_DATE" || echo "GIT_DATE $GIT_DATE"
test -z "$GIT_DESCRIBE" || echo "GIT_DESCRIBE \"$GIT_DESCRIBE\""

