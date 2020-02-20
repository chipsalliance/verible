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
bazel build //...
```

### Test

To run the tests in [bazel]:

```bash
# Run all tests
bazel test //...
```

You can access the generated artifacts under `bazel-bin/`. For instance the
syntax checker will be at `bazel-bin/verilog/tools/syntax/verilog_syntax`
(corresponding to the target name `//verilog/tools/syntax:verilog_syntax`).

### Install

Install in the preferred way on your operating system. On Unix-like systems,
this would be commands like (for the tools described below):

```bash
sudo install bazel-bin/verilog/tools/syntax/verilog_syntax /usr/local/bin
sudo install bazel-bin/verilog/tools/formatter/verilog_format /usr/local/bin
sudo install bazel-bin/verilog/tools/lint/verilog_lint /usr/local/bin
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
`//verilog/tools/syntax:verilog_syntax`.

```
verilog_syntax: usage: verilog_syntax [options] <file> [<file>...]

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
(token stream). `verilog_syntax --printtokens` shows the tokens that feeds into
the parser, and `--printrawtokens` to shows all tokens including whitespaces,
comments, and attributes.

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
`verilog_syntax --printtree`.

For example, the following code (same as above):

```
// This is module foo.
module foo(input a, b, output z);
endmodule : foo
```

produces this CST (rendered by `verilog_syntax --printtree`):

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
rule.

The linter tool is available as `//verilog/tools/lint:verilog_lint`.

```
verilog_lint: usage: verilog_lint [options] <file> [<file>...]

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

The formatter tool is available as `//verilog/tools/formatter:verilog_format`.

```
verilog_format: usage: verilog_format [options] <file>
To pipe from stdin, use '-' as <file>.

  Flags from verilog/tools/formatter/verilog_format.cc:
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

### Future Intent

The Verible team is interested in exploring how it can help other tool
developers in providing a SystemVerilog front end, for example, emitting an
abstract syntax tree (AST). If you are interested in collaborating, contact us.

[bazel]: https://bazel.build/
[SV-LRM]: https://ieeexplore.ieee.org/document/8299595
[lint-rule-list]: https://google.github.io/verible/lint.html
