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

mkdir -p "$KOKORO_ARTIFACTS_DIR/bazel_test_logs"
# rename all test.log to sponge_log.log and then copy them to the kokoro
# artifacts directory.
find -L . -name "test.log" -exec rename 's/test.log/sponge_log.log/' {} \;
find -L . -name "sponge_log.log" -exec cp --parents {} "$KOKORO_ARTIFACTS_DIR/bazel_test_logs" \;
# rename all test.xml to sponge_log.xml and then copy them to kokoro
# artifacts directory.
find -L . -name "test.xml" -exec rename 's/test.xml/sponge_log.xml/' {} \;
find -L . -name "sponge_log.xml" -exec cp --parents {} "$KOKORO_ARTIFACTS_DIR/bazel_test_logs" \;
