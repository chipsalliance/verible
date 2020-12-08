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

set -x
set -e
export TAG=${TAG:-$(git describe --match=v*)}

git config --local user.name "Deployment Bot"
git config --local user.email "verible-dev@googlegroups.com"

case $MODE in
test|clean)
    # Nothing to do
    ;;

compile)
    # Set up things for GitHub Pages deployment
    ./.github/workflows/github-pages-setup.sh
    ;;

bin)
    # Create a tag of form v0.0-183-gdf2b162-20191112132344
    rm -rf releasing/out
    git tag "$TAG" || true
    echo "TAG=$TAG" >> $GITHUB_ENV
    ls -l /tmp/releases
    ;;

*)
    echo "success.sh: Unknown mode $MODE"
    exit 1
    ;;
esac
