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

KYTHE_DIRNAME="kythe-${KYTHE_VERSION}"
KYTHE_DIR_ABS="$(readlink -f "kythe-bin/${KYTHE_DIRNAME}")"

# Create the kythe output directory
KYTHE_OUTPUT_DIRECTORY="${PWD}/kythe_output"
mkdir -p "${KYTHE_OUTPUT_DIRECTORY}"

# Build everything in Verible to index its source
if [[ "$KYTHE_VERSION" == "master" ]]
then
  # Attempt to build kythe's tools on-the-fly.
  # This will drag in massive dependencies like LLVM.
  # :extract_cxx still points to locally installed kythe binaries,
  # TODO: may need to hack verible/BUILD to adjust locations.
  bazel \
    build \
    --experimental_action_listener=":extract_cxx" \
    --define="kythe_corpus=github.com/chipsalliance/verible" \
    -- \
    //...
else
  # Use kythe's released tools.
  # --override_repository kythe_release expects an absolute dir
  bazel \
    --bazelrc="${KYTHE_DIR_ABS}/extractors.bazelrc" \
    build \
    --override_repository kythe_release="${KYTHE_DIR_ABS}" \
    --define="kythe_corpus=github.com/chipsalliance/verible" \
    -- \
    //...
fi

# Merge the kzips and move them to kokoro artifacts directory.
# Note: kzip tool path assumes it came with the release pre-built.
"$KYTHE_DIR_ABS/tools/kzip" merge \
  --output "${KYTHE_OUTPUT_DIRECTORY}/${GIT_VERSION}_$(date -u +%Y-%m-%d-%H%M).kzip" \
  $(find bazel-out/*/extra_actions/ -name "*.kzip")
