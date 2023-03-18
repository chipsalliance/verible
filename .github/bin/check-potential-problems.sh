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

# std::string_view has different semantics on Windows compared to gcc or clang
# c++ libraries: the iterators don't return const char*, but a wrapping object.
#
# There are a few assumptions in the code that assumes const char*, however.
#
# So, we need to use absl::string_view that comes with the same implementation
# everywhere.
find . -name "*.h" -o -name "*.cc" | xargs grep -n "std::string_view"
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
find . -name "*.h" | xargs grep -n '#include "common/util/status_macros.h"'
if [ $? -eq 0 ]; then
  echo "::error:: using status_macros.h in a header pollutes global namespace."
  echo
  EXIT_CODE=1
fi

# Don't accidentally use anything from the verilog namespace in the common
# verible namespace to make sure common stays independent.
# Use of that namespace in a comment is ok, or if waived with // NOLINT
find common -name "*.h" -o -name "*.cc" | xargs grep "verilog::" \
  | egrep -v "(//.*verilog::|// NOLINT)"
if [ $? -eq 0 ]; then
  echo "::error:: use of the verilog::-namespace inside common/"
  echo
  EXIT_CODE=1
fi

exit "${EXIT_CODE}"
