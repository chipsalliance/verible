# Copyright 2025 The Verible Authors.
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

"""Provide a helper function to extract version number from MODULE.bazel"""

def get_version_define_from_module(name = ""):
    module_version = native.module_version()
    if module_version:
        return ['-DVERIBLE_MODULE_VERSION=\\"{0}\\"'.format(module_version)]
    else:
        return []
