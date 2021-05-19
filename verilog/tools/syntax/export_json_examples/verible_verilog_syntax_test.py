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
"""VeribleVerilogSyntax test"""


import os
import stat
import sys
import tempfile
import unittest
import platform

import verible_verilog_syntax


class VeribleVerilogSyntaxTest(unittest.TestCase):
  def __init__(self, *args, **kwargs):
    super().__init__(*args, **kwargs)
    self.tmp = None

  def setUp(self):
    if len(sys.argv) > 1:
      self.parser = verible_verilog_syntax.VeribleVerilogSyntax(
                                              executable=sys.argv[1])
    else:
      self.parser = verible_verilog_syntax.VeribleVerilogSyntax()

    self.assertIsNotNone(self.parser)

  def tearDown(self):
    if self.tmp is not None:
      os.unlink(self.tmp.name)
    self.tmp = None

  def create_temp_file(self, content):
    # On Windows, a file can't be opened twice, so use delete=False and
    # logic to cleanup in teardown
    self.tmp = tempfile.NamedTemporaryFile(mode="w", suffix=".sv", delete=False)
    self.tmp.write(content)
    self.tmp.close()
    return self.tmp


class TestParseMethods(VeribleVerilogSyntaxTest):
  def test_parse_string(self):
    data = self.parser.parse_string("module X(); endmodule;")
    self.assertIsNotNone(data)
    self.assertIsNotNone(data.tree)
    self.assertIsNone(data.errors)

  def test_parse_string_with_all_options(self):
    data = self.parser.parse_string("module X(); endmodule;", options={
      "gen_tree": True,
      "gen_tokens": True,
      "gen_rawtokens": True,
    })
    self.assertIsNotNone(data)
    self.assertIsNotNone(data.tree)
    self.assertIsNotNone(data.tokens)
    self.assertIsNotNone(data.rawtokens)
    self.assertIsNone(data.errors)

  def test_parse_string_error(self):
    data = self.parser.parse_string("endmodule X(); module;")
    self.assertIsNotNone(data)
    self.assertIsNotNone(data.errors)

  def test_parse_file(self):
    file_ = self.create_temp_file("module X(); endmodule;")
    data = self.parser.parse_file(file_.name)
    self.assertIsNotNone(data)
    self.assertIsNotNone(data.tree)
    self.assertIsNone(data.errors)

  def test_parse_file_with_all_options(self):
    file_ = self.create_temp_file("module X(); endmodule;")
    data = self.parser.parse_file(file_.name, options={
      "gen_tree": True,
      "gen_tokens": True,
      "gen_rawtokens": True,
    })
    self.assertIsNotNone(data)
    self.assertIsNotNone(data.tree)
    self.assertIsNotNone(data.tokens)
    self.assertIsNotNone(data.rawtokens)
    self.assertIsNone(data.errors)

  def test_parse_file_error(self):
    file_ = self.create_temp_file("endmodule X(); module;")
    data = self.parser.parse_file(file_.name)
    self.assertIsNotNone(data)
    self.assertIsNotNone(data.errors)

  def test_parse_files(self):
    files = [
      self.create_temp_file("module X(); endmodule;"),
      self.create_temp_file("module Y(); endmodule;"),
    ]
    data = self.parser.parse_files([f.name for f in files])
    self.assertIsNotNone(data)
    for f in files:
      self.assertIsNotNone(data[f.name])
      self.assertIsNotNone(data[f.name].tree)
      self.assertIsNone(data[f.name].errors)

  def test_parse_files_with_all_options(self):
    files = [
      self.create_temp_file("module X(); endmodule;"),
      self.create_temp_file("module Y(); endmodule;"),
    ]
    data = self.parser.parse_files([f.name for f in files], options={
      "gen_tree": True,
      "gen_tokens": True,
      "gen_rawtokens": True,
    })
    self.assertIsNotNone(data)
    for f in files:
      self.assertIsNotNone(data[f.name])
      self.assertIsNotNone(data[f.name].tree)
      self.assertIsNotNone(data[f.name].tokens)
      self.assertIsNotNone(data[f.name].rawtokens)
      self.assertIsNone(data[f.name].errors)

  def test_parse_files_error(self):
    # One file with, and one without errors
    files = [
      self.create_temp_file("endmodule X(); module;"),
      self.create_temp_file("module Y(); endmodule;"),
    ]
    data = self.parser.parse_files([f.name for f in files])
    self.assertIsNotNone(data)
    for f in files:
      self.assertIsNotNone(data[f.name])

    self.assertIsNotNone(data[files[0].name].errors)
    self.assertIsNone(data[files[1].name].errors)


class TestTree(VeribleVerilogSyntaxTest):
  def setUp(self):
    super().setUp()
    data = self.parser.parse_string(
        "module ModuleName#(parameter PARAM_NAME=42)\n" +
        "    (input portIn, output portOut);\n" +
        "  import import_pkg_name::*;\n" +
        "  wire wireName;\n" +
        "endmodule;\n" +
        "module OtherModule; endmodule;\n")
    self.assertIsNotNone(data)
    self.assertIsNotNone(data.tree)
    self.tree = data.tree

  def test_find(self):
    header = self.tree.find({"tag": "kModuleHeader"})
    self.assertIsNotNone(header)
    module_name = header.find({"tag": "SymbolIdentifier"})
    self.assertIsNotNone(module_name)
    self.assertEqual(module_name.text, "ModuleName")
    nonexistent = header.find({"tag": "SomeUndefinedTag"})
    self.assertIsNone(nonexistent)

  def test_find_all(self):
    headers = self.tree.find_all({"tag": "kModuleHeader"})
    self.assertEqual(len(headers), 2)

    identifiers = self.tree.find_all({"tag": "SymbolIdentifier"})
    self.assertEqual(len(identifiers), 7)

    some_identifiers = self.tree.find_all({"tag": "SymbolIdentifier"},
                                          max_count=4)
    self.assertEqual(len(some_identifiers), 4)

  def test_iter_find_all(self):
    identifiers = [n.text
                   for n
                   in self.tree.iter_find_all({"tag": "SymbolIdentifier"})]
    self.assertIn("ModuleName", identifiers)
    self.assertIn("PARAM_NAME", identifiers)
    self.assertIn("OtherModule", identifiers)

  def test_custom_filter(self):
    def tokens_past_135_byte(node):
      return (isinstance(node, verible_verilog_syntax.TokenNode)
              and node.start > 135)

    other_module_tokens = self.tree.find_all(tokens_past_135_byte)
    self.assertGreaterEqual(len(other_module_tokens), 5)
    for token in other_module_tokens:
      self.assertGreater(token.start, 135)

  def test_search_order(self):
    level_order = self.tree.find_all({"tag": "SymbolIdentifier"})
    depth_order = self.tree.find_all({"tag": "SymbolIdentifier"},
                              iter_=verible_verilog_syntax.PreOrderTreeIterator)

    def check_items_order(iterable, items_to_check):
      index = 0
      for item in iterable:
        if items_to_check[index] == item:
          index += 1
        if index == len(items_to_check):
          return True
      return False

    self.assertTrue(check_items_order([n.text for n in level_order],
                                      ["ModuleName", "OtherModule", "portIn"]))
    self.assertTrue(check_items_order([n.text for n in depth_order],
                                      ["ModuleName", "portIn", "OtherModule"]))

  def test_node_properties(self):
    header = self.tree.find({"tag": "kModuleHeader"})
    self.assertIsNotNone(header)

    module_kw = header.find({"tag": "module"})
    self.assertEqual(module_kw.text, "module")
    self.assertEqual(module_kw.start, 0)
    self.assertEqual(module_kw.end, 6)

    semicolon = header.find({"tag": ";"})
    self.assertEqual(semicolon.text, ";")
    self.assertEqual(semicolon.start, 78)
    self.assertEqual(semicolon.end, 79)

    self.assertEqual(header.start, module_kw.start)
    self.assertEqual(header.end, semicolon.end)
    self.assertTrue(header.text.startswith("module"))
    self.assertTrue(header.text.endswith(");"))


class TestTokens(VeribleVerilogSyntaxTest):
  def test_tokens(self):
    data = self.parser.parse_string(
          "module X(input portIn, output portOut); endmodule;", options={
      "gen_tree": False,
      "gen_tokens": True,
    })

    self.assertIsNotNone(data)
    self.assertIsNotNone(data.tokens)

    identifiers = [t for t in data.tokens if t.tag == "SymbolIdentifier"]

    module_name = identifiers[0]
    self.assertEqual(module_name.text, "X")
    self.assertEqual(module_name.start, 7)
    self.assertEqual(module_name.end, 8)

    texts = [t.text for t in identifiers]
    self.assertSequenceEqual(texts, ["X", "portIn", "portOut"])


  def test_rawtokens(self):
    data = self.parser.parse_string(
          "module X(input portIn, output portOut); endmodule;", options={
      "gen_tree": False,
      "gen_rawtokens": True,
    })

    self.assertIsNotNone(data)
    self.assertIsNotNone(data.rawtokens)

    identifiers = [t for t in data.rawtokens if t.tag == "SymbolIdentifier"]

    module_name = identifiers[0]
    self.assertEqual(module_name.text, "X")
    self.assertEqual(module_name.start, 7)
    self.assertEqual(module_name.end, 8)

    texts = [t.text for t in identifiers]
    self.assertSequenceEqual(texts, ["X", "portIn", "portOut"])


class TestVersionCheck(unittest.TestCase):
  def setUp(self):
    tmp_file = tempfile.NamedTemporaryFile(mode="w",
        suffix="_verible_verilog_syntax.py", delete=False)
    tmp_file.close()

    self.executable_file = os.path.abspath(tmp_file.name)
    os.chmod(self.executable_file, stat.S_IRUSR | stat.S_IWUSR | stat.S_IXUSR)

    if platform.system() == "Windows":
      self.executable_cmd = [sys.executable, self.executable_file]
    else:
      self.executable_cmd = self.executable_file

  def tearDown(self):
    os.unlink(self.executable_file)

  def test_version_getter(self):
    parser = verible_verilog_syntax.VeribleVerilogSyntax(
        executable=sys.argv[1])

    import subprocess
    v = subprocess.run([sys.argv[1], "--version"], capture_output=True, encoding="UTF_8")

    assert parser.version.startswith("v")

  def test_version_check(self):
    py_code_template = "\n".join((
      r"#!/usr/bin/env python3",
      r"import sys",
      r"if sys.argv[1] == '--version':",
      r"  print({version!r})",
      r"  exit(0)",
      r"elif sys.argv[1] == '--helpfull':",
      r"  print({help!r})",
      r"  exit(1)",
      "",
    ))
    compatible_help = "\n".join((
      r"  Flags from verilog/tools/syntax/verilog_syntax.cc:",
      r"    --error_limit (Limit the number of syntax errors reported.",
      r"      (0: unlimited)); default: 0;",
      r"    --export_json (Uses JSON for output. Intended to be used as an",
      r"      input for other tools.); default: false;",
      r"    --printrawtokens (Prints all lexed tokens, including filtered",
      r"      ones.); default: false;",
      r"    --printtokens (Prints all lexed and filtered tokens);",
      r"      default: false;",
      r"    --printtree (Whether or not to print the tree); default: false;",
    ))
    incompatible_help = "\n".join((
      r"  Flags from verilog/tools/syntax/verilog_syntax.cc:",
      r"    --error_limit (Limit the number of syntax errors reported.",
      r"      (0: unlimited)); default: 0;",
      r"    --printrawtokens (Prints all lexed tokens, including filtered",
      r"      ones.); default: false;",
      r"    --printtokens (Prints all lexed and filtered tokens);",
      r"      default: false;",
      r"    --printtree (Whether or not to print the tree); default: false;",
    ))
    # minimum version = "v0.0-925-gc1a388a"

    # Compatible version
    built_commit_lines = ("Commit\t2021-05-19", "Built\t2021-05-19T20:13:51Z")
    versions = (
      "v0.0-925-gc1a388a",
      "v0.0-925-g000",
      "v0.0-1000-g1234567",
      "v0.1-0-gabcdef",
      "v0.1-900-gabcdef",
      "v1.0-0-gabcdef",
      "v2.0-80-gabcdef",
    )
    for version in versions:
      with open(self.executable_file, "w") as f:
        f.write(py_code_template.format(
            version="\n".join([version, *built_commit_lines]),
            help=compatible_help))

      parser = verible_verilog_syntax.VeribleVerilogSyntax(
          executable=self.executable_cmd)
      assert parser.version == version

    # Compatible version without correct `--version` message
    version_msgs = (
      "",
      "Built\t2021-05-19T20:13:51Z",
      "v0.0-1000",
      "0.0-1000",
    )
    for version_msg in version_msgs:
      with open(self.executable_file, "w") as f:
        f.write(py_code_template.format(version=version_msg,
            help=compatible_help))

      verible_verilog_syntax.VeribleVerilogSyntax(
          executable=self.executable_cmd)

    # Incompatible version
    versions = (
      "v0.0-924-gc1a388a",
      "v0.0-924-g000",
      "v0.0-93-gffffff",
      "v0.0-9-gabcdef",
      "v0.0-0-gabcdef",
    )
    for version in versions:
      with open(self.executable_file, "w") as f:
        f.write(py_code_template.format(
            version="\n".join([version, *built_commit_lines]),
            help=incompatible_help))

      with self.assertRaises(Exception):
        verible_verilog_syntax.VeribleVerilogSyntax(
            executable=self.executable_cmd)

    # Incompatible version without correct `--version` message
    version_msgs = (
      "",
      "Built\t2021-05-19T20:13:51Z",
      "v0.0-100",
      "0.0-100",
    )
    for version_msg in version_msgs:
      with open(self.executable_file, "w") as f:
        f.write(py_code_template.format(version=version_msg,
            help=incompatible_help))

      with self.assertRaises(Exception):
        verible_verilog_syntax.VeribleVerilogSyntax(
            executable=self.executable_cmd)


if __name__ == "__main__":
  unittest.main(argv=sys.argv[0:1], verbosity=2)
