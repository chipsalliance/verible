#!/usr/bin/env bash
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

cd $(dirname "$0")

# Get target OS and version

if [ -z "${1}" ]; then
  echo "Make sure that \$1 ($1) is set."
  exit 1
fi

export TARGET=`echo "$1" | sed 's#:#-#g'`

TARGET_VERSION=`echo $TARGET | cut -d- -f2`
TARGET_OS=`echo $TARGET | cut -d- -f1`

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

# ==================================================================
# Generate the Docker files for ubuntu versions
# ==================================================================
if [ "$TARGET_OS" = "ubuntu" ]; then
    # Install basic tools
    # --------------------------------------------------------------
    mkdir -p ubuntu-${TARGET_VERSION}
    sed "s#ubuntu:VERSION#ubuntu:${TARGET_VERSION}#g" ubuntu.dockerfile > ubuntu-${TARGET_VERSION}/Dockerfile

    # Install compiler
    # --------------------------------------------------------------
    case $TARGET_VERSION in
        xenial)
            cat >> ubuntu-${TARGET_VERSION}/Dockerfile <<EOF
# Get a newer GCC and flex version
RUN add-apt-repository ppa:ubuntu-toolchain-r/test; apt-get update
RUN apt-get install -y  \\
    bison               \\
    build-essential     \\
    flex                \\
    g++-7               \\
    gcc-7

RUN ln -sf /usr/bin/gcc-7 /usr/bin/gcc
RUN ln -sf /usr/bin/g++-7 /usr/bin/g++

# Link libstdc++ statically
ENV BAZEL_LINKOPTS "-static-libstdc++:-lm"
ENV BAZEL_LINKLIBS "-l%:libstdc++.a"
EOF
            ;;
        *)
            cat >> ubuntu-${TARGET_VERSION}/Dockerfile <<EOF

# Install compiler
RUN apt-get install -y  \\
    bison               \\
    build-essential     \\
    flex                \\
    g++                 \\
    gcc

EOF
            ;;
    esac

    # Install Bazel
    # --------------------------------------------------------------
    cat >> ubuntu-${TARGET_VERSION}/Dockerfile <<EOF

# Install bazel
RUN \\
    wget --no-verbose "https://github.com/bazelbuild/bazel/releases/download/${BAZEL_VERSION}/bazel_${BAZEL_VERSION}-linux-x86_64.deb" -O /tmp/bazel.deb; \\
    dpkg -i /tmp/bazel.deb || true; \\
    apt-get -f install -y

EOF
fi

# ==================================================================
# Generate the Docker files for centos versions
# ==================================================================
if [ "$TARGET_OS" = "centos" ]; then
    # Install basic tools
    # --------------------------------------------------------------
    mkdir -p centos-${TARGET_VERSION}
    sed "s#centos:VERSION#centos:${TARGET_VERSION}#g" centos.dockerfile > centos-${TARGET_VERSION}/Dockerfile

    # Install compiler
    # --------------------------------------------------------------
    case $TARGET_VERSION in
        6|7)
            cat >> centos-${TARGET_VERSION}/Dockerfile <<EOF
# Link libstdc++ statically so people don't have to install devtoolset-8
# just to use verible.
ENV BAZEL_LINKOPTS "-static-libstdc++:-lm -static-libstdc++:-lrt"
ENV BAZEL_LINKLIBS "-l%:libstdc++.a"

# Get a newer GCC version
RUN yum install -y --nogpgcheck centos-release-scl
RUN yum install -y --nogpgcheck devtoolset-8

RUN yum install -y bison flex

# Build a newer version of flex
RUN \\
    wget --no-verbose "https://github.com/westes/flex/files/981163/flex-2.6.4.tar.gz" && \\
    tar -xvf flex-2.6.4.tar.gz && \\
    cd flex-2.6.4 && \\
    ./configure --prefix=/usr && \\
    make -j && \\
    make install && \\
    cd .. && \\
    rm -rf flex-*
RUN flex --version

SHELL [ "scl", "enable", "devtoolset-8" ]
EOF
            ;;
        8)
            cat >> centos-${TARGET_VERSION}/Dockerfile <<EOF
# Install C++ compiler
RUN \\
    yum -y group install --nogpgcheck "Development Tools" || \\
    yum -y groupinstall --nogpgcheck "Development Tools"

RUN yum install -y bison flex

RUN gcc --version
RUN g++ --version
EOF
            ;;
        *)
            echo "Unknown Centos version ${TARGET_VERSION} when installing compiler."
            exit 1
            ;;
    esac

    # Install Bazel
    # --------------------------------------------------------------
    cat >> centos-${TARGET_VERSION}/Dockerfile <<EOF

# Install bazel
RUN yum install -y --nogpgcheck \\
    java-1.8.0-openjdk-devel \\
    unzip \\
    zip

RUN java -version
RUN javac -version
EOF

    case $TARGET_VERSION in
        6)
            cat >> centos-${TARGET_VERSION}/Dockerfile <<EOF
RUN yum install -y --nogpgcheck rh-python36
SHELL [ "scl", "enable", "rh-python36", "devtoolset-8" ]

RUN python --version
RUN python3 --version

# Build bazel
RUN wget --no-verbose https://github.com/bazelbuild/bazel/releases/download/${BAZEL_VERSION}/bazel-${BAZEL_VERSION}-dist.zip
RUN unzip bazel-${BAZEL_VERSION}-dist.zip -d bazel-${BAZEL_VERSION}-dist
RUN cd bazel-${BAZEL_VERSION}-dist; \\
    EXTRA_BAZEL_ARGS="--copt=-D__STDC_FORMAT_MACROS --host_copt=-D__STDC_FORMAT_MACROS" ./compile.sh
RUN cp bazel-${BAZEL_VERSION}-dist/output/bazel /usr/local/bin
RUN bazel --version

SHELL [ "scl", "enable", "devtoolset-8" ]
EOF
            ;;
        7|8)
            cat >> centos-${TARGET_VERSION}/Dockerfile <<EOF
ADD https://copr.fedorainfracloud.org/coprs/vbatts/bazel/repo/epel-${TARGET_VERSION}/vbatts-bazel-epel-${TARGET_VERSION}.repo /etc/yum.repos.d
RUN yum install -y --nogpgcheck bazel3
EOF
            ;;
        *)
            echo "Unknown Centos version ${TARGET_VERSION} when installing bazel"
            exit 1
            ;;
    esac
    cat >> centos-${TARGET_VERSION}/Dockerfile <<EOF
RUN bazel --version
EOF
fi

# ==================================================================

REPO_SLUG=${GITHUB_REPOSITORY_SLUG:-google/verible}
GIT_DATE=${GIT_DATE:-$(git show -s --format=%ci)}
GIT_VERSION=${GIT_VERSION:-$(git describe --match=v*)}
GIT_HASH=${GIT_HASH:-$(git rev-parse HEAD)}

# Install gflags2man and build Verible
# --------------------------------------------------------------
cat >> ${TARGET_OS}-${TARGET_VERSION}/Dockerfile <<EOF
# Install gflags2man
RUN \\
    wget --no-verbose https://repo.anaconda.com/miniconda/Miniconda2-latest-Linux-x86_64.sh; \\
    chmod a+x Miniconda2-latest-Linux-x86_64.sh; \\
    ./Miniconda2-latest-Linux-x86_64.sh -p /usr/local -b -f; \\
    conda --version
RUN which pip
RUN /usr/local/bin/pip install python-gflags
RUN chmod a+x /usr/local/bin/gflags2man.py
RUN ln -s /usr/local/bin/gflags2man.py /usr/bin/gflags2man
RUN gflags2man

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

RUN bazel build --workspace_status_command=bazel/build-version.sh $BAZEL_OPTS //...
EOF

(cd .. ; git archive --prefix verible-$GIT_VERSION/ --output releasing/$TARGET/verible-$GIT_VERSION.tar.gz HEAD)

IMAGE="verible:$TARGET-$TAG"

echo "::group::Docker file for $IMAGE"
cat $TARGET/Dockerfile
echo '::endgroup::'

echo "::group::Docker build $IMAGE"
docker build --tag $IMAGE $TARGET
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
  -v $(pwd)/out:/out \
  $IMAGE \
  ./releasing/build.sh
echo '::endgroup::'
