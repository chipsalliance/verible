#!/bin/bash

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
