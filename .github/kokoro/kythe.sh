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

set -x
set -e

# Source the internal common scripts.
source "${KOKORO_GFILE_DIR}/common_google.sh"

# Cleanup previous build cache.
rm -rf "$HOME/.cache/bazel"

# Setup kythe
export KYTHE_VERSION=v0.0.48
cd "${TMPDIR}"
wget -q -O kythe.tar.gz \
    https://github.com/kythe/kythe/releases/download/$KYTHE_VERSION/kythe-$KYTHE_VERSION.tar.gz
tar -xzf kythe.tar.gz
export KYTHE_DIRNAME="kythe-${KYTHE_VERSION}"
sudo cp -R "${KYTHE_DIRNAME}" /opt
export KYTHE_DIR="/opt/${KYTHE_DIRNAME}"
sudo chmod -R 755 "${KYTHE_DIR}"
echo "KYTHE_DIR=${KYTHE_DIR}" >> ~/.bashrc

# Prepare the environment for verible build
cd "${KOKORO_ARTIFACTS_DIR}/github/verible/"

# Downloads bazelisk to ~/bin as `bazel`.
function set_bazel_outdir {
  mkdir -p /tmpfs/bazel_output
  export TEST_TMPDIR=/tmpfs/bazel_output
}
function install_bazelisk {
  date
  case "$(uname -s)" in
    Darwin) local name=bazelisk-darwin-amd64 ;;
    Linux)  local name=bazelisk-linux-amd64  ;;
    *) die "Unknown OS: $(uname -s)" ;;
  esac
  mkdir -p "$HOME/bin"
  wget --no-verbose -O "$HOME/bin/bazel" \
      "https://github.com/bazelbuild/bazelisk/releases/download/v1.3.0/$name"
  chmod u+x "$HOME/bin/bazel"
  if [[ ! ":$PATH:" =~ :"$HOME"/bin/?: ]]; then
    PATH="$HOME/bin:$PATH"
  fi
  set_bazel_outdir
  which bazel
  bazel version
  date
}

# Create the kythe output directory
export KYTHE_OUTPUT_DIRECTORY="${KOKORO_ARTIFACTS_DIR}/kythe_output"
mkdir -p "${KYTHE_OUTPUT_DIRECTORY}"

# Build everything
bazel --bazelrc="${KYTHE_DIR}/extractors.bazelrc" \
  build --override_repository kythe_release="${KYTHE_DIR}" \
  --define=kythe_corpus=github.com/google/verible \
  -- \
  //...

# Merge the kzips and move them to kokoro artifacts directory.
"$KYTHE_DIR/tools/kzip" merge \
  --output "${KYTHE_OUTPUT_DIRECTORY}/${KOKORO_GIT_COMMIT}_${KOKORO_BUILD_NUMBER}.kzip" \
  $(find bazel-out/*/extra_actions/ -name "*.kzip")
