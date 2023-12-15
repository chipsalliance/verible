FROM ubuntu:VERSION

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
    wget                       \
    openjdk-11-jdk             \
    python3
