# Text Structural Representation Libraries

<!--*
freshness: { owner: 'hzeller' reviewed: '2020-10-04' }
*-->

At the heart of language-tooling libraries and applications lie various
structural representations of text, and the functions that operate on them. This
directory contains _language-agnostic_ data structures like:

*   Tokens: annotated substrings of a body of text, often what a lexer produces.
    *   Token streams: iterable representations of lexer output, including
        filtered views thereof.
*   Syntax trees: represent how parsers understand and organize code
    hierarchically.

## Key Concepts

`absl::string_view`s do not just represent text, but they represent **position**
within a larger body of text, by virtue of comparing their `begin` and `end`
bounds. This concept is leveraged heavily to avoid unnecessary string copying. A
base `string_view` that represents a body of text and serve as the basis for
interchanging between substring-views and byte-offsets relative to the start of
the base.
