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

def jcxxgen(name, src, out, namespace = ""):
    """Generate C/C++ language source from a jcxxgen schema file.
    """
    tool = "//common/lsp:jcxxgen"
    json_header = '"nlohmann/json.hpp"'

    native.genrule(
        name = name + "_gen",
        srcs = [src],
        outs = [out],
        cmd = '$(location jcxxgen) --json_header=\'' + json_header + '\' --class_namespace ' + namespace + ' --output $@ $<',
        tools = [tool],
    )
    native.cc_library(
        name = name,
        srcs = [out],
        deps = ["@jsonhpp//:jsonhpp"],
    )
