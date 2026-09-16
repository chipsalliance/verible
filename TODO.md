# Kalen's TODO File

This is Kalen's private TODO file. Don't attempt to upstream this.

## Formatter issues to volunteer

These are the next three issues to take after the current formatter PRs land
([#2545](https://github.com/chipsalliance/verible/pull/2545),
[#2543](https://github.com/chipsalliance/verible/pull/2543),
[#2546](https://github.com/chipsalliance/verible/pull/2546),
[#2548](https://github.com/chipsalliance/verible/pull/2548)).

Samples were re-run on `origin/master` (`1ea007ec`, `v0.0-4148-g1ea007ec`).

### 1. Crash on `wire signed` — [#2008](https://github.com/chipsalliance/verible/issues/2008)

Also covers [#2474](https://github.com/chipsalliance/verible/issues/2474) and
likely [#2063](https://github.com/chipsalliance/verible/issues/2063).

The formatter **aborts** on `input wire signed [...]`. Five-line sample, same
`tree-unwrapper.cc` CHECK. Highest new-issue value: crash, tiny repro, one family.

Claim **#2008** and mark #2474 / #2063 as duplicates once they share the same abort.

### 2. Long function header drops code — [#886](https://github.com/chipsalliance/verible/issues/886)

Wrapping a long function header **drops the function parameters**. Still happens
on current master: the formatted output is not even valid SystemVerilog. Worse
than a style bug.

Draft PR: [#2578](https://github.com/chipsalliance/verible/pull/2578)
(`kbrunham-intel/fix/886`).

### 3. Spaces inserted in `*nix` paths — [#2352](https://github.com/chipsalliance/verible/issues/2352)

Turns `` `PATH_MY_MODULE/src/config_class.sv `` into
`` `PATH_MY_MODULE / src / config_class.sv ``, which **does not compile**.
Small, well-scoped.

Draft PR: [#2577](https://github.com/chipsalliance/verible/pull/2577)
(`kbrunham-intel/fix/2352`).

## Issues I created

### `formatter_test.cc` is too large — [#2579](https://github.com/chipsalliance/verible/issues/2579)

`verible/verilog/formatting/formatter_test.cc` is ~21k lines. Split by concern
(macros, alignment, wrapping, …) so formatter PRs stop colliding at the end of
one file.

### `run-format.sh` should be a no-op on master — [#2580](https://github.com/chipsalliance/verible/issues/2580)

Running `.github/bin/run-format.sh` currently rewrites files that were already
on master. Land a format-only cleanup so the tree matches CI style.

### Other open formatter issues

- [#2539](https://github.com/chipsalliance/verible/issues/2539) — CHECK abort on line-continuation before aligned ports. PR [#2546](https://github.com/chipsalliance/verible/pull/2546).
- [#2542](https://github.com/chipsalliance/verible/issues/2542) — non-convergence with continuation EOL comments. PR [#2543](https://github.com/chipsalliance/verible/pull/2543).
- [#2544](https://github.com/chipsalliance/verible/issues/2544) — MacroIdentifier vs MacroIdItem at EOL. PR [#2545](https://github.com/chipsalliance/verible/pull/2545).
- [#2547](https://github.com/chipsalliance/verible/issues/2547) — non-convergence wrapping multi-line macro sums. PR [#2548](https://github.com/chipsalliance/verible/pull/2548).
