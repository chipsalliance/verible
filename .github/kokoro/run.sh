#!/bin/bash

set -e

CALLED=$_
[[ "${BASH_SOURCE[0]}" != "${0}" ]] && SOURCED=1 || SOURCED=0

SCRIPT_SRC="$(realpath ${BASH_SOURCE[0]})"
SCRIPT_DIR="$(dirname "${SCRIPT_SRC}")"

export PATH="/usr/sbin:/usr/bin:/sbin:/bin"

cd github/$KOKORO_DIR

. $SCRIPT_DIR/steps/git.sh
. $SCRIPT_DIR/steps/hostsetup.sh
. $SCRIPT_DIR/steps/hostinfo.sh

echo
echo "---------------------------------------------------------------"
echo " Building Verible"
echo "---------------------------------------------------------------"
set -x
bazel build --cxxopt='-std=c++17' //...
set +x

echo
echo "---------------------------------------------------------------"
echo " Testing Verible"
echo "---------------------------------------------------------------"
set -x
bazel test --cxxopt='-std=c++17' //...
set +x
