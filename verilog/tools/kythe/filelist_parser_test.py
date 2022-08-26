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
"""Tests for filelist_parser."""

import sys
import unittest

import filelist_parser


class FilelistParserTest(unittest.TestCase):

  def test_empty_filelist(self):
    filelist = ""
    result = filelist_parser.ParseFileList(filelist)
    self.assertSequenceEqual(result.files, [])
    self.assertSequenceEqual(result.include_dirs, [])

  def test_complex_filelist(self):
    filelist = """# A comment to ignore.
+incdir+/an/include_dir1
// Another comment
// on two lines
+incdir+/an/include_dir2

/a/source/file/1.sv
/a/source/file/2.sv"""
    result = filelist_parser.ParseFileList(filelist)
    self.assertSequenceEqual(result.files,
                             ["/a/source/file/1.sv", "/a/source/file/2.sv"])
    self.assertSequenceEqual(result.include_dirs,
                             ["/an/include_dir1", "/an/include_dir2"])

  def test_comment_removal(self):
    filelist = """# A comment to ignore.
// Another comment
// on two lines
/a/source/file/1.sv  /* comment after the file */
/a/source/file/2.sv  // another comment after the file
/a/source/file/3.sv  # yet another comment after the file
/* multi line
comment
to remove */
/a/source/file/4.sv"""
    result = filelist_parser.ParseFileList(filelist)
    self.assertSequenceEqual(result.files, [
        "/a/source/file/1.sv", "/a/source/file/2.sv", "/a/source/file/3.sv",
        "/a/source/file/4.sv"
    ])
    self.assertSequenceEqual(result.include_dirs, [])

  def test_plus_minus_args(self):
    filelist = """
+incdir+/an/include_dir
+libdir+/a/lib_dir
+libdir-nocase+/a/lib_dir_nocase
-y /a/lib_dir_short
+libext+/a/lib_ext
+define+a=b
+parameter+c=d
+timescale+a/b
+vhdl-work+/a/vhdl_work
/a/source/file/normal.sv
+toupper-filename
/a/source/file/upper.sv
-v /a/source/file/v_param_upper.sv
-l /a/source/file/l_param_upper.sv
+tolower-filename
/A/SOURCE/FILE/LOWER1.SV
/A/SOURCE/FILE/LOWER2.SV
"""
    result = filelist_parser.ParseFileList(filelist)
    self.assertSequenceEqual(result.files, [
        "/a/source/file/normal.sv", "/A/SOURCE/FILE/UPPER.SV",
        "/A/SOURCE/FILE/V_PARAM_UPPER.SV", "/A/SOURCE/FILE/L_PARAM_UPPER.SV",
        "/a/source/file/lower1.sv", "/a/source/file/lower2.sv"
    ])
    self.assertSequenceEqual(result.include_dirs, ["/an/include_dir"])
    self.assertSequenceEqual(
        result.lib_dirs,
        ["/a/lib_dir", "/a/lib_dir_nocase", "/a/lib_dir_short"])
    self.assertSequenceEqual(result.libext_suffixes, ["/a/lib_ext"])
    self.assertSequenceEqual(result.defines, ["a=b"])
    self.assertSequenceEqual(result.parameters, ["c=d"])
    self.assertSequenceEqual(result.timescales, ["a/b"])
    self.assertSequenceEqual(result.vhdl_work, ["/a/vhdl_work"])


if __name__ == "__main__":
  unittest.main(argv=sys.argv[0:1], verbosity=2)
