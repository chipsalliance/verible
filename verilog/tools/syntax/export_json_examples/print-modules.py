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
import anytree


def process_file_data(path, data):
    if not data.tree:
        return

    for module in data.tree.find_by_tag("kModuleDeclaration"):
        header = module.find_by_tag("kModuleHeader", max_count=1)
        if not header:
            continue
        name = header.find_by_tag(["SymbolIdentifier", "EscapedIdentifier"], max_count=1, iter=anytree.PreOrderIter)
        if not name:
            continue

        print(f"module \033[1;32m{name.text}\033[0m (in \033[1m{path}\033[0m)")
        for port in header.find_by_tag(["kPortDeclaration", "kPort"]):
            port_name = port.find_by_tag(["SymbolIdentifier", "EscapedIdentifier"], max_count=1)
            start = port.start
            end = port.end
            if start and end:
                name_start = port_name.start
                name_end = port_name.end

                code = data.source_code
                prefix = code[start:name_start].decode("utf8")
                port_name_text = code[name_start:name_end].decode("utf8")
                suffix = code[name_end:end].decode("utf8")
                print(f"    {prefix}\033[32;92m{port_name_text}\033[0m{suffix}")
        print()


def main():
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} PATH_TO_VERIBLE_VERILOG_SYNTAX VERILOG_FILE [VERILOG_FILE [...]]")
        return 1

    parser_path = sys.argv[1]
    files = sys.argv[2:]

    parser = VeribleVerilogSyntax(executable=parser_path)
    data = parser.parse_files(files)

    for file_path, file_data in data.items():
        process_file_data(file_path, file_data)


if __name__ == "__main__":
    exit(main())
