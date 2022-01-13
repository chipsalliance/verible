ARG _VERSION_

FROM ubuntu:$_VERSION_ AS base

ENV DEBIAN_FRONTEND noninteractive
ENV TZ UTC

RUN apt-get update

# Install basic tools
RUN apt-get install -y         \
    curl                       \
    file                       \
    git                        \
    gnupg                      \
    lsb-release                \
    software-properties-common \
    wget

#--

FROM base AS xenial

# Get a newer GCC and flex version
RUN add-apt-repository ppa:ubuntu-toolchain-r/test; apt-get update
RUN apt-get install -y \
    bison              \
    build-essential    \
    flex               \
    g++-9              \
    gcc-9

RUN ln -sf /usr/bin/gcc-9 /usr/bin/gcc
RUN ln -sf /usr/bin/g++-9 /usr/bin/g++

# Link libstdc++ statically
ENV BAZEL_LINKOPTS "-static-libstdc++:-lm"
ENV BAZEL_LINKLIBS "-l%:libstdc++.a"

#--

FROM base

# Install compiler
RUN apt-get install -y \
    bison              \
    build-essential    \
    flex               \
    g++                \
    gcc
