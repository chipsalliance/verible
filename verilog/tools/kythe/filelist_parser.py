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
"""Parser for Icarus Verilog filelists (http://iverilog.wikia.com/wiki/Command_File_Format).

The C++ version is verilog::ParseSourceFileList().
"""

import dataclasses
import re

from typing import List
from enum import Enum

from absl import logging


@dataclasses.dataclass
class FileList:
  """Contents of a System Verilog filelist."""
  files: List[str]
  include_dirs: List[str]
  lib_dirs: List[str]
  libext_suffixes: List[str]
  defines: List[str]
  parameters: List[str]
  vhdl_work: List[str]
  timescales: List[str]


class FilenameOperation(Enum):
  """How to modify the filename."""
  NONE = 1
  TO_UPPERCASE = 2
  TO_LOWERCASE = 3


def MatchAndAppend(line: str, prefix: str, output: List[str]) -> bool:
  """Returns true on the prefix match and appends the stripped line."""
  if line.startswith(prefix):
    output.append(line[len(prefix):])
    return True
  return False


def ParseFileList(content: str) -> FileList:
  result = FileList(
      files=[],
      include_dirs=[],
      lib_dirs=[],
      libext_suffixes=[],
      defines=[],
      parameters=[],
      vhdl_work=[],
      timescales=[])

  # Remove multiline comments /* ... */
  multiline_comment_re = re.compile(r"/\*.*?\*/", re.MULTILINE | re.DOTALL)
  content = multiline_comment_re.subn("", content)[0]
  # Remove single line comments
  single_line_comment_re = re.compile(r"(\/\/|#).*")
  content = single_line_comment_re.subn("", content)[0]

  # Prefix and destination
  prefixes_and_destinations = [("+incdir+", result.include_dirs),
                               ("+libdir+", result.lib_dirs),
                               ("+libdir-nocase+", result.lib_dirs),
                               ("-y ", result.lib_dirs),
                               ("+libext+", result.libext_suffixes),
                               ("+define+", result.defines),
                               ("+timescale+", result.timescales),
                               ("+parameter+", result.parameters),
                               ("+vhdl-work+", result.vhdl_work)]

  file_op = FilenameOperation.NONE
  for l in content.split("\n"):
    l = l.strip()
    # Ignore blank lines
    if not l:
      continue

    if l == "+toupper-filename":
      file_op = FilenameOperation.TO_UPPERCASE
      continue
    if l == "+tolower-filename":
      file_op = FilenameOperation.TO_LOWERCASE
      continue

    for prefix, destination in prefixes_and_destinations:
      if MatchAndAppend(l, prefix, destination):
        break
    else:
      # Treat library files as normal files. Just drop the prefix.
      for file_lib_prefix in ["-v ", "-l "]:
        if l.startswith(file_lib_prefix):
          l = l[len(file_lib_prefix):]

      if l.startswith("+"):
        logging.warning("Dropping unsupported plus arg: %s", l)
        continue
      if l.startswith("-"):
        logging.warning("Dropping unsupported minus arg: %s", l)
        continue

      # A regular file
      if file_op == FilenameOperation.TO_LOWERCASE:
        l = l.lower()
      elif file_op == FilenameOperation.TO_UPPERCASE:
        l = l.upper()
      result.files.append(l)
  return result
