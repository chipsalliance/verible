#!/usr/bin/env python3
"""
Invoke bazel with --workspace_status_command=bazel/build-version.py to
get this invoked and populate bazel-out/volatile-status.txt
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
