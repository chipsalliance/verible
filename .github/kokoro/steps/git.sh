#!/bin/bash

set -e

echo
echo "========================================"
echo "Git log"
echo "----------------------------------------"
git log -n5 --stat
echo "----------------------------------------"

echo
echo "========================================"
echo "Git fetching tags"
echo "----------------------------------------"
# Don't fail if there are no tags
git fetch --tags || true
echo "----------------------------------------"

echo
echo "========================================"
echo "Git version info"
echo "----------------------------------------"
git log -n1
echo "----------------------------------------"
git describe --tags || true
echo "----------------------------------------"
git describe --tags --always || true
echo "----------------------------------------"
