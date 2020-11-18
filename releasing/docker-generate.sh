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

DIRS=${1:-$(find -mindepth 1 -maxdepth 1 -type d)}

REPO_SLUG=${GITHUB_REPOSITORY_SLUG:-google/verible}
GIT_DATE=${GIT_DATE:-$(git show -s --format=%ci)}
GIT_VERSION=${GIT_VERSION:-$(git rev-parse --short "$GITHUB_SHA")}
GIT_HASH=${GIT_HASH:-$(git rev-parse HEAD)}

# Generate the docker files for ubuntu versions
for UBUNTU_VERSION in trusty xenial bionic eoan focal; do
    cat > ubuntu-${UBUNTU_VERSION}/Dockerfile <<EOF
FROM ubuntu:$UBUNTU_VERSION

RUN apt-get update
RUN apt-get install -y curl gnupg software-properties-common

RUN \
    curl https://bazel.build/bazel-release.pub.gpg | apt-key add - ; \
    echo "deb [arch=amd64] http://storage.googleapis.com/bazel-apt stable jdk1.8" | tee /etc/apt/sources.list.d/bazel.list ; \
    apt-get update

RUN apt-get install -y \
    bazel \
    build-essential \
    gcc \
    g++ \
    file \
    lsb-release \
    wget \

EOF
    case $UBUNTU_VERSION in
        trusty)
            cat >> ubuntu-${UBUNTU_VERSION}/Dockerfile <<EOF
# Get a newer GCC version
RUN add-apt-repository ppa:ubuntu-toolchain-r/test; \
    apt-get update
RUN apt-get install -y \
    gcc-6 \
    g++-6 \

RUN ln -sf /usr/bin/gcc-6 /usr/bin/gcc
RUN ln -sf /usr/bin/g++-6 /usr/bin/g++

# Link libstdc++ statically
ENV BAZEL_LINKOPTS "-static-libstdc++:-lm"
ENV BAZEL_LINKLIBS "-l%:libstdc++.a"
EOF
            ;;
    esac

done

# Generate the docker files for centos versions
for CENTOS_VERSION in 6 7 8; do
    # Install basic tools
    cat > centos-${CENTOS_VERSION}/Dockerfile <<EOF
FROM centos:$CENTOS_VERSION

RUN yum install -y \
    file \
    redhat-lsb \
    tar \
    git \
    wget \

EOF

    # Install compiler
    case $CENTOS_VERSION in
        6|7)
            cat >> centos-${CENTOS_VERSION}/Dockerfile <<EOF
# Get a newer GCC version
RUN yum install -y --nogpgcheck centos-release-scl
RUN yum install -y --nogpgcheck devtoolset-7
SHELL [ "scl", "enable", "devtoolset-7" ]

# Link libstdc++ statically
ENV BAZEL_LINKOPTS "-static-libstdc++:-lm -static-libstdc++:-lrt"
ENV BAZEL_LINKLIBS "-l%:libstdc++.a"
EOF
            ;;
        8)
            cat >> centos-${CENTOS_VERSION}/Dockerfile <<EOF
# Install C++ compiler
RUN yum -y group install --nogpgcheck "Development Tools" || yum -y groupinstall --nogpgcheck "Development Tools"
RUN gcc --version
RUN g++ --version
EOF
            ;;
    esac

    # Install Bazel
    cat >> centos-${CENTOS_VERSION}/Dockerfile <<EOF
# Install bazel
RUN yum install -y --nogpgcheck \
    java-1.8.0-openjdk-devel \
    unzip \
    zip \

RUN java -version
RUN javac -version
EOF
    case $CENTOS_VERSION in
        6)
            cat >> centos-${CENTOS_VERSION}/Dockerfile <<EOF
RUN yum install -y --nogpgcheck rh-python36
SHELL [ "scl", "enable", "rh-python36", "devtoolset-7" ]

RUN python --version
RUN python3 --version

# Build bazel
RUN wget https://github.com/bazelbuild/bazel/releases/download/2.1.0/bazel-2.1.0-dist.zip
RUN unzip bazel-2.1.0-dist.zip -d bazel-2.1.0-dist
RUN cd bazel-2.1.0-dist; ./compile.sh
RUN cp bazel-2.1.0-dist/output/bazel /usr/local/bin
RUN bazel --version

SHELL [ "scl", "enable", "devtoolset-7" ]
EOF
            ;;
        7|8)
            cat >> centos-${CENTOS_VERSION}/Dockerfile <<EOF
ADD https://copr.fedorainfracloud.org/coprs/vbatts/bazel/repo/epel-${CENTOS_VERSION}/vbatts-bazel-epel-${CENTOS_VERSION}.repo /etc/yum.repos.d
RUN yum install -y --nogpgcheck bazel
EOF
    esac

    # Install gflags2man
    cat >> centos-${CENTOS_VERSION}/Dockerfile <<EOF
# Install gflags2man
RUN \
    wget https://repo.anaconda.com/miniconda/Miniconda2-latest-Linux-x86_64.sh; \
    chmod a+x Miniconda2-latest-Linux-x86_64.sh; \
    ./Miniconda2-latest-Linux-x86_64.sh -p /usr/local -b -f; \
    conda --version
RUN which pip
RUN /usr/local/bin/pip install python-gflags
RUN chmod a+x /usr/local/bin/gflags2man.py
RUN ln -s /usr/local/bin/gflags2man.py /usr/bin/gflags2man
RUN gflags2man
EOF

done

for DFILE in $(find -name Dockerfile); do
    cat >> $DFILE <<EOF

ENV REPO_SLUG $REPO_SLUG
ENV GIT_VERSION $GIT_VERSION
ENV GIT_DATE $GIT_DATE
ENV GIT_HASH $GIT_HASH

ADD verible-$GIT_VERSION.tar.gz /src/verible
WORKDIR /src/verible/verible-$GIT_VERSION
RUN bazel build --workspace_status_command=bazel/build-version.sh -c opt //...
RUN echo $REPO_SLUG
RUN echo $GIT_DATE
RUN echo $GIT_HASH
RUN echo $GIT_VERSION

RUN ./.github/workflows/github-pages-setup.sh
RUN ./.github/workflows/github-releases-setup.sh /out/
EOF
done

# Create archive for each docker directory
for DIR in $DIRS; do
    (cd .. ; git archive --prefix verible-$GIT_VERSION/ --output releasing/$DIR/verible-$GIT_VERSION.tar.gz HEAD)
done
