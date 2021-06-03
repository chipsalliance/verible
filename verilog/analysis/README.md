# SystemVerilog Analysis Library

<!--*
freshness: { owner: 'hzeller' reviewed: '2020-10-15' }
*-->

This directory contains libraries for analyzing SystemVerilog code. Analysis is
often done on the token stream and syntax tree from the lexer and parser, as
read-only operations.

[Linter user documentation can be found here](../tool/lint).

## Partial File or Single Construct Parsing

For cases where one wishes to parse only subset of the grammar, e.g.
module-body-items, we provide
[wrappers around the full-parser](verilog_excerpt_parse.h). This approach
maximizes reuse of the full language parser without having to generate or slice
out separate grammars, and guarantees consistent concrete syntax tree node
enumerations.

### Alternative Parsing Modes

Source files that are known to be indented for inclusion in another syntactic
context may trigger an alternative parsing mode with comment directives. These
directives are understood by tools including the style-linter and formatter.

The following example would normally fail to compile because wire declarations
can only occur inside modules:

```verilog
// This file is `included inside a module definition body.
// verilog_syntax: parse-as-module-body
wire x;
initial begin
  x = 0;
end
```

However, the `verilog_syntax` directive tells the parser (in this case) that the
contents of this file are to be treated as if they are inside a module
declaration. Scanning for this directive stops as soon as any
non-preprocessing-control token is encountered (after `` `ifndef`` or comments
is ok, but not after any keyword like `module`, for instance).

Available parser modes include:

*   `parse-as-statements`: Text contains only statements (e.g. inside function,
    always, ...)
*   `parse-as-expression`: Text occurs inside an expression.
*   `parse-as-module-body`: Text occurs inside a module definition.
*   `parse-as-class-body`: Text occurs inside a class definition.
*   `parse-as-package-body`: Text occurs inside a package definition.
*   `parse-as-library-map`: Verilog library map sub-language (LRM: Ch. 33).

## Lint Rules

Individual lint rules live under [checkers](./checkers). The
[global lint rule registry](lint_rule_registry.h) manager the entire collection
of lint rules. The [lint analysis driver](verilog_linter.h) traverses the
structural representation of the code-under-analysis as
[configured](verilog_linter_configuration.h).

The default-enabled set of lint rules is maintained [here](default_rules.h).

Linter tool user documentation can be found [here](../toosl/lint).

## Equivalence Checking

[Lexical equivalence checking](verilog_equivalence.h) verifies that two Verilog
source files are equivalent under various conditions.

[Diff tool user documentation can be found here](../tool/diff).
