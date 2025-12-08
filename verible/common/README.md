# Verible's Language-Agnostic Core Library

<!--*
freshness: { owner: 'hzeller' reviewed: '2020-10-04' }
*-->

The libraries provided herein are _language-agnostic_ and have nothing to do
with Verilog or SystemVerilog.

## Subdirectory Summary

*   [util](./util): generic algorithms, data structures, patterns
*   [strings](./strings): functions that operate purely on strings
*   [lexer](./lexer): lexer interfaces and adaptors that produce token streams
*   [parser](./parser): parser interfaces and adaptors that build syntax trees
    out of tokens
*   [text](./text): structural representations of source code (tokens, lines,
    trees, etc.)
*   [analysis](./analysis): various analysis and query facilities on text
    structures
*   [formatting](./formatting): reusable source code formatting operations
