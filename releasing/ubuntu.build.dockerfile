ARG _VERSION_

FROM verible:ubuntu-$_VERSION_-basic

ARG _BAZEL_VERSION_

# Install bazel
RUN wget --no-verbose "https://github.com/bazelbuild/bazel/releases/download/$_BAZEL_VERSION_/bazel_$_BAZEL_VERSION_-linux-x86_64.deb" -O /tmp/bazel.deb; \
    dpkg -i /tmp/bazel.deb || true; \
    apt-get -f install -y
