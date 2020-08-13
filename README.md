---
---

# Verible

The Verible project's main mission is to parse SystemVerilog (IEEE 1800-2017)
for a wide variety of applications.

It was born out of a need to parse *un-preprocessed* source files, which is
suitable for single-file applications like style-linting and formatting. In
doing so, it can be adapted to parse *preprocessed* source files, which is what
real compilers and toolchains require.

The spirit of the project is that no-one should ever have to develop a
SystemVerilog parser for their own application, because developing a
standard-compliant parser is an enormous task due to the syntactic complexity of
the language.

A lesser (but notable) objective is that the language-agnostic components of
Verible be usable for rapidly developing language support tools for other
languages.

## Build

Verible's code base is written in C++.

To build, you need the [bazel] build system and a C++11 compatible compiler
(e.g. >= g++-7; Using clang currently fails to compile the m4 dependency).

```bash
# Build all tools and libraries
bazel build -c opt //...
```

You can access the generated artifacts under `bazel-bin/`. For instance the
syntax checker will be at
`bazel-bin/verilog/tools/syntax/verible-verilog-syntax` (corresponding to the
target name `//verilog/tools/syntax:verible-verilog-syntax`).

### Installation

For simple installation, we provide regular [binary releases].

If you prefer to install the binaries on your workstation yourself, use the
`:install` target which you invoke with `bazel run`.

The following builds and installs the `verible-verilog-format`,
`verible-verilog-lint` and `verible-verilog-syntax` binaries in the provided
location:

```bash
# In your home directory
bazel run :install -c opt -- ~/bin

# For a system directory that requires root-access, call with -s option.
# (Do _not_ run bazel with sudo.)
bazel run :install -c opt -- -s /usr/local/bin
```

### Test

To run the tests in [bazel]:

```bash
# Run all tests
bazel test //...
```

## Mailing Lists

Join the Verible community!

*   Developers: verible-dev@googlegroups.com
    ([join](https://groups.google.com/forum/#!forum/verible-dev/join))
*   Users: verible-users@googlegroups.com
    ([join](https://groups.google.com/forum/#!forum/verible-users/join))

## SystemVerilog Support

### Parser

The lexer is implemented using GNU Flex, and the parser is implemented using GNU
Bison (yacc). To parse un-preprocessed input, preprocessing constructs had to be
handled explicitly in the parser, and are permitted in limited places. The
grammatic rules in the yacc input are approximate and permissive; it may accept
some syntactically invalid constructs. The priority is to *accept* all
syntactically *valid* SystemVerilog, as defined in the [SV-LRM]. Status: As of
2019, it accepts the vast majority of SystemVerilog (IEEE 1800-2017), but there
is work ahead to reach 100%.

The lexer and parser are *decoupled*, which means that the lexer can be used
standalone to tokenize text, and the parser is adapted to accept tokens from
sources other than the direct use of the lexer. This separation enables the
insertion of different passes between the lexer and parser, such as integrated
preprocessing, and context-based lexical disambiguation (with arbitrary
lookahead) where required by the language.

The parser can be tested as a standalone program,
`//verilog/tools/syntax:verible-verilog-syntax`.

```
verible-verilog-syntax: usage: verible-verilog-syntax [options] <file> [<file>...]

  Flags from verilog/tools/syntax/verilog_syntax.cc:
    -printrawtokens (Prints all lexed tokens, including filtered ones.);
      default: false;
    -printtokens (Prints all lexed and filtered tokens); default: false;
    -printtree (Whether or not to print the tree); default: false;
    -verifytree (Verifies that all tokens are parsed into tree, prints unmatched
      tokens); default: false;

Try --helpfull to get a list of all flags.
```

#### Selecting Parsing Mode {#parsing-mode}

If your source file is not a standalone Verilog description as defined by the
LRM, (e.g. a body snippet of a module, class, task, or function without the
header or end) the parser can use an alternate parsing mode by using a comment
directive before the first (non-comment) token.

```verilog
// This file is `included inside a module definition body.
// verilog_syntax: parse-as-module-body
wire x;
initial begin
  x = 0;
end
```

The `verilog_syntax` directive tells the parser that the contents of this file
are to be treated as if they are inside a module declaration.

Available parser selection modes include:

*   `parse-as-statements`: Text contains only statements (e.g. inside function,
    always, ...)
*   `parse-as-expression`: Text occurs inside an expression.
*   `parse-as-module-body`: Text occurs inside a module definition.
*   `parse-as-class-body`: Text occurs inside a class definition.
*   `parse-as-package-body`: Text occurs inside a package definition.

#### Token Stream

The lexer partitions a text buffer into a sequence of tokens with annotations
(token stream). `verible-verilog-syntax --printtokens` shows the tokens that
feeds into the parser, and `--printrawtokens` to shows all tokens including
whitespaces, comments, and attributes.

For example, the following code:

```
// This is module foo.
module foo(input a, b, output z);
endmodule : foo
```

produces the following tokens (shown using `--printrawtokens`):

```
All lexed tokens:
All lexed tokens:
(#"// end of line comment" @0-22: "// This is module foo.")
(#"<<\\n>>" @22-23: "
")
(#"module" @23-29: "module")
(#"<<space>>" @29-30: " ")
(#SymbolIdentifier @30-33: "foo")
(#'(' @33-34: "(")
(#"input" @34-39: "input")
(#"<<space>>" @39-40: " ")
(#SymbolIdentifier @40-41: "a")
(#',' @41-42: ",")
(#"<<space>>" @42-43: " ")
(#SymbolIdentifier @43-44: "b")
(#',' @44-45: ",")
(#"<<space>>" @45-46: " ")
(#"output" @46-52: "output")
(#"<<space>>" @52-53: " ")
(#SymbolIdentifier @53-54: "z")
(#')' @54-55: ")")
(#';' @55-56: ";")
(#"<<\\n>>" @56-57: "
")
(#"endmodule" @57-66: "endmodule")
(#"<<space>>" @66-67: " ")
(#':' @67-68: ":")
(#"<<space>>" @68-69: " ")
(#SymbolIdentifier @69-72: "foo")
(#"<<\\n>>" @72-73: "
")
(#"<<\\n>>" @73-74: "
")
(#$end @74-74: "")
```

The token names (after `#`) correspond to description strings in the yacc
grammar file; keywords are shown the same as the text they match. Byte offsets
are shown as the range that follows '@'. The raw, unfiltered token stream is
lossless with respect to the original input text.

#### Concrete Syntax Tree

The parser produces a concrete syntax tree (CST), which can be diagnosed with
`verible-verilog-syntax --printtree`.

For example, the following code (same as above):

```
// This is module foo.
module foo(input a, b, output z);
endmodule : foo
```

produces this CST (rendered by `verible-verilog-syntax --printtree`):

```
Parse Tree:
Node (tag: kDescriptionList) {
  Node (tag: kModuleDeclaration) {
    Node (tag: kModuleHeader) {
      (#"module" @23-29: "module")
      (#SymbolIdentifier @30-33: "foo")
      Node (tag: kParenGroup) {
        (#'(' @33-34: "(")
        Node (tag: kPortDeclarationList) {
          Node (tag: kPortDeclaration) {
            (#"input" @34-39: "input")
            Node (tag: kDataType) {
            }
            Node (tag: kUnqualifiedId) {
              (#SymbolIdentifier @40-41: "a")
            }
            Node (tag: kUnpackedDimensions) {
            }
          }
          (#',' @41-42: ",")
          Node (tag: kPort) {
            Node (tag: kPortReference) {
              Node (tag: kUnqualifiedId) {
                (#SymbolIdentifier @43-44: "b")
              }
            }
          }
          (#',' @44-45: ",")
          Node (tag: kPortDeclaration) {
            (#"output" @46-52: "output")
            Node (tag: kDataType) {
            }
            Node (tag: kUnqualifiedId) {
              (#SymbolIdentifier @53-54: "z")
            }
            Node (tag: kUnpackedDimensions) {
            }
          }
        }
        (#')' @54-55: ")")
      }
      (#';' @55-56: ";")
    }
    (#"endmodule" @57-66: "endmodule")
    Node (tag: kLabel) {
      (#':' @67-68: ":")
      (#SymbolIdentifier @69-72: "foo")
    }
  }
}
```

Nodes of the CST may link to other nodes or leaves (which contain tokens). The
nodes are tagged with language-specific enumerations. Each leaf encapsulates a
token and is shown with its corresponding byte-offsets in the original text (as
`@left-right`). Null nodes are not shown.

The exact structure of the SystemVerilog CST is fragile, and should not be
considered stable; at any time, node enumerations can be created or removed, and
subtree structures can be re-shaped. In the above example, `kModuleHeader` is an
implementation detail of a module definition's composition, and doesn't map
directly to a named grammar construct in the [SV-LRM]. The `verilog/CST` library
provides functions that abstract away internal structure.

#### Abstract Syntax Tree

An abstract syntax tree (AST) does not exist yet, but is planned.

### Style Linter

The style linter is an analysis tool that identifies constructs or patterns
deemed undesirable according to a style guide. The main goal is to relieve
humans the burden of reviewing code for style compliance. Many lint rules use
syntax tree pattern matching to find style violations. The tool provides
generating a [built-in lint documentation][lint-rule-list] with details to each
rule; you can generate it by invoking the linter with `--generate_markdown` or
use the build script:

```bash
# Generating documentation
bazel build :lint_doc

# It will be generated into
bazel-bin/documentation_verible_lint_rules.md
```

The linter tool is available as `//verilog/tools/lint:verible-verilog-lint`.

```
verible-verilog-lint: usage: verible-verilog-lint [options] <file> [<file>...]

  Flags from verilog/analysis/verilog_linter.cc:
    --rules (Comma-separated of lint rules to enable. No prefix or a '+' prefix
      enables it, '-' disable it. Configuration values for each rules placed
      after '=' character.); default: ;
    --rules_config (Path to lint rules configuration file.);
      default: ".rules.verible_lint";
    --ruleset ([default|all|none], the base set of rules used by linter);
      default: default;

  Flags from verilog/tools/lint/verilog_lint.cc:
    -generate_markdown (If true, print the description of every rule formatted
      for the markdown and exit immediately. Intended for the output to be
      written to a snippet of markdown.); default: false;
    -help_rules ([all|<rule-name>], print the description of one rule/all rules
      and exit immediately.); default: "";
    -lint_fatal (If true, exit nonzero if linter finds violations.);
      default: false;
    -parse_fatal (If true, exit nonzero if there are any syntax errors.);
      default: false;

Try --helpfull to get a list of all flags.
```

#### Rule configuration

The `--rules` flag allows to enable/disable rules as well as pass configuration
to rules that accept them. It accepts a comma-separated list
[rule names][lint-rule-list]. If prefixed with a `-` (minus), the rule is
disabled. No prefix or a '+' (plus) prefix enables the rule. An optional
configuration can be passed after an `=` assignment.

The following example enables the
[`enum-name-style`][lint-rule-list_enum-name-style] rule, enables and configures
the [`line-length`][lint-rule-list_line-length] rule (80 characters length) and
disables the [`no-tabs`][lint-rule-list_no-tabs] rule.

```
verible-verilog-lint --rules=enum-name-style,+line-length=length:80,-no-tabs ...
```

Additionally, the `--rules_config` flag can be used to read configuration stored
in a file. The syntax is the same as above, except the rules can be also
separated with the newline character.

#### Waiving Lint Violations {#lint-waiver}

In the rare circumstance where a line needs to be waived from a particular lint
rule, you can use the following waiver comment:

```
// This example waives the line after the waiver.
// verilog_lint: waive rule-name
The next non-comment line like this one is waived.

// This example waives the same line as the waiver.
This line is waived.  // verilog_lint: waive rule-name

// This example shows accumulation of waivers over multiple lines.
// verilog_lint: waive rule-name-1
// verilog_lint: waive rule-name-2
// Other comments, possibly waivers for other tools.
This line will be waived for both rule-name-1 and rule-name-2.

// This example shows how to waive an entire range of lines.
// verilog_lint: waive-start rule-X
...
All lines in between will be waived for rule-X
...
// verilog_lint: waive-stop rule-X
```

Another option is to use a configuration file with the `--waiver_files` flag.
The flag accepts a single file or a list of files (comma-separated). Specifying
multiple files is equivalent to concatenating the files in order of appearance.
By default, the rules are applied to all files, but with `--location` you can to
choose to only apply them to filenames matching the location regexp.

The format of this file is as follows:

```
waive --rule=rule-name-1 --line=10
waive --rule=rule-name-2 --line=5:10
waive --rule=rule-name-3 --regex="^\s*abc$"
waive --rule=rule-name-4 --line=42 --location=".*some_file.*"
```

The `--line` flag can be used to specify a single line to apply the waiver to or
a line range (separated with the `:` character). Additionally the `--regex` flag
can be used to dynamically match lines on which a given rule has to be waived.
This is especially useful for projects where some of the files are
auto-generated.

The name of the rule to waive is at the end of each diagnostic message in `[]`.

Syntax errors cannot be waived. A common source of syntax errors is if the file
is not a standalone Verilog program as defined by the LRM, e.g. a body snippet
of a module, class, task, or function. In such cases, the syntax parser can be
directed to treat the code as a snippet by selecting a
[parsing mode](#parsing-mode).

### Formatter

The formatter is a transformative tool that manages whitespace in accordance
with a particular style. The main goal is to relieve humans of having to
manually manage whitespace, wrapping, and indentation, and to provide a tool
that can be integrated into any editor to enable editor-independent consistency.

The formatter tool is available as
`//verilog/tools/formatter:verible-verilog-format`.

```
verible-verilog-format: usage: verible-verilog-format [options] <file>
To pipe from stdin, use '-' as <file>.

  Flags from verilog/tools/formatter/verilog_format.cc:
    --failsafe_success (If true, always exit with 0 status, even if there were
      input errors or internal errors. In all error conditions, the original
      text is always preserved. This is useful in deploying services where
      fail-safe behaviors should be considered a success.); default: true;
    --inplace (If true, overwrite the input file on successful conditions.);
      default: false;
    --lines (Specific lines to format, 1-based, comma-separated, inclusive N-M
      ranges, N is short for N-N. By default, left unspecified, all lines are
      enabled for formatting. (repeatable, cumulative)); default: ;
    --max_search_states (Limits the number of search states explored during line
      wrap optimization.); default: 100000;
    --show_equally_optimal_wrappings (If true, print when multiple optimal
      solutions are found (stderr), but continue to operate normally.);
      default: false;
    --show_inter_token_info (If true, along with show_token_partition_tree,
      include inter-token information such as spacing and break penalties.);
      default: false;
    --show_largest_token_partitions (If > 0, print token partitioning and then
      exit without formatting output.); default: 0;
    --show_token_partition_tree (If true, print diagnostics after token
      partitioning and then exit without formatting output.); default: false;
    --stdin_name (When using '-' to read from stdin, this gives an alternate
      name for diagnostic purposes. Otherwise this is ignored.);
      default: "<stdin>";
    --verify_convergence (If true, and not incrementally formatting with
      --lines, verify that re-formatting the formatted output yields no further
      changes, i.e. formatting is convergent.); default: true;

Try --helpfull to get a list of all flags.
```

#### Disabling Formatting {#disable-formatting}

When you want to exempt a range of text from formatting, write:

```
// verilog_format: off
... untouched code ...
// verilog_format: on
```

or

```
/* verilog_format: off */
... untouched code ...
/* verilog_format: on */
```

As a good practice, include a reason why you choose to disable a section.

```
// verilog_format: off  // my alignment is prettier than the tool's

// verilog_format: off  // issue #N: working around.
```

These directives take precedence over `--lines` specifications.

#### Interactive Formatting

The formatter is ever a work-in-progress and may not always behave the way a
user would like it to behave. If you wish to review changes introduced by the
formatter and apply them interactively, we've provided a helper script (that is
actually formatter-agnostic) for the job.

```
verible-transform-interactive.sh -- verible-verilog-format -- files...
```

This will run the formatter on the affected files without modifying them
initially, and pass the `diff`-generated patch into a tool
(`verible-patch-tool`) that interactively applies patch hunks.

Answer the \[y/n\] prompts.

The original files will be modified in-place with the elected changes.

For convenience, one could create an alias like:

```
alias verilog-format-interactive='verible-transform-interactive.sh -- verible-verilog-format --'
```

#### Incremental Formatting

If you wish to format only changed lines (a.k.a. incremental or partial
formatting), the following tools are provided to assist.

##### Git users

`git-verible-verilog-format.sh` (installed along with `verible-verilog-format`)
can be run from within any subdirectory of a Git project. It automatically
detects new and changed lines, and generates the `--lines` flag for each
modified file in your workspace.

From `--help`:

```
git-verible-verilog-format.sh:
Performs incremental file formatting (verible-verilog-format) based on current diffs.
New files explicitly git-add-ed by the user are wholly formatted.

Actions:
  1) Runs 'git add -u' to stage currently modified files to the index.
     To format new files (wholly), 'git add' those before calling this script.
  2) Runs 'git diff -u --cached' to generate a unified diff.
  3) Diff is scanned to determine added or modified lines in each file.
  4) Invokes 'verible-verilog-format --inplace' on all touched or new Verilog files,
     but does not 'git add' so that the changes may be examined and tested.
     Formatting can be easily undone with:
       'git diff | git apply --reverse -'.

usage: git-verible-verilog-format.sh [script options] [-- [verilog_format options]]
  (no positional arguments)
  Run from anywhere inside a git project tree.

script options: (options with arguments can be: --flag=VALUE or --flag VALUE)
  --help | -h : print help and exit
  --verbose | -v : execute verbosely
  --dry-run : stop before running formatter, and print formatting commands
  --formatter TOOL : path to verilog_format binary
       [using: /usr/local/bin/verible-verilog-format]
  -- : stops option processing, and forwards remaining args as flags to the
       underlying --formatter tool.
```

#### Incremental Interactive Formatting

In your locally modified client (p4, git) run:

```shell
verible-verilog-format-changed-lines-interactive.sh
```

and follow the prompts.

> NOTE: git and hg users can pass in a different base revision to diff against:
>
> ```shell
> # Diff against the previous revision in hg
> verible-verilog-format-changed-lines-interactive.sh --rev .^
> # Diff against upstream mainline in git
> verible-verilog-format-changed-lines-interactive.sh --rev origin/main
> ```

### Lexical Diff

`verible-verilog-diff` compares two input files for equivalence, where
equivalence is determined by the `--mode` of operation.

```
verible-verilog-diff: usage: verible-verilog-diff [options] file1 file2

  Flags from verilog/tools/diff/verilog_diff.cc:
    --mode (Defines difference functions.
      format: ignore whitespaces, compare token texts
        This is useful for verifying formatter (e.g. verilog_format) output.
      obfuscate: preserve whitespaces, compare token texts' lengths only.
        This is useful for verifying verilog_obfuscate output.
      ); default: format;
```

Exit codes:

*   0: files are equivalent
*   1: files differ, or contain lexical errors
*   2: error reading file

### Code Obfuscator

`verible-verilog-obfuscate` transforms Verilog code by replacing identifiers
with obfuscated names of equal length, and preserving all other text, including
spaces. Output is written to stdout. The resulting file size is the same as the
original.

This is useful for preparing potentially sensitive test cases with tool vendors.

```
verible-verilog-obfuscate: usage: verible-verilog-obfuscate [options] < original_file > output

verible-verilog-obfuscate mangles Verilog code by changing identifiers.
All whitespaces and identifier lengths are preserved.
Output is written to stdout.

  Flags from verilog/tools/obfuscator/verilog_obfuscate.cc:
    --decode (If true, when used with --load_map, apply the translation
      dictionary in reverse to de-obfuscate the source code, and do not
      obfuscate any unseen identifiers. There is no need to --save_map with this
      option, because no new substitutions are established.); default: false;
    --load_map (If provided, pre-load an existing translation dictionary
      (written by --save_map). This is useful for applying pre-existing
      transforms.); default: "";
    --save_map (If provided, save the translation to a dictionary for reuse in a
      future obfuscation with --load_map.); default: "";
```

### Preprocessor

`verible-verilog-preprocessor` is a collection of preprocessor-like tools, (but
does not include a fully-featured Verilog preprocessor yet.)

#### Strip Comments

Removing comments can be useful step in preparing to obfuscate code.

```shell
$ verible-verilog-preprocessor strip-comments FILE
```

This strips comments from a Verilog/SystemVerilog source file, _including_
comments nested inside macro definition bodies and macro call arguments.

Comments are actually replaced by an equal number of spaces (and newlines) to
preserve the byte offsets and line ranges of the original text.

## Indexing Verible C++ Code using Kythe

The steps mentioned here can be generalized for indexing Bazel-based projects.

More information about indexing Bazel-based projects using Kythe
[here](https://github.com/kythe/kythe/tree/master/kythe/cxx/extractor#bazel-c-extractor).

### Initializing Kythe

Download the latest Kythe release from https://github.com/kythe/kythe/releases
and then unpack it for a snapshot of Kythe’s toolset.

```bash
tar xzf kythe-v*.tar.gz
rm -rf /opt/kythe
mv kythe-v*/ /opt/kythe
```

More information can be found
[here](https://github.com/kythe/kythe#getting-started).

Clone Kythe from [here](https://github.com/kythe/kythe). Then from within the
Kythe clone, build the web frontend and copy its files into /opt/kythe/

```bash
bazel build //kythe/web/ui
mkdir -p /opt/kythe/web/ui
cp -r bazel-bin/kythe/web/ui/resources/public/* /opt/kythe/web/ui
cp -r kythe/web/ui/resources/public/* /opt/kythe/web/ui
chmod -R 755 /opt/kythe/web/ui
```

More information can be found
[here](https://github.com/google/haskell-indexer#building-from-source).

### Extracting Compilations for Verible

#### VNames.json File

`vnames.json` file is used for renaming certain filepaths during extraction, but
renaming isn’t needed here but you can add the renaming you find suitable.

#### Run the extractor

```bash
# run on all targets
bazel test --experimental_action_listener=:extract_cxx  //...

# run on specific target (e.g. some cc_binary or cc_library)
bazel test --experimental_action_listener=:extract_cxx //verilog/analysis:default_rules
```

Extracted kzip files will be in
bazel-out/local-fastbuild/extra_actions/extractor folder. One kzip file per
target.

```bash
find -L bazel-out -name '*cxx.kzip'
```

More information can be found
[here](https://github.com/kythe/kythe/tree/master/kythe/cxx/extractor).

### Indexing Extracted Kzip Files and Generating GraphStore

For extracting the indexing facts from extracted `kzip` files, run the following
command to apply the indexing for each `kzip` file and generate the result into
`kythe graphstore`.

```bash
for i in $(find -L bazel-out -name '*cxx.kzip'); do
    # Indexing C++ compilations
    /opt/kythe/indexers/cxx_indexer --ignore_unimplemented "$i" > entries

    # Write entry stream into a GraphStore
    /opt/kythe/tools/write_entries --graphstore leveldb:.kythe_graphstore < entries
done
```

### Generating Serving Tables

```bash
# Generate corresponding serving tables
/opt/kythe/tools/write_tables --graphstore .kythe_graphstore --out .kythe_serving
```

### Visualizing Cross-References

Run Kythe's `UI` to visualize cross-reference and code navigation.

```bash
# --listen localhost:8080 allows access from only this machine; change to
# --listen 0.0.0.0:8080 to allow access from any machine
/opt/kythe/tools/http_server \
  --public_resources /opt/kythe/web/ui \
  --listen localhost:8080 \
  --serving_table .kythe_serving
```

### Future Intent

The Verible team is interested in exploring how it can help other tool
developers in providing a SystemVerilog front end, for example, emitting an
abstract syntax tree (AST). If you are interested in collaborating, contact us.

[bazel]: https://bazel.build/
[SV-LRM]: https://ieeexplore.ieee.org/document/8299595
[lint-rule-list]: https://google.github.io/verible/lint.html
[lint-rule-list_enum-name-style]: https://google.github.io/verible/lint.html#enum-name-style
[lint-rule-list_line-length]: https://google.github.io/verible/lint.html#line-length
[lint-rule-list_no-tabs]: https://google.github.io/verible/lint.html#no-tabs
[binary releases]: https://github.com/google/verible/releases
