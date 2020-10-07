# SystemVerilog Analysis Library

<!--*
freshness: { owner: 'fangism' reviewed: '2020-10-04' }
*-->

This directory contains libraries for analyzing SystemVerilog code. Analysis is
often done on the token stream and syntax tree from the lexer and parser, as
read-only operations.

## Partial File or Single Construct Parsing

For cases where one wishes to parse only subset of the grammar, e.g.
module-body-items, we provide
[wrappers around the full-parser](verilog_excerpt_parse.h). This approach
maximizes reuse of the full language parser without having to generate or slice
out separate grammars, and guarantees consistent concrete syntax tree node
enumerations.

## Lint Rules

Individual lint rules live under [checkers](./checkers). The
[global lint rule registry](lint_rule_registry.h) manager the entire collection
of lint rules. The [lint analysis driver](verilog_linter.h) traverses the
structural representation of the code-under-analysis as
[configured](verilog_linter_configuration.h).

The default-enabled set of lint rules is maintained [here](default_rules.h).

Linter tool documentation can be found [here](../toosl/lint).

## Equivalence Checking

[Lexical equivalence checking](verilog_equivalence.h) verifies that two Verilog
source files are equivalent under various conditions.
