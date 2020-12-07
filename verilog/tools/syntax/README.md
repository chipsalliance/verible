# SystemVerilog Syntax Tool

<!--*
freshness: { owner: 'fangism' reviewed: '2020-10-15' }
*-->

`verible-verilog-syntax` checks SystemVerilog syntax, and provides some useful
options for examining lexed/parsed representations. When troubleshooting
Verible's SystemVerilog tools, start with this tool.

You can read about [lexer and parser implementation details here](../../parser).

## Usage

```
usage: verible-verilog-syntax [options] <file(s)...>

  Flags from verilog/tools/syntax/verilog_syntax.cc:
    --error_limit (Limit the number of syntax errors reported. (0: unlimited));
      default: 0;
    --export_json (Uses JSON for output. Intended to be used as an input for
      other tools.); default: false;
    --lang (Selects language variant to parse. Options:
      auto: SystemVerilog-2017, but may auto-detect alternate parsing modes
      sv: strict SystemVerilog-2017, with explicit alternate parsing modes
      lib: Verilog library map language (LRM Ch. 33)
      ); default: auto;
    --printrawtokens (Prints all lexed tokens, including filtered ones.);
      default: false;
    --printtokens (Prints all lexed and filtered tokens); default: false;
    --printtree (Whether or not to print the tree); default: false;
    --verifytree (Verifies that all tokens are parsed into tree, prints
      unmatched tokens); default: false;
```

## Features

The parser supports
[alternative parsing modes](../../analysis#alternative-parsing-modes) where a
file is intended to be included in another context, such as module body items,
and can be triggered with comments near the top-of-file like `// verilog_syntax:
parse-as-module-body`.

## Token Stream Example

The following code:

```systemverilog
// This is module foo.
module foo(input a, b, output z);
endmodule : foo
```

produces the following tokens (shown using `--printrawtokens`):

```
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

With `--printtokens`, you should see whitespace tokens filtered out.

## Concrete Syntax Tree Example

The following code (same as above):

```systemverilog
// This is module foo.
module foo(input a, b, output z);
endmodule : foo
```

produces this [concrete syntax tree (CST)](../../CST), rendered by
`verible-verilog-syntax --printtree`:

```
Parse Tree:
Node @0 (tag: kDescriptionList) {
  Node @0 (tag: kModuleDeclaration) {
    Node @0 (tag: kModuleHeader) {
      Leaf @0 (#"module" @23-29: "module")
      Leaf @2 (#SymbolIdentifier @30-33: "foo")
      Node @5 (tag: kParenGroup) {
        Leaf @0 (#'(' @33-34: "(")
        Node @1 (tag: kPortDeclarationList) {
          Node @0 (tag: kPortDeclaration) {
            Leaf @0 (#"input" @34-39: "input")
            Node @2 (tag: kDataType) {
            }
            Node @3 (tag: kUnqualifiedId) {
              Leaf @0 (#SymbolIdentifier @40-41: "a")
            }
            Node @4 (tag: kUnpackedDimensions) {
            }
          }
          Leaf @1 (#',' @41-42: ",")
          Node @2 (tag: kPort) {
            Node @0 (tag: kPortReference) {
              Node @0 (tag: kUnqualifiedId) {
                Leaf @0 (#SymbolIdentifier @43-44: "b")
              }
            }
          }
          Leaf @1 (#',' @41-42: ",")
          Node @2 (tag: kPort) {
            Node @0 (tag: kPortReference) {
              Node @0 (tag: kUnqualifiedId) {
                Leaf @0 (#SymbolIdentifier @43-44: "b")
              }
            }
          }
          Leaf @3 (#',' @44-45: ",")
          Node @4 (tag: kPortDeclaration) {
            Leaf @0 (#"output" @46-52: "output")
            Node @2 (tag: kDataType) {
            }
            Node @3 (tag: kUnqualifiedId) {
              Leaf @0 (#SymbolIdentifier @53-54: "z")
            }
            Node @4 (tag: kUnpackedDimensions) {
            }
          }
        }
        Leaf @2 (#')' @54-55: ")")
      }
      Leaf @7 (#';' @55-56: ";")
    }
    Node @1 (tag: kModuleItemList) {
    }
    Leaf @2 (#"endmodule" @57-66: "endmodule")
    Node @3 (tag: kLabel) {
      Leaf @0 (#':' @67-68: ":")
      Leaf @1 (#SymbolIdentifier @69-72: "foo")
    }
  }
}
```

The `N` in `Node @N` or `Leaf @N` refers to the child rank of that node/leaf
with respect to its immediate parent node, starting at 0. `nullptr` nodes are
skipped and will look like gaps in the rank sequence.

Nodes of the CST may link to other nodes or leaves (which contain tokens). The
nodes are tagged with
[language-specific enumerations](../../CST/verilog_nonterminals.h). Each leaf
encapsulates a token and is shown with its corresponding byte-offsets in the
original text (as `@left-right`). Null nodes are not shown.

The exact structure of the SystemVerilog CST is fragile, and should not be
considered stable; at any time, node enumerations can be created or removed, and
subtree structures can be re-shaped. In the above example, `kModuleHeader` is an
implementation detail of a module definition's composition, and doesn't map
directly to a named grammar construct in the [SV-LRM]. The
[`verilog/CST`](../../CST) library provides functions that abstract away
internal structure.

<!-- reference links -->

[SV-LRM]: https://ieeexplore.ieee.org/document/8299595
