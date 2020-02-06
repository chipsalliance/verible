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

# GitHub Releases
# ---------------
# Generate the GitHub Releases to deploy

RELEASE_DIR=/tmp/releases
rm -rf $RELEASE_DIR

PREFIX=$RELEASE_DIR/verible-$TRAVIS_TAG
make PREFIX=$PREFIX install

DISTRO=$(lsb_release --short --id)
DISTRO_RELEASE=$(lsb_release --short --release)
DISTRO_CODENAME=$(lsb_release --short --codename)
TARBALL=$RELEASE_DIR/verible-$TRAVIS_TAG-$TRAVIS_OS_NAME-$TRAVIS_CPU_ARCH-$DISTRO-$DISTRO_RELEASE-$DISTRO_CODENAME.tar.gz
(
    cd $RELEASE_DIR
    tar -zcvf $TARBALL verible-$TRAVIS_TAG
)
