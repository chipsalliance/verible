# -*- Python -*-
# Copyright 2017-2026 The Verible Authors.
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

"""A placeholder target for genrule's `toolchains` attribute.

genrule's `toolchains` attribute requires each entry to provide
TemplateVariableInfo (or ToolchainTypeInfo). Used where a platform (e.g.
Windows) needs no real toolchain there, like for using local winflexbison.
"""

def _no_toolchain_impl(ctx):
    return [platform_common.TemplateVariableInfo({})]

no_toolchain = rule(
    implementation = _no_toolchain_impl,
)
