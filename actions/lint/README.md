# `SymbiFlow/actions/checks@main` - GitHub Action for basic SymbiFlow checks.

This GitHub Actions performs a number of basic checks against a repository.

# Using

```yaml

jobs:
  SymbiFlowBasicChecks:
    runs-on: ubuntu-latest

    steps:
    - name: Getting code
      uses: actions/checkout@v2
      with:
        submodules: recursive

    - name: SystemVerilog Lint
      uses: google/verible/actions/lint@main
```

# Checks

The following checks are performed.

## Files

The checks are currently performed on the following files;

 - Python files - `*.py`
 - Yaml files - `*.yaml` & `*.yml`
 - make files - `Makefile` and `*.mk`
 - Shell/Bash files - `*.sh`

## License Checks


### Excluding files

Files can be excluded from the license checks using the `exclude_license`
input.

### Third Party License Files

Check all directories in _third party directories_ have license files. Looks
for `LICENSE`, `LICENCE` or `COPYING` files.

Which directories are considered _third party directories_ can be configured
with `third_party` input, default is directories named `third_party`.

### SPDX Identifiers in Text Files

Checks that files have a `SPDX-License-Identifier` value in the header.

## Python Checks

### Excluding files

Files can be excluded from the Python checks using the `exclude_python` input.

### Shebang

Checks the shebang line is **exactly** `#!/usr/bin/env python3`.

### UTF-8 Coding line

Checks the second line in a Python file has the `coding: utf-8` statement (to
force Python and editors into UTF-8) mode.
