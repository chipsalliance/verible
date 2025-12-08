#!/usr/bin/env python3

# Copyright 2023 The Verible Authors.
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

###
# Analysis tool for the generated error logs from smoke-test-error-logger.sh
# Arguments that need to be supplied:
#   - path: path to the directory where all the *-nonzeros directories are
# This script checks various conditions that classify each error based on
# criteria and previous errors, such that a clear picture is generated
# in the end of what is the main cause of non-zero exits in the smoke tests.

import glob
import tempfile
import subprocess
import re
from collections import defaultdict
from copy import deepcopy
import argparse
from enum import Enum
import os
from mdutils.mdutils import MdUtils

CATEGORY_DESCRIPTIONS = """
Each return code different than 0 in the smoke test triggered saving the stderr
of that particular run to a file, and each line of those files
(except project tool)
contained a single error. This tool analyzes the error logs and classifies each
error into a category to visualize what is the main cause of issues still
present in the smoke tests.

Error categories:
 - `undefined`: Errors that do not fit any criteria
 - `slang-verified-error`: Errors that also are present in slang, verifying
the legitimacy of them
 - `related-to-slang-validated-error`: Errors that occured after a slang error;
if there was a syntax error, a lot of later tokens will not fit
causing additional errors.
- `macro-call-in-module-params`: Errors that occured because of a macro call
in a module parameter list. Those usually also contain the line delimiters
of which the parser will be unaware causing syntax errors.
 - `caused-by-macro-call-in-module-params`: Syntax errors that occur after a
missing define. Analogous to `related-to-slang-validated-error`,
those are syntax errors caused by earlier "missing" tokens.
 - `unresolved-macro`: Errors that occured because of an unresolved macro call
 - `test-designed-to-fail`: Errors that are intentional, present in e.g. ivtest
 - `misc-preprocessor`: Errors that are not related to the above categories
but are related to preprocessor keywords
 - `misc-preprocessor-related`: Other errors that may have appeared because
there was a preprocessor problem earlier.
eg. a syntax error after an unresolved macro
 - `standalone-header`: Errors that occured while parsing a header file, that
really should not be parsed outside of its context where
it is included
 - `hit-preprocessor-failsafe`: During parsing a preprocessor-based
`ifdef/elseif/else` decision tree a branch was selected that fails on
purpose (present in rsd)
 - `likely-unhandled-macro-call`: Above the syntax error there was a macro call
that likely contained additional needed tokens that are not present now
and the syntax is technically invalid.
- `related-to-likely-unhandled-macro-call`: Syntax errors that occur after an
unhandled macro was present near a syntax error
"""

# Externally used binaries
SLANG="slang"
RIPGREP="rg"

SLANG_DEBUG_OUT = False

parser = argparse.ArgumentParser()
parser.add_argument("path")
parser.add_argument(
        "--verible-path",
        type=str,
        required=False,
        help="Verible project source root path"
)
args = parser.parse_args()
root = args.path


class State(Enum):
    NORMAL = 0,
    MODULE_DEFINE = 1,
    SLANG_VERIFIED = 2,
    MISC_PREPROCESSOR = 3,
    MACRO_CALL_SYNTAX = 4


# structure that contains the information about a particular error
class ErrorContainer:
    def __init__(
            self,
            project,
            source_path,
            line_number,
            start_char,
            end_char,
            error_text,
            category='undefined'
    ):
        self.project = project
        self.source_path = source_path
        self.line_number = line_number
        self.start_char = start_char
        self.end_char = end_char
        self.category = category
        self.error = error_text
        self.slang_output = None
        self.rg_output = None

    def __str__(self):
        return f"Error in project: {self.project}\
 of category: {self.category}\n \
 source file: {self.source_path}\n \
 on line: {self.line_number}, col: {self.start_char}-{self.end_char}\n \
 full text:{self.error}"


# error_dirs holds all the *-nonzeros directories in the provided root
error_dirs = glob.glob(root+'/*-nonzeros')
project_urls = sorted([
            "https://github.com/lowRISC/ibex",
            "https://github.com/lowRISC/opentitan",
            "https://github.com/chipsalliance/sv-tests",
            "https://github.com/chipsalliance/Cores-VeeR-EH2",
            "https://github.com/chipsalliance/caliptra-rtl",
            "https://github.com/openhwgroup/cva6",
            "https://github.com/SymbiFlow/uvm",
            "https://github.com/taichi-ishitani/tnoc",
            "https://github.com/jamieiles/80x86",
            "https://github.com/SymbiFlow/XilinxUnisimLibrary",
            "https://github.com/black-parrot/black-parrot",
            "https://github.com/steveicarus/ivtest",
            "https://github.com/trivialmips/nontrivial-mips",
            "https://github.com/pulp-platform/axi",
            "https://github.com/rsd-devel/rsd",
            "https://github.com/syntacore/scr1",
            "https://github.com/olofk/serv",
            "https://github.com/bespoke-silicon-group/basejump_stl"
])

urls_with_names = sorted(
        [(i, i.split('/')[-1]) for i in project_urls],
        key=lambda x: x[1].lower()
)
error_dirs = sorted(error_dirs, key=lambda x: x.lower())


# method that classifies the errors depending on some categories
# and the internal state of the error checker - for each file the
# state is reset, as leaving it present in between files was not
# needed at the current moment
def error_classifier(src, line, state, project):
    # extracting the postition presetned as
    # filename:line:starting_col:ending_col:
    error_pos = re.search(
            r":[0-9]+:[0-9]+(-[0-9]+)*:",
            line
    )
    line_number = error_pos[0].split(':')[1]
    start_char = error_pos[0].split(':')[2]
    end_char = start_char.split('-')[-1]
    start_char = start_char.split('-')[0]

    # creating the container for the error
    err = ErrorContainer(
            project_name,
            source_path,
            int(line_number),
            int(start_char),
            int(end_char),
            line
    )
    # error processing

    # check if the macro had not been resolved and if so - mark the error
    if re.search(
            r'Error expanding macro identifier',
            line):
        err.category = 'unresolved-macro'
    # find a syntax error where the define is placed near
    # (inside of the parameter declaration) of a module -
    # it causes a chain of syntax errors later so change
    # the state to another value
    if state == State.SLANG_VERIFIED:
        err.category = 'related-to-slang-validated-error'
    if err.source_path[-4:] == '.svh' and re.search(
            r'syntax error at token "(?:(?!include|define|undef|ifdef|ifndef).)+',  # noqa: E501
            line):
        err.category = 'standalone-header'
    elif re.search(
            r'syntax error at token "`(?:(?!include|define|undef|ifdef|ifndef).)+',  # noqa: E501
            line):
        if re.search(
                "module",
                '\n'.join(src[max(err.line_number-30, 0):err.line_number])):
            state = State.MODULE_DEFINE
            err.category = 'macro-call-in-module-params'
        else:
            state = State.MISC_PREPROCESSOR
            err.category = 'misc-preprocessor'
    elif state == State.MISC_PREPROCESSOR and re.search(
            "syntax error at token",
            line):
        err.category = 'misc-preprocessor-related'
    # if the state is 1 (macro-call-in-module-params error), then every
    # subsequent syntax error should be marked as related to it
    elif state == State.MODULE_DEFINE and re.search(
            "syntax error at token",
            line):
        err.category = 'caused-by-macro-call-in-module-params'
    elif state == State.MODULE_DEFINE and re.search(
            "syntax error at token",
            line):
        err.category = 'caused-by-macro-call-in-module-params'
    # usually the syntax error related to the macro-call-in-module-params
    # problem end at an endmodule token - change the state back to
    # default when it is detected
    if state == State.MODULE_DEFINE and re.search(
            "syntax error at token \"endmodule\"",
            line):
        state = State.NORMAL
    if state == State.MACRO_CALL_SYNTAX and re.search(
            "syntax error at token",
            line):
        err.category = 'related-to-likely-unhandled-macro-call'
    # see if in ivtest - a project with some files having intentional
    # errors for testing purposes - the presence of an error is indicated
    # in the file name
    if 'ivtest' in project and re.search(
            r'ivtest\/(\w+\/)+.*(fail|error)\w*\.\w+',
            line):
        err.category = 'test-designed-to-fail'
    if 'rsd' in project and re.search(
            r'"Error!"',
            line):
        err.category = 'hit-preprocessor-failsafe'
    if err.category == 'undefined' and \
            re.search("syntax error at token", line) and \
            re.search(
                r'`(?:(?!include|define|undef|ifdef|ifndef).)+',
                '\n'.join(src[max(err.line_number-2, 0):err.line_number])):
        err.category = 'likely-unhandled-macro-call'
        state = State.MACRO_CALL_SYNTAX
    return err, state


def get_slang_output(srcpath):
    # subprocess run where the output is captured into a string
    proc = subprocess.run(
            [SLANG, '--error-limit=0', srcpath.strip()],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE
    )
    # split the output into lines
    slang_output = proc.stderr.decode('utf-8').split('\n')
    return slang_output


def get_rg_output(project_root, filename):
    # subprocess run where the output is captured into a string
    proc = subprocess.run(
            [RIPGREP, "include \""+filename, project_root],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE
    )
    # split the output into lines
    rg_output = proc.stdout.decode('utf-8').split('\n')
    return rg_output


# function that given an array of strings joins them up until
# a next regex match is detected. If there are no matches,
# the enitre strings array will get concatenated.
# It is meant to be used with slang output
# to join multiline errors and their notes together:
def join_until_regex(strings, regex):
    new_src = []
    joined = ''
    for line in strings:
        if not re.search(regex, line):
            joined += line
        elif joined != '':
            new_src.append(joined)
            joined = line
        else:
            joined = line
    return new_src


# a function that returns true if there is no match with common
# preprocessor / include problems that are just not important for this
def is_slang_line_important(line):
    return re.search(
            r'error:( \w*)*(unknown|undeclared)',
            line) is None


def validate_slang(src, line, state, project, srcpath, err):
    if err.slang_output is None:
        err.slang_output = get_slang_output(srcpath)
    # split the output such that there is one error per string in the
    # errors list
    errors = join_until_regex(err.slang_output, re.escape(srcpath))
    # extract the error line number and column from the
    # slang errors
    for line in errors:
        # regex that matches the :line:character: pattern
        # present in slang error messages
        error_pos = re.search(
                r":[0-9]+:[0-9]+:",
                line
        )
        line_number = int(error_pos[0].split(':')[1].strip())
        start_char = int(error_pos[0].split(':')[2].strip())
        # just to make sure that those were extracted such that
        # they are comparable
        assert isinstance(line_number, type(err.line_number))

        # check if the slang error is in the vicinity of the
        # original error and if the error is not a complaint
        # about a missing macro,function, etc
        if line_number == err.line_number and \
                abs(start_char - err.start_char) <= 10 and \
                is_slang_line_important(line):
            err.category = 'slang-verified-error'
            state = State.SLANG_VERIFIED
    return err.category, state


# create a markdown file that will contain the output of the script
mdFile = MdUtils(file_name='sta', title='Smoke test result analysis')
mdFile.new_paragraph(CATEGORY_DESCRIPTIONS)

# load files and get the metadata from them
for i, (url, project_name) in zip(error_dirs, urls_with_names):
    assert ("-".join(i.split('/')[-1].split('-')[:-1]) == project_name)
    with tempfile.TemporaryDirectory() as tempdirname:
        # keeps the indented block for
        # swapping with the `with` statement
        # while having a predictable path
        p = subprocess.run(
                ["git clone " + url+' '+tempdirname+'/'+project_name],
                stdout=subprocess.PIPE, shell=True,
                stderr=subprocess.DEVNULL
        )
        p.check_returncode()
        main_path = os.getcwd()
        os.chdir(tempdirname+'/'+project_name)
        project_files = glob.glob(
                '**',
                recursive=True
        )
        os.chdir(i)
        verible_error_files = glob.glob('**', recursive=True)
        os.chdir(main_path)
        project_errors = defaultdict(list)
        for file in verible_error_files:
            file_with_tool = ':'.join(file.split('-')[1:])
            exit_code = int(file.split('-')[0])
            if exit_code == 1:
                tool = file_with_tool.split('_')[-1].replace(':', '-')
                if tool == 'preprocessor':
                  continue   # error messages here are not file:line:col yet.
                filename = '_'.join(file_with_tool.split('_')[:-1])

                # if there were dashes ('-') in the file name, they
                # need to be re-replaced back from colons
                if len(filename.split(':')) > 1:
                    filename = filename.replace(':', '-')

                source_path_matches = [
                        i for i in project_files
                        if re.search(re.escape(filename), i)
                ]
                source_path = None
                if len(source_path_matches) > 1:
                    # the path needs to be scooped from the file itself
                    with open(i+'/'+file, 'r') as f:
                        for line in f:
                            m = re.search(
                                    project_name+r"(\/[\w,:\-\.]+)+\/[^:]+",
                                    line
                            )
                            if m and source_path is None:
                                source_path = "/".join(m[0].split('/')[1:])
                                break
                elif 'project' not in tool:
                    try:
                        source_path = source_path_matches[0]
                    except IndexError:
                        print(file, filename)
                        input()
                        break
                else:
                    # TODO: deal with the project problems later
                    continue
                with open(tempdirname+'/'+project_name+'/'+source_path) as s:
                    src = s.readlines()
                    state = State.NORMAL
                    with open(i+'/'+file, 'r') as f:
                        for line in f:
                            err, state = error_classifier(
                                    src,
                                    line,
                                    state,
                                    project_name
                            )
                            if err.category == 'undefined':
                                if err.rg_output is None:
                                    project_root = tempdirname+'/' + \
                                            project_name+'/'
                                    filename = source_path.split('/')[-1]
                                    err.rg_output = get_rg_output(
                                            project_root,
                                            filename
                                    )
                                    if err.rg_output[0] != "":
                                        err.category = 'standalone-header'
                            if err.category == 'undefined':
                                srcpath = tempdirname+'/' + \
                                        project_name+'/' + \
                                        source_path
                                err.category, state = validate_slang(
                                        src,
                                        line,
                                        state,
                                        project_name,
                                        srcpath,
                                        err
                                )
                            if SLANG_DEBUG_OUT:
                                if err.category == 'slang-verified-error':
                                    print(err, "state: ", state)
                                if 'slang' in err.category:
                                    print("\n".join(err.slang_output))
                            project_errors[project_name].append(deepcopy(err))
    # Per-project stats
    all = len(project_errors[project_name])
    error_types = defaultdict(int)
    for error in project_errors[project_name]:
        error_types[error.category] += 1
    mdFile.new_header(level=1, title=project_name)
    mdFile.new_line()
    md_table = ["Name", "Count", "All", str(all)]
    if all > 0:
        for key, value in error_types.items():
            md_table.extend([str(key), str(value)])
        mdFile.new_table(
                columns=2,
                rows=len(error_types)+2,
                text=md_table,
                text_align='left'
        )
    else:
        mdFile.new_table(columns=2, rows=2, text=md_table, text_align='left')
    mdFile.new_line()
    print(
        "Project: ", project_name,
        "\n  -All:",
        all,
        "\n  -Undefined:",
        error_types['undefined'],
        "\n  -Slang:",
        error_types['slang-verified-error'],
        "\n  -Related to slang:",
        error_types['related-to-slang-validated-error'],
        "\n  -Macro call in module params:",
        error_types['macro-call-in-module-params'],
        "\n  -Macro call in module params caused:",
        error_types['caused-by-macro-call-in-module-params'],
        "\n  -Unresolved macro:",
        error_types['unresolved-macro'],
        "\n  -Test designed to fail:",
        error_types['test-designed-to-fail'],
        "\n  -Misc. preporcesor: ",
        error_types['misc-preprocessor'],
        '\n  -Related to misc. preprocessor: ',
        error_types['misc-preprocessor-related'],
        '\n  -Standalone header: ',
        error_types['standalone-header'],
        '\n  -Hit preprocessor failsafe condition:',
        error_types['hit-preprocessor-failsafe'],
        '\n  -Found a likely unhandled macro call:',
        error_types['likely-unhandled-macro-call'],
        '\n  -Related to likely unhandled macro call:',
        error_types['related-to-likely-unhandled-macro-call'],
    )

    # check if the output is sane
    assert sum([error_types[i] for i in error_types.keys()]) == all
    assert error_types['macro-call-in-module-params'] == 0 or \
        error_types['macro-call-in-module-params'] > 0
    assert error_types['related-to-slang-validated-error'] == 0 or \
        error_types['slang-verified-error'] > 0
    assert error_types['misc-preprocessor-related'] == 0 or \
        error_types['misc-preprocessor-related'] > 0
    assert error_types['related-to-likely-unhandled-macro-call'] == 0 or \
        error_types['likely-unhandled-macro-call'] > 0


# Output the slang version string to the log
proc = subprocess.run(
        [SLANG, '--version'],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE
)
slang_output = proc.stdout.decode('utf-8').split('\n')
print(slang_output[0])
mdFile.new_header(level=1, title="Version info")
mdFile.new_header(level=2, title="Slang")
mdFile.new_line(f"Slang version info:\n\n{slang_output[0]}")
mdFile.new_header(level=2, title="Verible")
if args.verible_path:
    # Give version string for verible
    proc = subprocess.run(
            [args.verible_path +
                "/bazel-bin/verible/verilog/tools/syntax/verible-verilog-syntax",
                '--version'],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE
    )
    verible_out = proc.stdout.decode('utf-8').split('\n')
    print("Verible version string:\n\n"+"\n".join(verible_out), end='')
    mdFile.new_line("Verible version string:\n"+"\n".join(verible_out))
else:
    print("Verible path not specified, omitting version string")
    mdFile.new_line("Verible path not specified, omitting version string")
    print("Please provide --verible-path path argument pointing to the root")
    mdFile.new_line(
            "Please provide --verible-path path argument pointing to the root"
    )
    print("of the verible repository")
    mdFile.new_line("of the verible repository")

mdFile.create_md_file()
