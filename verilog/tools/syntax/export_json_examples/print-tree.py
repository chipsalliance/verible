#!/usr/bin/env python3
# Copyright 2017-2020 The Verible Authors.
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

import sys
from verible_verilog_syntax import VeribleVerilogSyntax
from anytree import RenderTree


def process_file_data(path, data):
    print(f"\033[1;7;92mFILE: {path} \033[0m")
    if data.tree:
        for prefix, _, node in RenderTree(data.tree):
            print(f"\033[90m{prefix}\033[0m{node.to_formatted_string()}")


def main():
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} PATH_TO_VERIBLE_VERILOG_SYNTAX VERILOG_FILE [VERILOG_FILE [...]]")
        return 1

    parser_path = sys.argv[1]
    files = sys.argv[2:]

    parser = VeribleVerilogSyntax(executable=parser_path)
    data = parser.parse_files(files, options={"gen_tree": True})

    for file_path, file_data in data.items():
        process_file_data(file_path, file_data)


if __name__ == "__main__":
    exit(main())
