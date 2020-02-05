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
PREFIX_BIN=$PREFIX/bin
PREFIX_DOC=$PREFIX/share/verible
PREFIX_MAN=$PREFIX/share/man/man1
mkdir -p $PREFIX
mkdir -p $PREFIX_BIN
mkdir -p $PREFIX_DOC
mkdir -p $PREFIX_MAN

# Binaries
cp -a bazel-bin/verilog/tools/syntax/verilog_syntax     $PREFIX_BIN
cp -a bazel-bin/verilog/tools/lint/verilog_lint         $PREFIX_BIN
cp -a bazel-bin/verilog/tools/formatter/verilog_format  $PREFIX_BIN

for BIN in $PREFIX_BIN/*; do
    ls -l $BIN
    file $BIN
    ldd $BIN
done

# Documentation
cp -a /tmp/pages/* $PREFIX_DOC
# Man pages
gflags2man --help_flag="--helpfull" --dest_dir $PREFIX_MAN bazel-bin/verilog/tools/syntax/verilog_syntax
gzip $PREFIX_MAN/verilog_lint.1
gflags2man --help_flag="--helpfull" --dest_dir $PREFIX_MAN bazel-bin/verilog/tools/lint/verilog_lint
gzip $PREFIX_MAN/verilog_lint.1
gflags2man  --help_flag="--helpfull" --dest_dir $PREFIX_MAN bazel-bin/verilog/tools/formatter/verilog_format
gzip $PREFIX_MAN/verilog_format.1

DISTRO=$(lsb_release --short --id)
DISTRO_RELEASE=$(lsb_release --short --release)
DISTRO_CODENAME=$(lsb_release --short --codename)
TARBALL=$RELEASE_DIR/verible-$TRAVIS_TAG-$TRAVIS_OS_NAME-$TRAVIS_CPU_ARCH-$DISTRO-$DISTRO_RELEASE-$DISTRO_CODENAME.tar.gz
(
    cd $RELEASE_DIR
    tar -zcvf $TARBALL verible-$TRAVIS_TAG
)
