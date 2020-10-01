# Verible SystemVerilog Parser Design

Verible uses traditional tools like Flex and Yacc/Bison, but *not* in the usual
manner where the generated `yyparse()` function calls `yylex()` directly.
Instead, the generated lexer and parser are completely decoupled.

## Lexer

The lexer is generated using Flex. It supports both SystemVerilog
language-proper lexical tokens as well as preprocessing directives, macros, and
other directives, all-in-one. The Flex-generated code is then wrapped into an
interface that returns tokens one-by-one, until ending with a special EOF token.

## Contextualizer

The contextualizer is a pass over the token stream produced by the lexer that
can help disambiguate tokens with multiple interpretations. This in turn, helps
simplify the grammar in the parser implementation in ways that would not be
possible with a context-free LR parser alone.

## Preprocessor

There is no standalone preprocessor yet.

## Filter

Token streams can be filtered into token stream views in various ways. One of
the most useful filters hides comments and attributes so that the resulting view
can be parsed.

## Parser

The parser is generated using Bison, using the LALR(1) algorithm. The grammar
implemented is unconventional in that it explicitly handles preprocessing
constructs, directives, macros -- this allows it to operate on a
limited-but-useful subset of *unpreprocessed* code. The parser constructs a
concrete syntax tree (CST) that captures all non-comment tokens, including
punctuation and syntactic sugar.
