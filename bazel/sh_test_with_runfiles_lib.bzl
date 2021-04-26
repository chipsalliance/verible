# -*- Python -*-
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

"""Bazel rule to wrap sh_test with a wrapper loading runfiles library prior to execution
"""

def sh_test_with_runfiles_lib(name, srcs, size, args, data, deps = []):
    """sh_test wrapper that loads bazel's runfiles library before calling the test.

    This is necessary because on Windows, runfiles are not symlinked like on Unix and
    are thus not available from the path returned by $(location Label). The runfiles
    library provide the rlocation function, which converts a runfile path (from $location)
    to the fullpath of the file.

    Args:
        name: sh_test's name
        srcs: sh_test's srcs, must be an array of a single file
        size: sh_test's size
        args: sh_test's args
        data: sh_test's data
        deps: sh_test's deps
    """

    if len(srcs) > 1:
        fail("you must specify exactly one file in 'srcs'")

    # Add the runfiles library to dependencies
    if len(deps) == 0:
        deps = ["@bazel_tools//tools/bash/runfiles"]
    else:
        deps.append("@bazel_tools//tools/bash/runfiles")

    # Replace first arguments with location of the main script to run
    # and add script to run to sh_test's data
    args = ["$(location " + srcs[0] + ")"] + args
    data += srcs

    native.sh_test(
        name = name,
        srcs = ["//bazel:sh_test_with_runfiles_lib.sh"],
        size = size,
        args = args,
        data = data,
        deps = deps,
    )
