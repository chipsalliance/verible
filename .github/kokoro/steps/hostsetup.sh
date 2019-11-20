#!/bin/bash

set -e

# LLVM - Sources
# wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key | apt-key add -
# LLVM_VERSION_STRING = 8, 9, etc
# add-apt-repository "deb http://apt.llvm.org/bionic/   llvm-toolchain-bionic$LLVM_VERSION_STRING main"

echo
echo "========================================"
echo "Adding Ubuntu Toolchain PPA"
echo "----------------------------------------"
sudo add-apt-repository ppa:ubuntu-toolchain-r/test
echo "----------------------------------------"


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
        g++-9 \
        gcc-9 \
        git \
        m4 \
        make \
        psmisc \
        wget \


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
