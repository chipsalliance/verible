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

set -e

DIRS=$( echo ${1:-$(find -mindepth 1 -maxdepth 1 -type d)} | sed 's#:#-#')
export TAG=${TAG:-$(git describe --match=v*)}

./docker-generate.sh $DIRS

for DIR in $DIRS; do
    IMAGE="verible:$DIR-$TAG"
    echo
    echo "Docker file for $IMAGE"
    echo "--------------------------------"
    cat $DIR/Dockerfile
    echo "--------------------------------"
    echo
    echo "Docker build for $IMAGE"
    echo "--------------------------------"
    docker build --tag $IMAGE $DIR
    echo "--------------------------------"
    echo
    echo "Build extraction for $IMAGE"
    echo "--------------------------------"
    DOCKER_ID=$(docker create $IMAGE)
    docker cp $DOCKER_ID:/out - | tar xvf -
    echo "--------------------------------"
    echo
    echo "Cleanup for $IMAGE"
    echo "--------------------------------"
    docker rm -v $DOCKER_ID
    echo "--------------------------------"
done
