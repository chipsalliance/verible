# ![](./img/verible-logo-headline.png)Verible



[![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)
[![Continuous Integration](https://github.com/chipsalliance/verible/workflows/ci/badge.svg)](https://github.com/chipsalliance/verible/actions/workflows/verible-ci.yml)
[![codecov](https://codecov.io/gh/chipsalliance/verible/branch/master/graph/badge.svg?token=5f656dpmDT)](https://codecov.io/gh/chipsalliance/verible)

<!--*
freshness: { owner: 'hzeller' reviewed: '2022-08-31' }
*-->

The Verible project's main mission is to parse SystemVerilog (IEEE 1800-2017)
(as standardized in the [SV-LRM]) for a wide variety of applications, including
developer tools.

It was born out of a need to parse *un-preprocessed* source files, which is
suitable for single-file applications like style-linting and formatting. In
doing so, it can be adapted to parse *preprocessed* source files, which is what
real compilers and toolchains require.

The spirit of the project is that no-one should ever have to develop a
SystemVerilog parser for their own application, because developing a
standard-compliant parser is an enormous task due to the syntactic complexity of
the language. Verible's parser is also regularly tested against an ever-growing
suite of (tool-independent) language compliance tests at
https://symbiflow.github.io/sv-tests/.

A lesser (but notable) objective is that the language-agnostic components of
Verible be usable for rapidly developing language support tools for other
languages.

### Installation

For simple installation, we provide regular [binary releases] for Linux and
Windows, including statically linked binaries for x86 and Arm to run on almost
any Linux distribution.

There are also some distributions that include Verible

  * [Nix] has binaries for Linux x86 and Arm.
  * There is a [homebrew] package for MacOS.

If you prefer to build and install the binaries locally yourself, see
details below in the [Developers](#developers-welcome) section.

## SystemVerilog Developer Tools

### Parser

[Learn more about the parser implementation here](./verible/verilog/parser).

We provide a standalone [`verible-verilog-syntax`](./verible/verilog/tools/syntax) tool
to help with visualizing the syntax structure as understood by the lexer and
parser. This is very useful for troubleshooting and understand the internal
representations seen by the other tools.

The tool has an ability of exporting a concrete syntax tree in JSON format,
making use of it in external tools easy. There is also a
[Python wrapper module and a few example scripts](./verible/verilog/tools/syntax/export_json_examples).

### Style Linter

[`verible-verilog-lint`](./verible/verilog/tools/lint) identifies constructs or patterns
in code that are deemed undesirable according to a style guide. The main goal is
to relieve humans the burden of reviewing code for style compliance. Many
[lint rules][lint-rule-list] use syntax tree pattern matching to find style
violations.

Features:

 * Style guide citations in diagnostics
 * Rule deck configurability
 * Waiver mechanisms: in-file, external waiver file
 * [Github SystemVerilog linter action][github-lint-action] available.

![Integrating Verible Linter in Github screenshot](./img/example-github-integration.png)

Documentation:

*   [Style linter user documentation](./verible/verilog/tools/lint)
*   [Generated lint rule documentation][lint-rule-list]

### Formatter

The [`verible-verilog-format`](./verible/verilog/tools/formatter) formatter manages
whitespace in accordance with a particular style. The main goal is to relieve
humans of having to manually manage whitespace, wrapping, and indentation, and
to provide a tool that can be integrated into any editor to enable
editor-independent consistency.

Features (various degress of work-in-progress):

 * Corrects indentation
 * Corrects inter-token spacing, with syntax context awareness
 * Line-wrapping to a column limit
 * Support for incremental formatting, only touched changed lines.
 * Interactive formatting: accept or decline formatting changes
 * Tabular alignment
 * [Github SystemVerilog formatter action][github-format-action] available.

<!--
TODO(fangism): a demo GIF animation here.
See https://github.com/chipsalliance/verible/issues/528
-->

### Language Server

The [`verible-verilog-ls`](./verible/verilog/tools/ls) is a language server that
provides the functionalities that come with the Verible command line tools
also directly in your editor.

It implements the standardized [language server protocol] that is supported
by a myriad of editors and IDEs.

The language server provides formatting and linting. If possible, it also
provides quick-fixes

![Showing a lint message with quick-fix in vscode screenshot](./img/language-server-demo-vscode.png)

### Lexical Diff

[`verible-verilog-diff`](./verible/verilog/tools/diff) compares two input files for
equivalence.

### Verible project tool

[`verible-verilog-project`](./verible/verilog/tools/project) is a multi-tool that
operates on whole Verilog projects, consisting of a file list and related
configurations. This serves as a diagnostic tool for analyzing (and potentially
transforming) project-level sources.

### Code Obfuscator

[`verible-verilog-obfuscate`](./verible/verilog/tools/obfuscator) transforms Verilog
code by replacing identifiers with obfuscated names of equal length, and
preserving all other text, including spaces. Output is written to stdout. The
resulting file size is the same as the original. This is useful for preparing
potentially sensitive test cases with tool vendors.

<!--
TODO(fangism): a short demo GIF animation here.
See https://github.com/chipsalliance/verible/issues/528
-->

### Preprocessor

[`verible-verilog-preprocessor`](./verible/verilog/tools/preprocessor) is a collection
of preprocessor-like tools, (but does not include a fully-featured Verilog
preprocessor yet.)

### Source Code Indexer

[`verible-verilog-kythe-extractor`](./verible/verilog/tools/kythe) extracts indexing
facts from SV source code using the [Kythe](http://kythe.io) schema, which can
then enhance IDEs with linked cross-references for ease of source code
navigation.

<!--
TODO(minatoma): short animation of hover/navigation features
-->

## Developers, Welcome

For source code browsing, we recommend using the fully-indexed and searchable
mirror at https://cs.opensource.google/verible/verible.

If you'd like to contribute, check out the [contributing](./CONTRIBUTING.md)
guide and the [development resources](./doc/development.md).

### Build

Verible's code base is written in C++.

To build, you need the [bazel] build system and a C++17
compatible compiler (e.g. >= g++-10), as well as python3.
A lot of users of Verible have to work on pretty old installations,
so we try to keep the requirements as minimal as possible.

Use your package manager to install the dependencies; on a system with
the nix package manager simply run `nix-shell` to get a build environment.

```bash
# Build all tools and libraries
bazel build -c opt //...
```

You can access the generated artifacts under `bazel-bin/`. For instance the
syntax checker will be at
`bazel-bin/verible/verilog/tools/syntax/verible-verilog-syntax` (corresponding to the
target name `//verible/verilog/tools/syntax:verible-verilog-syntax`).

Moreover, if you need statically linked executables that don't depend on your
shared libraries, you can use custom config
`create_static_linked_executables` (with this setting `bfd` linker will be used,
instead of default `gold` linker).

```bash
# Generate statically linked executables.
# Uses bfd linker and needs static system libs available.
bazel build -c opt --config=create_static_linked_executables //...
```

### Deprecated: using local flex/bison for build

There used to be `--//bazel:use_local_flex_bison`, but it has been disabled
as it won't work with
[bazel 8](https://github.com/chipsalliance/verible/issues/2435).

### Building on Windows

Building on Windows requires LLVM, WinFlexBison 3 and Git-bash to be installed. Using package manager [chocolatey], this can be done with

```powershell
choco install git llvm winflexbison3
```

Bazel may also require environment variable to use git-bash and LLVM, on powershell

```powershell
$env:BAZEL_SH="C:\Program Files\Git\git-bash.exe"
$env:BAZEL_LLVM="C:\Program Files\LLVM"
```

### Installation

For simple installation, we provide regular [binary releases].

If you prefer to build and install the binaries locally yourself:

```bash
bazel build -c opt :install-binaries

# Install in your home directory
.github/bin/simple-install.sh ~/bin

# For a system directory that requires root-access, call scfript with sudo.
sudo .github/bin/simple-install.sh /usr/local/bin
```

(this requies a compliant `install` utility, otherwise simply copy
the binaries from `bazel-bin/` to your desired location)

### Test

We strongly encourage running the test suite using [bazel]:

```bash
# Run all tests
bazel test -c opt //...
```

Whenever adding new features in file, say, `foo.cc` always make sure to also
update (or add) the corresponding `foo_test.cc`. Once you've written the
test, you can use `.github/bin/generate-coverage-html.sh` to double-check
that you have covered all code-paths in your test; narrow the coverage
run to your test to make sure coverage is not accidentally coming from
unrelated tests that happen to use the library:

```bash
MODE=coverage .github/bin/build-and-test.sh //foo/bar:foo_test
.github/bin/generate-coverage-html.sh
```

## Mailing Lists

Join the Verible community!

*   Developers: verible-dev@googlegroups.com
    ([join](https://groups.google.com/forum/#!forum/verible-dev/join))
*   Users: verible-users@googlegroups.com
    ([join](https://groups.google.com/forum/#!forum/verible-users/join))

### Future

The Verible team is interested in exploring how it can help other tool
developers in providing a SystemVerilog front end, for example, emitting an
abstract syntax tree (AST) or possibly even provide more higher-level
[UHDM] format. If you are interested in collaborating, contact us.

[bazel]: https://bazel.build/
[SV-LRM]: https://ieeexplore.ieee.org/document/8299595
[lint-rule-list]: https://chipsalliance.github.io/verible/lint.html
[github-lint-action]: https://github.com/chipsalliance/verible-linter-action
[github-format-action]: https://github.com/chipsalliance/verible-formatter-action
[binary releases]: https://github.com/chipsalliance/verible/releases
[language server protocol]: https://microsoft.github.io/language-server-protocol/
[UHDM]: https://github.com/chipsalliance/UHDM
[homebrew]: https://github.com/chipsalliance/homebrew-verible
[Nix]: https://search.nixos.org/packages?channel=unstable&query=verible
[chocolatey]: https://chocolatey.org/
