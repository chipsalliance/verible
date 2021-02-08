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

# This script generates Dockerfile scripts to produce images used for
# releasing binary packages.

set -e

DIRS=${1:-$(find -mindepth 1 -maxdepth 1 -type d)}

REPO_SLUG=${GITHUB_REPOSITORY_SLUG:-google/verible}
GIT_DATE=${GIT_DATE:-$(git show -s --format=%ci)}
GIT_VERSION=${GIT_VERSION:-$(git describe --match=v*)}
GIT_HASH=${GIT_HASH:-$(git rev-parse HEAD)}

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
for UBUNTU_VERSION in xenial bionic focal groovy; do
    # Install basic tools
    # --------------------------------------------------------------
    sed "s#ubuntu:VERSION#ubuntu:${UBUNTU_VERSION}#g" $(dirname "$0")/ubuntu.dockerfile > ubuntu-${UBUNTU_VERSION}/Dockerfile

    # Install compiler
    # --------------------------------------------------------------
    case $UBUNTU_VERSION in
        xenial)
            cat >> ubuntu-${UBUNTU_VERSION}/Dockerfile <<EOF
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
            cat >> ubuntu-${UBUNTU_VERSION}/Dockerfile <<EOF

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
    cat >> ubuntu-${UBUNTU_VERSION}/Dockerfile <<EOF

# Install bazel
RUN \\
    wget --no-verbose "https://github.com/bazelbuild/bazel/releases/download/${BAZEL_VERSION}/bazel_${BAZEL_VERSION}-linux-x86_64.deb" -O /tmp/bazel.deb; \\
    dpkg -i /tmp/bazel.deb || true; \\
    apt-get -f install -y

EOF

done

# ==================================================================
# Generate the Docker files for centos versions
# ==================================================================
for CENTOS_VERSION in 7 8; do
    # Install basic tools
    # --------------------------------------------------------------
    sed "s#centos:VERSION#centos:${CENTOS_VERSION}#g" $(dirname "$0")/centos.dockerfile > centos-${CENTOS_VERSION}/Dockerfile

    # Install compiler
    # --------------------------------------------------------------
    case $CENTOS_VERSION in
        6|7)
            cat >> centos-${CENTOS_VERSION}/Dockerfile <<EOF
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
            cat >> centos-${CENTOS_VERSION}/Dockerfile <<EOF
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
            echo "Unknown Centos version ${CENTOS_VERSION} when installing compiler."
            exit 1
            ;;
    esac

    # Install Bazel
    # --------------------------------------------------------------
    cat >> centos-${CENTOS_VERSION}/Dockerfile <<EOF

# Install bazel
RUN yum install -y --nogpgcheck \\
    java-1.8.0-openjdk-devel \\
    unzip \\
    zip

RUN java -version
RUN javac -version
EOF

    case $CENTOS_VERSION in
        6)
            cat >> centos-${CENTOS_VERSION}/Dockerfile <<EOF
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
            cat >> centos-${CENTOS_VERSION}/Dockerfile <<EOF
ADD https://copr.fedorainfracloud.org/coprs/vbatts/bazel/repo/epel-${CENTOS_VERSION}/vbatts-bazel-epel-${CENTOS_VERSION}.repo /etc/yum.repos.d
RUN yum install -y --nogpgcheck bazel3
EOF
            ;;
        *)
            echo "Unknown Centos version ${CENTOS_VERSION} when installing bazel"
            exit 1
            ;;
    esac
    cat >> centos-${CENTOS_VERSION}/Dockerfile <<EOF
RUN bazel --version
EOF

done
# ==================================================================

for DFILE in $(find -name Dockerfile); do
    # Install gflags2man
    # --------------------------------------------------------------
    cat >> $DFILE <<EOF

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
EOF

    # Build Verible
    # ==================================================================
    cat >> $DFILE <<EOF

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
RUN echo $REPO_SLUG
RUN echo $GIT_DATE
RUN echo $GIT_HASH
RUN echo $GIT_VERSION

RUN ./.github/workflows/github-pages-setup.sh
RUN ./.github/workflows/github-releases-setup.sh /out/
EOF
done

# Create archive for each Docker directory
# ==================================================================
for DIR in $DIRS; do
    (cd .. ; git archive --prefix verible-$GIT_VERSION/ --output releasing/$DIR/verible-$GIT_VERSION.tar.gz HEAD)
done
