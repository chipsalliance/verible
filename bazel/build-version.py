#!/usr/bin/env python3
# Copyright 2020-2021 The Verible Authors.
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
"""Invoke bazel with --workspace_status_command=bazel/build-version.py to get this invoked and populate bazel-out/volatile-status.txt
"""

import os

from subprocess import Popen, PIPE


def run(*cmd):
  process = Popen(cmd, stdout=PIPE)
  output, _ = process.communicate()

  return output.strip().decode()


def main():
  try:
    date = run("git", "log", "-n1", "--date=short", "--format=%cd")
  except:
    date = ""

  try:
    version = run("git", "describe")
  except:
    version = ""

  if not date:
    date = os.environ["GIT_DATE"]

  if not version:
    version = os.environ["GIT_VERSION"]

  print("GIT_DATE", '"{}"'.format(date))
  print("GIT_DESCRIBE", '"{}"'.format(version))


if __name__ == "__main__":
  main()
