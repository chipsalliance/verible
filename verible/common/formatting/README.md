# Formatting Library

<!--*
freshness: { owner: 'hzeller' reviewed: '2020-10-04' }
*-->

This library contains language-agnostic formatting capabilities, including
representations useful for formatting algorithms.

## Key Data Structures

[PreFormatToken](format_token.h) is a markup layer over a token stream that
annotates each token with [InterTokenInfo](format_token.h) that influences
formatting (e.g. minimum spacing between tokens).

[FormattedToken](format_token.h) is similar, but represents the result of
formatting, **after** analysis and transformations have completed and decisions
have been bound.

[FormatTokenRange](format_token.h) is an iterator range over an array of
PreFormatTokens.

[UnwrappedLine](unwrapped_line.h) represents an indentable unit of formatting
and contains a FormatTokenRange and indentation information.

[TokenPartitionTree](token_partition_tree.h) is a language-agnostic hierarchical
representation of subranges of tokens and text. The data nodes of this tree are
UnwrappedLines. From any language-specific text and structural representation,
producing a language-agnostic TokenPartitionTree will gain access to a wide
variety of formatting functions.

## Concepts

Don't mutate text/syntax tree data structures, present a form that looks like a
mutation and render it. What the formatting functions do is produce annotated
scaffoling over the original representation that, when printed to stream,
produces the same effect as having mutated the original form. By treating the
orignal representation as read-only, this strategy allows one to easily explore
multiple formatting subsolutions.
