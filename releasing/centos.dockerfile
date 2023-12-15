FROM centos:VERSION

# Install basic tools
RUN yum install -y \
    file           \
    git            \
    redhat-lsb     \
    tar            \
    wget           \
    java-11-openjdk-devel \
    python3
