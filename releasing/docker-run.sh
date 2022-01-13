#!/usr/bin/env bash
# Copyright 2020-2021 The Verible Authors.
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

# Usage: docker-run.sh container_image task
# where 'container_image' is one of the supported base docker images
# and task is one of 'basic', 'build' or 'verible'

set -e

cd $(dirname "$0")

# Get target OS and version

if [ -z "${1}" ]; then
  echo "Make sure that \$1 ($1) is set."
  exit 1
fi

export TARGET=`echo "$1" | sed 's#:#-#g'`
TARGET_VERSION=`echo $TARGET | cut -d- -f2`
TARGET_OS=`echo $TARGET | cut -d- -f1`

IMAGE_BASIC="verible:$TARGET-basic"
IMAGE_BUILD="verible:$TARGET-bazel"

do_img_basic() {
echo "> Build image $IMAGE_BASIC"
  BUILD_TARGET=""
  case "$TARGET_OS" in
    ubuntu) [ "$TARGET_VERSION" = 'xenial' ] || [ "$TARGET_VERSION" = 'bionic' ] && BUILD_TARGET='--target=xenial' || true ;;
    centos) [ "$TARGET_VERSION" = '8' ] && BUILD_TARGET='--target=eight' || true ;;
  esac
  docker build -t "$IMAGE_BASIC" \
    --build-arg _VERSION_="$TARGET_VERSION" \
    $BUILD_TARGET - < $TARGET_OS.basic.dockerfile
}

do_img_build() {
if [ -z "${BAZEL_VERSION}" ]; then
  echo "Make sure that \$BAZEL_VERSION ($BAZEL_VERSION) is set."
  echo " (try 'source ../.github/settings.sh')"
  exit 1
fi

echo "> Build image $IMAGE_BUILD"
  BUILD_TARGET=""
  docker build -t "$IMAGE_BUILD" \
    --build-arg _VERSION_="$TARGET_VERSION" \
    --build-arg _BAZEL_VERSION_="$BAZEL_VERSION" \
    $BUILD_TARGET - < $TARGET_OS.build.dockerfile
}

build_verible() {
if [ -z "${BAZEL_OPTS}" ]; then
  echo "Make sure that \$BAZEL_OPTS ($BAZEL_OPTS) is set."
  echo " (try 'source ../.github/settings.sh')"
  exit 1
fi
if [ -z "${BAZEL_CXXOPTS}" ]; then
  echo "Make sure that \$BAZEL_CXXOPTS ($BAZEL_CXXOPTS) is set."
  echo " (try 'source ../.github/settings.sh')"
  exit 1
fi

GIT_VERSION=${GIT_VERSION:-$(git describe --match=v*)}
tarball="verible-$GIT_VERSION.tar.gz"
(
  cd ..
  git archive --prefix verible-$GIT_VERSION/ --output releasing/"$tarball" HEAD

)
mkdir -p $TARGET
cd $TARGET
tar -xf ../"$tarball"
cd ..
rm "$tarball"

echo "Build Verible (verible:$TARGET-${TAG:-$(git describe --match=v*)})"
docker run --rm \
  -e BAZEL_OPTS="$BAZEL_OPTS" \
  -e BAZEL_CXXOPTS="$BAZEL_CXXOPTS" \
  -e REPO_SLUG="${GITHUB_REPOSITORY_SLUG:-google/verible}" \
  -e GIT_VERSION="$GIT_VERSION" \
  -e GIT_DATE="${GIT_DATE:-$(git show -s --format=%ci)}" \
  -e GIT_HASH="${GIT_HASH:-$(git rev-parse HEAD)}" \
  -v $(pwd):/wrk \
  -w /wrk/$TARGET/verible-$GIT_VERSION \
  "$IMAGE_BUILD" \
  /wrk/build.sh
}

case "$2" in
  prepare-image-basic) do_img_basic ;;
  prepare-image-build) do_img_build ;;
  build-verible-release) build_verible ;;
esac
