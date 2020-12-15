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

import subprocess
import json
from anytree import NodeMixin, LevelOrderIter, PostOrderIter
import re

from dataclasses import dataclass
from typing import Optional, List


_CSI_SEQUENCE = re.compile("\033\\[.*?m")

def _colorize(formats, strings):
    result = ""
    fi = 0
    for s in strings:
        result += f"\033[{formats[fi]}m{s}\033[0m"
        fi = (fi+1) % len(formats)
    return result


class ReversePostOrderIter(PostOrderIter):
    @staticmethod
    def _get_children(children, stop):
        return super()._get_children(reversed(children), stop)


class Node(NodeMixin):
    def __init__(self, parent=None):
        self.parent = parent

    @property
    def syntax_data(self):
        return self.parent.syntax_data if self.parent else None

    @property
    def start(self):
        raise NotImplementedError("Subclass must implement 'start' property")

    @property
    def end(self):
        raise NotImplementedError("Subclass must implement 'start' property")

    @property
    def text(self):
        start = self.start
        end = self.end
        sd = self.syntax_data
        if start and end and sd and sd.source_code and end <= len(sd.source_code):
            return sd.source_code[start:end].decode("utf-8")
        return ""

    def __repr__(self):
        return _CSI_SEQUENCE.sub("", self.to_formatted_string())

    def to_formatted_string(self):
        return super().__repr__()


class BranchNode(Node):
    def __init__(self, tag, parent=None, children=None):
        super().__init__(parent)
        self.tag = tag
        self.children = children if children is not None else []

    @property
    def start(self):
        first_token = self.find(lambda n: isinstance(n, TokenNode), max_count=1, iter=PostOrderIter)
        return first_token.start if first_token else None

    @property
    def end(self):
        last_token = self.find(lambda n: isinstance(n, TokenNode), max_count=1, iter=ReversePostOrderIter)
        return last_token.end if last_token else None

    def find(self, filter_, max_count=0, iter=LevelOrderIter):
        found = []
        for item in iter(self, filter_):
            found.append(item)
            if max_count > 0 and len(found) >= max_count:
                break
        if max_count == 1:
            return found[0] if len(found) == 1 else None
        return found

    def find_by_tag(self, tags, max_count=0, iter=LevelOrderIter):
        if not isinstance(tags, list):
            tags = [tags]
        return self.find(lambda n: (hasattr(n, "tag") and n.tag in tags),
                max_count, iter)

    def to_formatted_string(self):
        tag = self.tag if self.tag == repr(self.tag)[1:-1] else repr(self.tag)
        return _colorize(["37", "1;97"], ["[", tag, "]"])


class RootNode(BranchNode):
    def __init__(self, tag, syntax_data=None, children=None):
        super().__init__(tag, None, children)
        self._syntax_data = syntax_data

    @property
    def syntax_data(self):
        return self._syntax_data


class LeafNode(Node):
    def __init__(self, parent=None):
        super().__init__(parent)

    @property
    def start(self):
        return 0

    @property
    def end(self):
        return 0

    def to_formatted_string(self):
        return _colorize(["90"], ["null"])


class TokenNode(LeafNode):
    def __init__(self, tag, start, end, parent=None):
        super().__init__(parent)
        self.tag = tag
        self._start = start
        self._end = end

    @property
    def start(self):
        return self._start

    @property
    def end(self):
        return self._end

    def to_formatted_string(self):
        tag = self.tag if self.tag == repr(self.tag)[1:-1] else repr(self.tag)
        parts = [
            _colorize(["37", "1;97"], ["[", tag, "]"]),
            _colorize(["33", "93"], ["@(", self.start, "-", self.end, ")"]),
        ]
        text = self.text
        if self.tag != text:
            parts.append(_colorize(["32", "92"], ["'", repr(text)[1:-1], "'"]))
        return " ".join(parts)


class Token:
    def __init__(self, tag, start, end, syntax_data=None):
        self.tag = tag
        self.start = start
        self.end = end
        self.syntax_data = syntax_data

    @property
    def text(self):
        sd = self.syntax_data
        if sd and sd.source_code and self.end <= len(sd.source_code):
            return sd.source_code[self.start:self.end].decode("utf-8")
        return ""

    def __repr__(self):
        return _CSI_SEQUENCE.sub("", self.to_formatted_string())

    def to_formatted_string(self):
        tag = self.tag if self.tag == repr(self.tag)[1:-1] else repr(self.tag)
        parts = [
            _colorize(["37", "1;97"], ["[", tag, "]"]),
            _colorize(["33", "93"], ["@(", self.start, "-", self.end, ")"]),
            _colorize(["32", "92"], ["'", repr(self.text)[1:-1], "'"]),
        ]
        return " ".join(parts)


@dataclass
class Error:
    line: int
    column: int
    phase: str
    message: str = ""


@dataclass
class SyntaxData:
    source_code: Optional[str] = None
    tree: Optional[RootNode] = None
    tokens: Optional[List[Token]] = None
    rawtokens: Optional[List[Token]] = None
    errors: Optional[List[Error]] = None


class VeribleVerilogSyntax:
    def __init__(self, executable="verible-verilog-syntax"):
        self.executable = executable

    @staticmethod
    def _transform_tree(tree, data, skip_null):
        def transform(tree):
            if tree is None:
                return None
            if "children" in tree:
                children = [
                    transform(child) or LeafNode()
                        for child in tree["children"]
                        if (not skip_null or child is not None)
                ]
                tag = tree["tag"]
                return BranchNode(tag, children=children)
            tag = tree["tag"]
            start = tree["start"]
            end = tree["end"]
            return TokenNode(tag, start, end)

        if "children" not in tree:
            return None

        children = [transform(child) for child in tree["children"]]
        tag = tree["tag"]
        return RootNode(tag, syntax_data=data, children=children)


    @staticmethod
    def _transform_tokens(tokens, data):
        return [Token(t["tag"], t["start"], t["end"], data) for t in tokens]


    @staticmethod
    def _transform_errors(tokens):
        return [Error(t["line"], t["column"], t["phase"], t.get("message", None))
                for t in tokens]

    def _parse(self, paths, input=None, options={}):
        options = {
            "gen_tree": True,
            "skip_null": False,
            "gen_tokens": False,
            "gen_rawtokens": False,
            **options,
        }

        args = ["-export_json"]
        if options["gen_tree"]:
            args.append("-printtree")
        if options["gen_tokens"]:
            args.append("-printtokens")
        if options["gen_rawtokens"]:
            args.append("-printrawtokens")

        proc = subprocess.run([self.executable, *args , *paths],
                stdout=subprocess.PIPE,
                input=input,
                encoding="utf-8")

        json_data = json.loads(proc.stdout)
        data = {}
        for file_path, file_json in json_data.items():
            file_data = SyntaxData()

            if file_path == "-":
                file_data.source_code = input
            else:
                with open(file_path, "rb") as f:
                    file_data.source_code = f.read()

            if "tree" in file_json:
                file_data.tree = VeribleVerilogSyntax._transform_tree(
                        file_json["tree"], file_data, options["skip_null"])

            if "tokens" in file_json:
                file_data.tokens = VeribleVerilogSyntax._transform_tokens(
                        file_json["tokens"], file_data)

            if "rawtokens" in file_json:
                file_data.rawtokens = VeribleVerilogSyntax._transform_tokens(
                        file_json["rawtokens"], file_data)

            if "errors" in file_json:
                file_data.errors = VeribleVerilogSyntax._transform_errors(file_json["errors"])

            data[file_path] = file_data

        return data

    def parse_files(self, paths, options={}):
        return self._parse(paths, options=options)

    def parse_file(self, path, options={}):
        return self._parse([path], options=options)

    def parse_string(self, string, options={}):
        return self._parse(["-"], input=string, options=options)
