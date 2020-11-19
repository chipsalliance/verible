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

if [ -z "${BAZEL_VERSION}" ]; then
       echo "Set \$BAZEL_VERSION"
       exit 1
fi

set -e

echo
echo "========================================"
echo "Host updating packages"
echo "----------------------------------------"
sudo apt-get update
echo "----------------------------------------"

echo
echo "========================================"
echo "Host install packages"
echo "----------------------------------------"
sudo apt-get install -y \
            bash \
            build-essential \
            ca-certificates \
            colordiff \
            coreutils \
            git \
            m4 \
            make \
            psmisc \
            wget
echo "----------------------------------------"

echo
echo "========================================"
echo "Install packages required to compile Kythe"
echo "----------------------------------------"
# From: https://www.kythe.io/getting-started/
sudo apt-get install -y \
           asciidoc \
           asciidoctor \
           bison \
           flex \
           graphviz \
           libncurses-dev \
           openjdk-8-jdk \
           parallel \
           source-highlight \
           uuid-dev
echo "----------------------------------------"

echo
echo "========================================"
echo "Installing bazel"
echo "----------------------------------------"
(
  set -x
  bazel version || true

  wget "https://github.com/bazelbuild/bazel/releases/download/${BAZEL_VERSION}/bazel_${BAZEL_VERSION}-linux-x86_64.deb"
  sudo dpkg -i "bazel_${BAZEL_VERSION}-linux-x86_64.deb"
  sudo apt-get -f install -y

  dpkg --listfiles bazel

  which bazel
  bazel version
)
