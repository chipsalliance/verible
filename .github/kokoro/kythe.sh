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

# This script indexes Verible's source code using kythe and its C++ indexer.
# This can be run as a continuous integration step to produce "kzip" files
# on every commit.

set -x
set -e

# Source the internal common scripts.
source "${KOKORO_GFILE_DIR}/common_google.sh"

echo "Selecting modern version of bazel"
# TODO(b/171989992): This setup should be done elsewhere in a common area.
# This is a short-term workaround to finding the wrong version of bazel
# in /usr/local/bin, when the one we want is in /usr/bin/bazel.
# Copied from steps/hostsetup.sh.
BAZEL_VERSION=3.7.0
wget --no-verbose "https://github.com/bazelbuild/bazel/releases/download/${BAZEL_VERSION}/bazel_${BAZEL_VERSION}-linux-x86_64.deb"
sudo dpkg -i "bazel_${BAZEL_VERSION}-linux-x86_64.deb"
BAZEL=/usr/bin/bazel

# Rely on set -e to fail on first missing prereq.
echo "Checking prerequisites ..."
which wget
which unzip
which tar
which "$BAZEL" && "$BAZEL" version
echo "... done."

cd "${TMPDIR}"

echo "Fetching kythe"
# TODO(b/171989992): revert to using release version after upgrading system
#   libraries/image (need: GLIBCXX_3.4.26, CXXABI_1.3.11, GLIBC_2.29).
# KYTHE_VERSION=master
KYTHE_VERSION=v0.0.48

if [[ "$KYTHE_VERSION" == "master" ]]
then
  # Note: top-of-tree archive does not come with binaries
  # Caution: Building kythe itself will require all of its build dependencies
  # and be extremely compute-intensive.
  wget --no-verbose -O kythe.zip \
    https://github.com/kythe/kythe/archive/master.zip
  unzip kythe.zip
else
  # Use release, which comes with pre-built binaries
  wget --no-verbose -O kythe.tar.gz \
    "https://github.com/kythe/kythe/releases/download/$KYTHE_VERSION/kythe-$KYTHE_VERSION.tar.gz"
  tar -xzf kythe.tar.gz
fi

KYTHE_DIRNAME="kythe-${KYTHE_VERSION}"
KYTHE_DIR_ABS="$(readlink -f "${KYTHE_DIRNAME}")"

# TODO(fangism): KYTHE_DIR is not referenced outside of this script.
#   Remove the following?  Remove reliance on .bashrc variables where possible.
if grep -q "KYTHE_DIR=" ~/.bashrc
then
  sed -i -e "/KYTHE_DIR=/s|=.*|=${KYTHE_DIR_ABS}|" ~/.bashrc
else
  echo "KYTHE_DIR=${KYTHE_DIR_ABS}" >> ~/.bashrc
fi

# Prepare the environment for verible build
cd "${KOKORO_ARTIFACTS_DIR}/github/verible"

# Cleanup previous build cache.
# or: "$BAZEL" clean
rm -rf "$HOME/.cache/bazel"

# TODO(hzeller): bazelisk section looks dead/unused.
# Clean-up in favor of installing a versioned release,
# like in steps/hostsetup.sh.
#
# https://github.com/bazelbuild/bazelisk
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
  date
}

# Create the kythe output directory
KYTHE_OUTPUT_DIRECTORY="${KOKORO_ARTIFACTS_DIR}/kythe_output"
mkdir -p "${KYTHE_OUTPUT_DIRECTORY}"

# Build everything in Verible to index its source
if [[ "$KYTHE_VERSION" == "master" ]]
then
  # Attempt to build kythe's tools on-the-fly.
  # This will drag in massive dependencies like LLVM.
  # :extract_cxx still points to locally installed kythe binaries,
  # TODO: may need to hack verible/BUILD to adjust locations.
  "$BAZEL" \
    build \
    --experimental_action_listener=":extract_cxx" \
    --define="kythe_corpus=github.com/google/verible" \
    -- \
    //...
else
  # Use kythe's released tools.
  # --override_repository kythe_release expects an absolute dir
  "$BAZEL" \
    --bazelrc="${KYTHE_DIR_ABS}/extractors.bazelrc" \
    build \
    --override_repository kythe_release="${KYTHE_DIR_ABS}" \
    --define="kythe_corpus=github.com/google/verible" \
    -- \
    //...
fi

# Merge the kzips and move them to kokoro artifacts directory.
# Note: kzip tool path assumes it came with the release pre-built.
"$KYTHE_DIR_ABS/tools/kzip" merge \
  --output "${KYTHE_OUTPUT_DIRECTORY}/${KOKORO_GIT_COMMIT}_${KOKORO_BUILD_NUMBER}.kzip" \
  $(find bazel-out/*/extra_actions/ -name "*.kzip")
