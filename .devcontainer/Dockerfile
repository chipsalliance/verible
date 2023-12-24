# Available versions: https://github.com/devcontainers/images/tree/main/src/ruby
FROM mcr.microsoft.com/devcontainers/cpp:ubuntu-22.04
RUN apt update && apt install -y clang-format clang-tidy
RUN wget https://github.com/bazelbuild/bazelisk/releases/download/v1.19.0/bazelisk-linux-amd64 -O /usr/local/bin/bazel && chmod a+rx /usr/local/bin/bazel && /usr/local/bin/bazel --version
