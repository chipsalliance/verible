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

# This is pretty noisy.
bazel run -c opt @hedron_compile_commands//:refresh_all > /dev/null 2>&1

sed -i -e 's/bazel-out\/.*\/bin\/external\//bazel-out\/..\/..\/..\/external\//g' compile_commands.json
sed -i -e 's/bazel-out\/.*\/bin/bazel-bin/g' compile_commands.json
