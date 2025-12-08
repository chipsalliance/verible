# SV Style Linter Developer Guide

<!--*
freshness: { owner: 'hzeller' reviewed: '2020-10-28' }
*-->

This document describes how to implement style lint rules.

Before you begin, familiarize yourself with:

*   [General contribution guidelines](../CONTRIBUTING.md)
*   [General developer resources](development.md)

## Communication

*   Join the developer's mailing list: verible-dev@googlegroups.com
    ([join](https://groups.google.com/forum/#!forum/verible-dev/join))
*   Use the github issue tracker, discuss and comment:
    *   [existing lint rule issues](https://github.com/chipsalliance/verible/issues?q=is%3Aissue+is%3Aopen+label%3Astyle-linter)
    *   [file new issue about existing rule](https://github.com/chipsalliance/verible/issues/new?assignees=&labels=style-linter&template=style-linter-bug.md&title=)
    *   [file new issue about a new rule](https://github.com/chipsalliance/verible/issues/new?assignees=&labels=enhancement%2C+style-linter&template=style-linter-feature-request.md&title=)

## Whose Style Guide?

Whose style guide serves as a reference for lint rules? Everyone and anyone's.
Every team may set its own guidelines on what constitutes correct style. The
style linter hosts an ever-growing library of rules, but _you_ decide
[which rules or configurations](../verible/verilog/tools/lint.md#usage) best suit your
project.

### Traits of good lint rules

*   Identify error-prone constructs, including those that may lead to production
    bugs
*   Have few exceptions, low risk of false positives, and low frequency of lint
    waivers
*   Reduces the number of choices for ways-to-express-a-concept
*   Rules that enforce [SV-LRM] conformance should be implemented in actual
    compilers, but sometimes can be enforced in style linters as well.

## Types of Analyses

The major classes of text analyses available today are:

*   [LineLintRule] analyzes text one line at a time. Examples:

    *   [no_trailing_spaces_rule](https://cs.opensource.google/verible/verible/+/master:verible/verilog/analysis/checkers/no-trailing-spaces-rule.h)
    *   [no_tabs_rule](https://cs.opensource.google/verible/verible/+/master:verible/verilog/analysis/checkers/no-tabs-rule.h)

*   [TokenStreamLintRule] scans one token at a time. Examples:

    *   [endif_comment_rule](https://cs.opensource.google/verible/verible/+/master:verible/verilog/analysis/checkers/endif-comment-rule.h)
        and
    *   [macro_name_style_rule](https://cs.opensource.google/verible/verible/+/master:verible/verilog/analysis/checkers/macro-name-style-rule.h)

*   [SyntaxTreeLintRule] analyzes the syntax trees, examining tree nodes and
    leaves. The
    [vast majority of SystemVerilog lint rules](https://cs.opensource.google/verible/verible/+/master:verible/verilog/analysis/checkers)
    fall under this category. The [Analysis Tools](#syntax-tree-analysis-tools)
    section describes various syntax tree analysis tools.

*   [TextStructureLintRule] analyzes an entire [TextStructureView] in any
    manner. This is the most flexible analyzer that can access all of the
    structured views of the analyzed text. It is best suited for rules that:

    *   need access to **more than one** of the following forms: lines, tokens,
        syntax-tree
    *   can be implemented efficiently without having to traverse any of the
        aforementioned forms.

    Examples:
    [module_filename_rule](https://cs.opensource.google/verible/verible/+/master:verible/verilog/analysis/checkers/module-filename-rule.h?q=class:%5CbModuleFilenameRule),
    [line_length_rule](https://cs.opensource.google/verible/verible/+/master:verible/verilog/analysis/checkers/line-length-rule.h?q=class:%5CbLineLengthRule),
    and
    [posix_eof_rule](https://cs.opensource.google/verible/verible/+/master:verible/verilog/analysis/checkers/posix-eof-rule.h?q=class:%5CbPosixEOFRule).

For complete links to examples of each of the above lint rule classes, click on
the class definition and navigate to "Extended By" inside the "Cross References"
panel in the code search viewer.

## Syntax Tree Analysis Tools

This section describes various tools and libraries for analyzing and querying
syntax trees.

### Syntax Tree Examiner

Use [`verible-verilog-syntax --printtree`](../verible/verilog/tools/syntax) to examine
the syntax structure of examples of code of interest.

### Syntax Tree Visitors

[TreeContextVisitor] is a syntax tree visitor that maintains a stack of ancestor
nodes in a stack as it traverses nodes and leaves. This is useful for being able
to query the stack to determine the context at any node.

### Syntax Tree Direct Substructure Access

The [SV concrete syntax tree (CST) is described here](../verible/verilog/CST). The CST
library contains a number of useful `GetXFromY`-type accessor functions. These
functions offer the most direct way of extracting information from syntax tree
nodes. Accessor functions are useful when you've already narrowed down your
search to one or a few specific types (enums) of syntax tree nodes.

Pros:

*   Fast, because no search is involved.

Cons:

*   You may have to write some new CST accessor functions if what you need
    doesn't already exist.

### Syntax Tree Searching

[SearchSyntaxTree](https://cs.opensource.google/verible/verible/+/master:verible/common/analysis/syntax-tree-search.h)
is a generic search function for identifying all syntax tree nodes that satisfy
a given predicate. Searching with this function yields TreeSearchMatch objects
that point to syntax tree nodes/leaves and include the context in which the node
matched.

Pros:

*   Good for finding nodes when you don't know (or don't care) where in a
    subtree they could appear.
*   More resilient (but not immune) to CST restructuring.

Cons:

*   Slower than direct access because a search always visits every subnode and
    leaf.

### Syntax Tree Pattern Matching

The
[CST Matcher library](https://cs.opensource.google/verible/verible/+/master:verible/common/analysis/matcher/)
provides a convenient way to create matcher objects that describe certain
syntactic patterns.

The Syntax Tree Matcher library uses some principles from
[Clang's ASTMatcher Library](http://clang.llvm.org/docs/LibASTMatchersReference.html).

Pros:

*   Expressive and composeable.
*   More resilient to CST positional substructure changes such as changing child
    ranks.

Cons:

*   Can be expensive due to searching.

### Single Node Type Matchers

For every [SystemVerilog CST node enum](../verible/verilog/CST/verilog_nonterminals.h),
we produce a corresponding node-matcher in [verilog_matchers.h] that finds that
node type. For example, `NodekFunctionDeclaration` matches nodes tagged
`kFunctionDeclaration`. These are defined using [TagMatchBuilder].

### Path-based Matchers

Path matchers are a shorthand for expressing a match on an ancestral chain of
node types. For example, `MakePathMatcher({X, Y, Z})`, where `X`, `Y`, and `Z`
are CST tags for specific nodes types, creates a matcher that will find nodes of
type `X` that directly contain a `Y` child that directly contains a `Z` child.
[verilog_matchers.h] contains several examples.

### Composition

Every matcher object can accept inner matchers that can refine matching
conditions and narrow search results. Composing matchers looks like
`OuterMatcher(InnerMatcher(...), ...)`, which would return a positive match on a
node that matches `OuterMatcher`, whose subtree also satisfies `InnerMatcher`.

Matcher _operators_ are functions described in [core_matchers.h].

Summary:

*   `AllOf(...)` matches positively if _all_ of its inner matchers positively.
*   `AnyOf(...)` matches positively if _any_ of its inner matchers positively.
*   `Unless(...)` matches positively if its inner matcher does **not** to match.

[TagMatchBuilder]s by default combines its inner matchers with `AllOf`, so can
write `NodekFoo(InnerMatcher1(), InnerMatcher2())` instead of the equivalent
`NodekFoo(AllOf(InnerMatcher1(), InnerMatcher2()))`.

The order of the inner matchers to the above functions is inconsequential to the
match result; they are fully commutative.

### Named Binding of Symbols

Many matchers support _binding_ to user-provided names called
[BindableMatchers]. This lets you save interesting subtree positions found
during the match and retrieve them from a [BoundSymbolManager].
[Example using `.Bind()`](../verible/verilog/analysis/checkers/undersized_binary_literal_rule.h).

## Reporting Positive Findings

When you've determined that the code being analyzed matches a pattern of
interest, record a [LintViolation] object.

Narrow down the location of the offending substring of text as much as possible
so that users can see precisely what is wrong. You can select a whole block of
text, a syntax node subtree, a single token, or even a substring within a token.

Include a diagnostic message that describes the problem, citing a passage from a
style guide. Recommend a corrective action where you can.

Each lint rule that analyzes code produces a [LintRuleStatus] that contains a
set of [LintViolations].

## Lint Rule Configuration

## Testing

A typical set of lint rule tests follow this template:

```c++
TEST(LintRuleNameTest, Various) {
  const std::initializer_list<LintTestCase> kTestCases = {
    ...
  };
  RunLintTestCases<VerilogAnalyzer, LintRuleName>(kTestCases);
}
```

Each [LintTestCase] is built from mix of plain string literals and tagged string
literals that markup where findings are to be expected.

```c++
  {"uninteresting text ",
  {kSymbolType, "interesting text"},  // expect a finding over this substring
  "; uninteresting text"},
```

The test driver converts this into input code to analyze and a set of expected
findings. A test will fail if the actual findings do not match the expected ones
_exactly_ (down to their locations).

Make sure to include negative tests that expect no lint violations.

## Exercises

<!-- reference links -->

[SV-LRM]: https://ieeexplore.ieee.org/document/8299595
[LineLintRule]: https://cs.opensource.google/verible/verible/+/master:verible/common/analysis/line-lint-rule.h
[TokenStreamLintRule]: https://cs.opensource.google/verible/verible/+/master:verible/common/analysis/token-stream-lint-rule.h
[SyntaxTreeLintRule]: https://cs.opensource.google/verible/verible/+/master:verible/common/analysis/syntax-tree-lint-rule.h
[TextStructureLintRule]: https://cs.opensource.google/verible/verible/+/master:verible/common/analysis/text-structure-lint-rule.h
[TextStructureView]: https://cs.opensource.google/verible/verible/+/master:verible/common/text/text-structure.h
[TreeContextVisitor]: https://cs.opensource.google/verible/verible/+/master:verible/common/text/tree-context-visitor.h
[LintViolation]: https://cs.opensource.google/verible/verible/+/master:verible/common/analysis/lint-rule-status.h?q=class:%5CbLintViolation%5Cb&ss=verible%2Fverible
[LintRuleStatus]: https://cs.opensource.google/verible/verible/+/master:verible/common/analysis/lint-rule-status.h?q=class:%5CbLintRuleStatus%5Cb&ss=verible%2Fverible
[LintTestCase]: https://cs.opensource.google/verible/verible/+/master:verible/common/analysis/linter-test-utils.h?q=class:%5CbLintTestCase%5Cb&ss=verible%2Fverible
[core_matchers.h]: https://cs.opensource.google/verible/verible/+/master:verible/common/analysis/matcher/core-matchers.h
[verilog_matchers.h]: https://cs.opensource.google/verible/verible/+/master:verible/verilog/CST/verilog-matchers.h
[TagMatchBuilder]: https://cs.opensource.google/verible/verible/+/master:verible/common/analysis/matcher/matcher-builders.h?q=class:%5CbTagMatchBuilder%5Cb%20&ss=verible%2Fverible
[BindableMatcher]: https://cs.opensource.google/verible/verible/+/master:verible/common/analysis/matcher/matcher.h?q=class:%5CbBindableMatcher%5Cb%20&ss=verible%2Fverible
[BoundSymbolManager]: https://cs.opensource.google/verible/verible/+/master:verible/common/analysis/matcher/bound-symbol-manager.h?q=class:BoundSymbolManager&ss=verible%2Fverible
