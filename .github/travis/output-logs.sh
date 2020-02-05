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

find -L -name \*.log
for F in $(find -L -name \*.log); do
    echo
    export FOLD_NAME=$(echo $F | sed -e's/[^A-Za-z0-9]/_/g')
    travis_fold start $FOLD_NAME
    echo -e "\n\n$F\n--------------"
    cat $F
    echo "--------------"
    travis_fold end $FOLD_NAME
    echo
done
