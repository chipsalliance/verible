---
---

# Verible

[![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)
[![Continuous Integration](https://github.com/chipsalliance/verible/workflows/verible-ci/badge.svg)](https://github.com/chipsalliance/verible/actions/workflows/verible-ci.yml)
[![codecov](https://codecov.io/gh/chipsalliance/verible/branch/master/graph/badge.svg?token=5f656dpmDT)](https://codecov.io/gh/chipsalliance/verible)

<!--*
freshness: { owner: 'hzeller' reviewed: '2020-10-08' }
*-->

The Verible project's main mission is to parse SystemVerilog (IEEE 1800-2017)
(as standardized in the [SV-LRM]) for a wide variety of applications, including
developer tools.

It was born out of a need to parse *un-preprocessed* source files, which is
suitable for single-file applications like style-linting and formatting. In
doing so, it can be adapted to parse *preprocessed* source files, which is what
real compilers and toolchains require.

The spirit of the project is that no-one should ever have to develop a
SystemVerilog parser for their own application, because developing a
standard-compliant parser is an enormous task due to the syntactic complexity of
the language. Verible's parser is also regularly tested against an ever-growing
suite of (tool-independent) language compliance tests at
https://symbiflow.github.io/sv-tests/.

A lesser (but notable) objective is that the language-agnostic components of
Verible be usable for rapidly developing language support tools for other
languages.


See the [README file for further information.](README.md)

## Tools

 * [verible-verilog-lint Info](verilog_lint.md)
 * [verible-verilog-format Info](verilog_format.md)
 * [verible-verilog-syntax Info](verilog_syntax.md)

## Information

 * [Code - https://github.com/google/verible](https://github.com/google/verible)
 * [Binaries - https://github.com/google/verible/releases](https://github.com/google/verible/releases)
 * [Bug Reports - https://github.com/google/verible/issues/new](https://github.com/google/verible/issues/new)
 * [Lint Rules](lint.md)
 * [Further Information](README.md)

## Authors

 * [Contributing](CONTRIBUTING.md)
 * [Authors](AUTHORS.md)
 * [License Information](license.md)

## Version

Generated on 2021-09-27 09:17:17 -0700 from [0bf7e75d](https://github.com/google/verible/commit/0bf7e75dc61f480fecf5eb05775235321c3ac7e3)
