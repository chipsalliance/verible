# Parser Library

<!--*
freshness: { owner: 'hzeller' reviewed: '2020-10-04' }
*-->

This language-agnostic library contains adapters and helpers for working with
Yacc/Bison-generated parsers. Such adapters includes being able to accept tokens
from sources that are not directly coming from a lexer function. Such
flexibility enables intermediate processing on token streams before parsing
them.

See the [text](../text) library for syntax tree data structures.
