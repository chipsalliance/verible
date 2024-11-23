# Development Resources

This document will help getting started [contributing](../CONTRIBUTING.md) to
Verible. Collecting development aids and design concepts.

## Searching and Navigating Verible's Source Code

https://cs.opensource.google/verible/verible is a search-indexed mirror of
[Verible's source code](https://github.com/chipsalliance/verible). Expect to spend a
lot of time here as you familiarize yourself with the codebase.

https://developers.google.com/code-search/reference provides a reference for
search syntax and more.

To learn more about how to use Kythe to
[index the source code yourself, read here](./indexing.md).

### Tips

*   Read the test code. Most `.h` and `.cc` files come with a `_test.cc` set of
    unit tests. The tests are never outdated because they are kept passing.
*   Find examples. Dig through history. Follow blame and annotation layers to
    see when particular lines of code were touched. Look for related closed
    issues and see the commits that addressed them.

## Code Organization

Each directory in the source tree contains a short README.md describing the
contents.

*   [common/](../verible/common) contains all language-agnostic libraries and tools
*   [verilog/](../verible/verilog) contains Verilog-specific libraries and tools
*   [external_libs/](../external_libs) contains some library dependencies

## Verilog Front-End

*   [Lexer and Parser design](./parser_design.md)

## Analyzers

*   [How to implement lint rules](./style_lint.md)

## Transformers

## Formatting

*   [Formatter](./foramtter.md): How the formatter works, and how to debug it.
