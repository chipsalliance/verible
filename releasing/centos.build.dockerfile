ARG _VERSION_

FROM verible:centos-$_VERSION_-basic AS base

# Install bazel
RUN yum install -y --nogpgcheck \
    java-1.8.0-openjdk-devel \
    unzip \
    zip

RUN java -version
RUN javac -version

#--

FROM base

ARG _VERSION_

ADD "https://copr.fedorainfracloud.org/coprs/vbatts/bazel/repo/epel-$_VERSION_/vbatts-bazel-epel-$_VERSION_.repo" /etc/yum.repos.d
RUN yum install -y --nogpgcheck bazel3

RUN bazel --version
