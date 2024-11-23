# SystemVerilog Preprocessor

<!--*
freshness: { owner: 'hzeller' reviewed: '2020-10-04' }
*-->

Since early applications such as the style linter and formatter operate on
_unpreprocessed_ source code, preprocessing constructs were incorporated
directly into the [grammar](../parser/verilog.y) with limitations.

## Pseudo Preprocessing

Pseudo-preprocessing refers to any number of strategies that perform partial or
approximate preprocessing, by making assumptions, such as whether conditional
branches are taken or not taken, or expanding macros to the extent where local
definitions are available. The outcome of these "transformations" is a modified
view of the original text that could result in covering different preprocessing
code paths (blanking out not-taken branches), which could result in different
lexing and parsing results. Any number of strategies could be used to overcome
the limitations of preprocessing support in the parser, which in turn improves
the outreach of tools like the linter and formatter.

Most of these are not yet implemented, but
[help is wanted](https://github.com/chipsalliance/verible/issues/183).

## Standard-Compliant SV Preprocessor

... does not exist in this codebase yet. See
https://github.com/chipsalliance/verible/issues/183.

SystemVerilog preprocessing is in many ways
[more complicated than C/C++ preprocessing](https://www.veripool.org/papers/Preproc_Good_Evil_SNUGBos10_paper.pdf).
