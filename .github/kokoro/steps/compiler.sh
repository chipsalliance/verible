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


# LLVM - Sources
# wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key | apt-key add -
# LLVM_VERSION_STRING = 8, 9, etc
# add-apt-repository "deb http://apt.llvm.org/bionic/   llvm-toolchain-bionic$LLVM_VERSION_STRING main"

echo
echo "========================================"
echo "Adding Ubuntu Compiler PPA"
echo "----------------------------------------"
sudo add-apt-repository ppa:ubuntu-toolchain-r/test
echo "----------------------------------------"

echo
echo "========================================"
echo "Update packages index"
echo "----------------------------------------"
sudo apt-get update
echo "----------------------------------------"

echo
echo "========================================"
echo "Install compiler"
echo "----------------------------------------"
sudo apt-get install -y \
        cpp-6 \
        cpp-7 \
        cpp-8 \
        cpp-9 \
        g++-6 \
        g++-7 \
        g++-8 \
        g++-9 \
        gcc-6 \
        gcc-7 \
        gcc-8 \
        gcc-9 \

echo "----------------------------------------"
dpkg --list | grep gcc-
echo "----------------------------------------"


echo
echo "========================================"
echo "Setting up compiler infrastructure"
echo "----------------------------------------"
# g++
sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-9 150
sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-8 100
sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-7 50
sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-6 25
# gcc
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-9 150
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-8 100
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-7 50
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-6 25
# cpp
sudo update-alternatives --install /usr/bin/cpp cpp-bin /usr/bin/cpp-9 150
sudo update-alternatives --install /usr/bin/cpp cpp-bin /usr/bin/cpp-8 100
sudo update-alternatives --install /usr/bin/cpp cpp-bin /usr/bin/cpp-7 50
sudo update-alternatives --install /usr/bin/cpp cpp-bin /usr/bin/cpp-6 25
# set alternatives
sudo update-alternatives --set g++ /usr/bin/g++-7
sudo update-alternatives --set gcc /usr/bin/gcc-7
sudo update-alternatives --set cpp-bin /usr/bin/cpp-7
echo "----------------------------------------"
