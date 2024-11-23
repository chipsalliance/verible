# SystemVerilog Concrete Syntax Tree

<!--*
freshness: { owner: 'hzeller' reviewed: '2020-10-04' }
*-->

The SystemVerilog concrete syntax tree (CST) uses the language-agnostic syntax
tree structure with its own set of (`int`) enumerations for tree nodes and
leaves. The CST includes all syntactically relevant tokens, no comments, no
attributes (at this time), and limited support for preprocessing constructs.

The exact [node enumerations](verilog_nonterminals.h) should be considered
fragile until stated otherwise; they may change, get new enumerations, or remove
obsolete ones. Code that depends direct use of these enumerations should be
well-tested so that breakages are easy to diagnose and fix.

CST leaves contain tokens, which bear the token enumerations generated from the
[parser implementation](../parser/verilog.y). These token enumerations are
relatively stable. However, where practical, we encourage use of
[token classification functions](../parser/verilog_token_classifications.h).

The node enumerations are used directly in the semantic actions of the
[SystemVerilog parser](../parser/verilog.y), with functions like
`MakeTaggedNode`.

Both node and token enumerations are used in syntax tree analyzers and also
drive formatting decisions in the formatter.

## Ideals

_Ideal_ properties of CST nodes:

*   Every construction of a CST node follows a **consistent** substructure, as
    if there were a class with one constructor for each node type. Consistency
    allows one to write simple functions that directly access substructure by
    descending through CST nodes positionally. For every node enumeration
    `kFoo`, there should be `MakeFoo` function that constructs a CST node from
    its arguments. Accessor functions should be short and composable.
    *   Construction also provides an opportunity to check that the programmer
        has not made a mistake, by asserting invariant properties about the
        arguments.
*   Access into a CST node's substructure is **exclusively** done by
    `GetFooFromBar`-style functions that hide the structural details of a node,
    while remaining consistent with construction.
*   Both implementation of the constructor and accessor functions should ideally
    come from a _single-source-of-truth_, that is, they should be **generated**
    (no later than compile-time) from one specification for each node type,
    rather than maintained independently.

This is not the case today because of the haste in which initial development
took place, but help is wanted towards achieving the aforementioned ideals. See
also https://github.com/chipsalliance/verible/issues/159.

## Abstract Syntax Tree?

_Wouldn't an abstract syntax tree (AST) satisfy the above ideals?_ Yes, this
would take time to write, and we would need
[help](https://github.com/chipsalliance/verible/issues/184).

An AST may not be a great representation for _unpreprocessed_ code, which is the
focus of the first developer tool applications. Having a
[standard-compliant SV preprocessor](https://github.com/chipsalliance/verible/issues/183)
would pave the way to making an AST more useful.

## Testing

Most CST accessor function tests should follow this outline:

*   Declare an array of test data in the form of
    [SyntaxTreeSearchTestCase](https://cs.opensource.google/verible/verible/+/master:verible/common/analysis/syntax-tree-search-test-utils.h)
    *   Each element compactly represents the code to analyze, and the set of
        expected findings as annotated subranges of text.
*   For every function-under-test, establish a function that extracts the
    targeted subranges of text (which must be non-overlapping). This could be a
    simple find-function on a syntax tree or contain any sequence of search
    refinements.
*   Pass these into the
    [TestVerilogSyntaxRangeMatches](https://cs.opensource.google/verible/verible/+/master:verible/verilog/CST/match-test-utils.h)
    test driver function which compare actual vs. expected subranges.
