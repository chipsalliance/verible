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
"""Produces Kythe KZip from the given SystemVerilog source files."""

import hashlib
import os
import sys
import zipfile

from absl import app
from absl import flags
from absl import logging
from collections.abc import Sequence

from third_party.proto.kythe import analysis_pb2
from verilog.tools.kythe import filelist_parser

flags.DEFINE_string(
    "filelist_path", "",
    ("The path to the filelist which contains the names of System Verilog "
     "files. The files should be ordered by definition dependencies."))
flags.DEFINE_string(
    "filelist_root", "",
    ("The absolute location which we prepend to the files in the filelist "
     "(where listed files are relative to)."))
flags.DEFINE_string("code_revision", "",
                    "Version control revision at which this code was taken.")
flags.DEFINE_string("corpus", "",
                    "Corpus (e.g., the project) to which this code belongs.")
flags.DEFINE_string("output_path", "", "Path where to write the kzip.")

FLAGS = flags.FLAGS


def PrintUsage(binary_name: str):
  print(
      """usage: {binary_name} [options] --filelist_path FILE --filelist_root FILE --output_path FILE
Produces Kythe KZip from the given SystemVerilog source files.

Input: A file which lists paths to the SystemVerilog top-level translation
       unit files (one per line; the path is relative to the location of the
       filelist).
Output: Produces Kythe KZip (https://kythe.io/docs/kythe-kzip.html).""".format(
    binary_name=binary_name))


def Sha256(content: bytes) -> str:
  """Returns SHA256 of the content as HEX string."""
  m = hashlib.sha256()
  m.update(content)
  return m.hexdigest()


def main(argv: Sequence[str]) -> None:
  if not FLAGS.filelist_path:
    PrintUsage(argv[0])
    raise app.UsageError("No --filelist_path was specified.")
  if not FLAGS.filelist_root:
    PrintUsage(argv[0])
    raise app.UsageError("No --filelist_root was specified.")
  if not FLAGS.output_path:
    PrintUsage(argv[0])
    raise app.UsageError("No --output_path was specified.")

  # open the filelist and parse it
  # collect the file paths relative to the root

  # indexed compilation unit
  compilation = analysis_pb2.IndexedCompilation()
  if FLAGS.code_revision:
    compilation.index.revisions.append(FLAGS.code_revision)
  unit = compilation.unit
  unit.v_name.corpus = FLAGS.corpus
  unit.v_name.root = FLAGS.filelist_root
  unit.v_name.language = "verilog"
  # The filelist path is arbitrary. We simplify it to always be "./filelist"
  unit.argument.append("--f=filelist")

  # Load the filelist
  with open(FLAGS.filelist_path, "rb") as f:
    filelist_content = f.read()
    filelist = filelist_parser.ParseFileList(
        filelist_content.decode(encoding=sys.getdefaultencoding()))

  # unit.required_input
  # input.info.path & v_name (path and root)

  # Zip files, populate required_input and zip the unit
  with zipfile.ZipFile(FLAGS.output_path, mode="w") as zf:
    # Add the filelist to the files
    digest = Sha256(filelist_content)
    zf.writestr(os.path.join("root", "files", digest), filelist_content)
    req_input = unit.required_input.add()
    req_input.info.path = "filelist"
    req_input.info.digest = digest

    # Add all files
    for file_path in filelist.files:
      # The filelist references the files relative to the filelist location.
      path_prefix = os.path.dirname(FLAGS.filelist_path)
      with open(os.path.join(path_prefix, file_path), "rb") as f:
        content = f.read()
        digest = Sha256(content)
        zf.writestr(os.path.join("root", "files", digest), content)
        req_input = unit.required_input.add()
        req_input.info.path = file_path
        req_input.info.digest = digest
        req_input.v_name.path = file_path
        req_input.v_name.root = FLAGS.filelist_root

    # Add the compilation unit
    serialized_unit = compilation.SerializeToString()
    zf.writestr(
        os.path.join("root", "pbunits", Sha256(serialized_unit)),
        serialized_unit)


if __name__ == "__main__":
  app.run(main)
