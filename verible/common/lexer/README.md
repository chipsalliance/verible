# Lexer Library

<!--*
freshness: { owner: 'hzeller' reviewed: '2020-10-04' }
*-->

This directory contains language-agnostic support libraries for working with
(Flex-generated) lexers. This library provides alternative interfaces for
working with lexer data, such as adapting them into generator functions, or
producing token streams. These adapters extend the use of lexers beyond direct
integrating into parsers.

See the [text](../text) library for token data structures.
