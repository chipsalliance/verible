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
export TARGET_LINK=${LINK_TYPE:-"dynamic"}

export TAG=${TAG:-$(git describe --match=v*)}

if [ -z "${BAZEL_VERSION}" ]; then
  echo "Make sure that \$BAZEL_VERSION ($BAZEL_VERSION) is set."
  echo " (try 'source ../.github/settings.sh')"
  exit 1
fi
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

# Generate the Dockerfile

OUT_DIR=${TARGET_OS}-${TARGET_VERSION}

# Basic tools
mkdir -p "${OUT_DIR}"
sed "s#${TARGET_OS}:VERSION#${TARGET_OS}:${TARGET_VERSION}#g" ${TARGET_OS}.dockerfile > ${OUT_DIR}/Dockerfile

BAZEL_OPTS="${BAZEL_OPTS} --java_runtime_version=local_jdk --tool_java_runtime_version=local_jdk"

case "$TARGET_OS" in
  ubuntu)
    # Compiler
    [ "$TARGET_VERSION" = xenial ] || [ "$TARGET_VERSION" = bionic ] && _version="$TARGET_VERSION" || _version="common"
    cat ${TARGET_OS}/${_version}/compiler.dockerstage >> ${OUT_DIR}/Dockerfile

    # Use local flex/bison on Jammy
    if [ "$TARGET_VERSION" = jammy ]; then
      BAZEL_OPTS="${BAZEL_OPTS} --//bazel:use_local_flex_bison"
      echo 'RUN apt-get install -y flex bison' >> ${OUT_DIR}/Dockerfile
    fi
  ;;
  centos)
    # Compiler
    cat ${TARGET_OS}/common/compiler.dockerstage >> ${OUT_DIR}/Dockerfile
  ;;
esac

if [ "${TARGET_LINK}" = "static" ]; then
  BAZEL_OPTS="${BAZEL_OPTS} --config=create_static_linked_executables"
  # Bazel link options with static libs cause error with
  # --config=create_static_linked_executables and they are redundant
  sed -i '/ENV BAZEL_LINK/d' ${OUT_DIR}/Dockerfile
fi

# Bazel
cat bazel.dockerstage >> ${OUT_DIR}/Dockerfile
# help2man
cat ${TARGET_OS}/common/help2man.dockerstage >> ${OUT_DIR}/Dockerfile

# ==================================================================

REPO_SLUG=${GITHUB_REPOSITORY_SLUG:-google/verible}
GIT_DATE=${GIT_DATE:-$(git show -s --format=%ci)}
GIT_VERSION=${GIT_VERSION:-$(git describe --match=v*)}
GIT_HASH=${GIT_HASH:-$(git rev-parse HEAD)}

# Build Verible

cat >> ${OUT_DIR}/Dockerfile <<EOF
ENV BAZEL_OPTS "${BAZEL_OPTS}"
ENV BAZEL_CXXOPTS "${BAZEL_CXXOPTS}"

ENV REPO_SLUG $REPO_SLUG
ENV GIT_VERSION $GIT_VERSION
ENV GIT_DATE $GIT_DATE
ENV GIT_HASH $GIT_HASH

RUN which bazel
RUN bazel --version

ADD verible-$GIT_VERSION.tar.gz /src/verible
WORKDIR /src/verible/verible-$GIT_VERSION

RUN bazel build --workspace_status_command=bazel/build-version.py $BAZEL_OPTS //...
EOF

(cd .. ; git archive --prefix verible-$GIT_VERSION/ --output releasing/$TARGET/verible-$GIT_VERSION.tar.gz HEAD)

IMAGE="verible:$TARGET-$TAG"

echo "::group::Docker file for $IMAGE"
cat $TARGET/Dockerfile
echo '::endgroup::'

if [[ "${ARCH}" != "arm64" ]]; then
    ARCH="x86_64"
fi

echo "::group::Docker build $IMAGE"
docker build \
  --tag $IMAGE \
  --build-arg BAZEL_VERSION="$BAZEL_VERSION" \
  --build-arg ARCH="${ARCH}" \
  --build-arg TARGET_VERSION="$TARGET_VERSION" \
  $TARGET
echo '::endgroup::'

echo "::group::Build in container"
mkdir -p out
docker run --rm \
  -e BAZEL_OPTS="${BAZEL_OPTS}" \
  -e BAZEL_CXXOPTS="${BAZEL_CXXOPTS}" \
  -e REPO_SLUG="${GITHUB_REPOSITORY_SLUG:-google/verible}" \
  -e GIT_VERSION="${GIT_VERSION:-$(git describe --match=v*)}" \
  -e GIT_DATE="${GIT_DATE:-$(git show -s --format=%ci)}" \
  -e GIT_HASH="${GIT_HASH:-$(git rev-parse HEAD)}" \
  -e TARGET_LINK \
  -v $(pwd)/out:/out \
  $IMAGE \
  ./releasing/build.sh
echo '::endgroup::'
