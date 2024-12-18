#!/usr/bin/env bash
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

##
# Test for things that we don't want to introduce accidentally.
##

EXIT_CODE=0

# std::string_view::iterator is a const char* in gcc, clang, and absl C++ libs.
# This resulted into the assumption that it is in many places in this code base.
#
# On Windows the iterator is a wrapping object, so breaking that assumption.
#
# So, until these assumptions are fixed, we need to use absl::string_view that
# comes with the same implementation everywhere.
find verible -name "*.h" -o -name "*.cc" | \
  xargs grep -n "std::string_view"
if [ $? -eq 0 ]; then
  echo "::error:: use absl::string_view instead of std::string_view"
  echo
  EXIT_CODE=1
fi

# The status macros pollute the global namespace with names that might
# clash in other environments that include these headers and have that name
# defined differently.
#
# So let's make sure to only use them in our compilation units, not headers.
# (Not a super-strong potential problem, so we might consider removing
#  this test, but if we need RETURN_IF_ERROR() in a header file, maybe it
#  is a good idea to move an implementation to a *.cc file anyway)
#
# TODO(hzeller): Arguably this might be good for common/util/logging.h as well.
find . -name "*.h" | xargs grep -n '#include "verible/common/util/status_macros.h"'
if [ $? -eq 0 ]; then
  echo "::error:: using status_macros.h in a header pollutes global namespace."
  echo
  EXIT_CODE=1
fi

# Don't accidentally use anything from the verilog namespace in the common
# verible namespace to make sure common stays independent.
# Use of that namespace in a comment is ok, or if waived with // NOLINT
find verible/common -name "*.h" -o -name "*.cc" | xargs grep "verilog::" \
  | egrep -v "(//.*verilog::|// NOLINT)"
if [ $? -eq 0 ]; then
  echo "::error:: use of the verilog::-namespace inside common/"
  echo
  EXIT_CODE=1
fi

# Always use fully qualified include paths.
find verible -name "*.h" -o -name "*.cc" | \
  xargs egrep -n '#include "[^/]*"'
if [ $? -eq 0 ]; then
  echo "::error:: always use a fully qualified name for #includes"
  echo
  EXIT_CODE=1
fi

find verible -name "*.h" -o -name "*.cc" | grep _ | grep -v _test
if [ $? -eq 0 ]; then
  echo "::error:: File naming-convention for c++ files is to use dashes as separator with underscore only in test files; e.g. foo-bar_test.cc"
  echo
  EXIT_CODE=1
fi

find verible -name "*-test.cc" -o -name "*-test.sh" | grep test
if [ $? -eq 0 ]; then
  echo "::error:: File naming-convention for tests is to end with _test; e.g. foo-bar.cc has test foo-bar_test.cc; similar with shell-script tests"
  echo
  EXIT_CODE=1
fi

# bazelbuild/rules_python is broken as it downloads a dynamically
# linked pre-built binary - This makes it _very_ platform specific.
# This should either compile Python from scratch or use the local system Python.
# So before rules_python() is added here, this needs to be fixed first upstream.
# https://github.com/bazelbuild/rules_python/issues/1211
grep rules_python MODULE.bazel
if [ $? -eq 0 ]; then
  echo "::error:: rules_python() breaks platform independence with shared libs."
  echo
  EXIT_CODE=1
fi

# Never use std::regex.
find verible -name "*.h" -o -name "*.cc" | \
  xargs grep -n '#include <regex>'
if [ $? -eq 0 ]; then
  echo "::error:: Don't use stdlib regex, it is slow and requires exceptions. Use RE2 instead (https://github.com/google/re2; header #include \"re2/re2.h\")."
  echo
  EXIT_CODE=1
fi

if [ -e .bazelversion ]; then
  echo "Don't use .bazelversion. It is a poorly implemented bazel feature that does not support semantic versioning. Instead, make the repo work with all currently active bazel versions."
  EXIT_CODE=1
fi

exit "${EXIT_CODE}"
