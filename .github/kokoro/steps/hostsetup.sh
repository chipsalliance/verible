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
echo "Installing bazel"
echo "----------------------------------------"
(
  set -x
  bazel version || true

  wget https://github.com/bazelbuild/bazel/releases/download/1.2.0/bazel_1.2.0-linux-x86_64.deb
  sudo dpkg -i bazel_1.2.0-linux-x86_64.deb

  dpkg --listfiles bazel

  which bazel
  bazel version
)
