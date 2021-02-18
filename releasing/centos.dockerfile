FROM centos:VERSION

# Install basic tools
RUN yum install -y \
    file           \
    git            \
    redhat-lsb     \
    tar            \
    wget           \
    python3
