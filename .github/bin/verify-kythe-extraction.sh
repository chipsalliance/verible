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

set -e

# Verify that Verilog Kythe indexer produces the expected Kythe indexing facts.
# Note: verifier tool path assumes it came with the release pre-built.
KYTHE_DIRNAME="kythe-${KYTHE_VERSION}"
KYTHE_DIR_ABS="$(readlink -f "kythe-bin/${KYTHE_DIRNAME}")"
bazel test --test_output=errors --test_arg="$KYTHE_DIR_ABS/tools/verifier" verilog/tools/kythe:verification_test
