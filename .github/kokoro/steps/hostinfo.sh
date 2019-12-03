#!/bin/bash

set -e

echo
echo "========================================"
echo "Host Environment"
echo "----------------------------------------"
export
echo "----------------------------------------"

echo
echo "========================================"
echo "Host CPU"
echo "----------------------------------------"
export CORES=$(nproc --all)
echo "Cores: $CORES"
echo
echo "Memory"
echo "----------------------------------------"
cat /proc/meminfo
echo "----------------------------------------"
export MEM_GB=$(($(awk '/MemTotal/ {print $2}' /proc/meminfo)/(1024*1024)))
echo "Memory (GB): $CORES"
export MEM_CORES=$(($MEM_GB/4))

echo
echo "========================================"
echo "Package version"
echo "----------------------------------------"
set -x
gcc --version
set +x
echo "----------------------------------------"
