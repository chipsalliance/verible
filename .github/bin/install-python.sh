#!/usr/bin/env bash
# Copyright 2021 The Verible Authors.
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

# Latest Python version in Xenial is 3.5. `verible_verilog_syntax.py` test
# requires at least 3.6.
echo 'deb http://ppa.launchpad.net/deadsnakes/ppa/ubuntu bionic main' | sudo tee /etc/apt/sources.list.d/deadsnakes.list
sudo apt-key adv --keyserver keyserver.ubuntu.com --recv-keys F23C5A6CF475977595C89F51BA6932366A755776

sudo apt update
sudo apt install -y \
    curl \
    python3.9 libpython3.9-stdlib \
    python3.9-distutils

sudo ln -sf /usr/bin/python3.9 /usr/bin/python3

python3 --version
