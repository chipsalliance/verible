---
---

# `verible-verilog-format`

Tool for formatting Verilog and SystemVerilog code. Part of the Verible tool
suite.

## Command line arguments
```
verible-verilog-format: usage: bazel-bin/verilog/tools/formatter/verible-verilog-format [options] <file> [<file...>]
To pipe from stdin, use '-' as <file>.

  Flags from external/com_google_absl/absl/flags/parse.cc:
    --flagfile (comma-separated list of files to load flags from); default: ;
    --fromenv (comma-separated list of flags to set from the environment [use
      'export FLAGS_flag1=value']); default: ;
    --tryfromenv (comma-separated list of flags to try to set from the
      environment if present); default: ;
    --undefok (comma-separated list of flag names that it is okay to specify on
      the command line even if the program does not define a flag with that
      name); default: ;


  Flags from external/com_google_absl/absl/flags/internal/usage.cc:
    --help (show help on important flags for this binary [tip: all flags can
      have two dashes]); default: false;
    --helpfull (show help on all flags); default: false; currently: true;
    --helpmatch (show help on modules whose name contains the specified substr);
      default: "";
    --helpon (show help on the modules named by this flag value); default: "";
    --helppackage (show help on all modules in the main package);
      default: false;
    --helpshort (show help on only the main module for this program);
      default: false;
    --only_check_args (exit after checking all flags); default: false;
    --version (show version and build info and exit); default: false;


  Flags from verilog/parser/verilog_parser.cc:
    --verilog_trace_parser (Trace verilog parser); default: false;


  Flags from verilog/tools/formatter/verilog_format.cc:
    --assignment_statement_alignment (Format various assignments:
      {align,flush-left,preserve,infer}); default: infer;
    --case_items_alignment (Format case items:
      {align,flush-left,preserve,infer}); default: infer;
    --class_member_variables_alignment (Format class member variables:
      {align,flush-left,preserve,infer}); default: infer;
    --failsafe_success (If true, always exit with 0 status, even if there were
      input errors or internal errors. In all error conditions, the original
      text is always preserved. This is useful in deploying services where
      fail-safe behaviors should be considered a success.); default: true;
    --formal_parameters_alignment (Format formal parameters:
      {align,flush-left,preserve,infer}); default: infer;
    --formal_parameters_indentation (Indent formal parameters: {indent,wrap});
      default: wrap;
    --inplace (If true, overwrite the input file on successful conditions.);
      default: false;
    --lines (Specific lines to format, 1-based, comma-separated, inclusive N-M
      ranges, N is short for N-N. By default, left unspecified, all lines are
      enabled for formatting. (repeatable, cumulative)); default: ;
    --max_search_states (Limits the number of search states explored during line
      wrap optimization.); default: 100000;
    --named_parameter_alignment (Format named actual parameters:
      {align,flush-left,preserve,infer}); default: infer;
    --named_parameter_indentation (Indent named parameter assignments:
      {indent,wrap}); default: wrap;
    --named_port_alignment (Format named port connections:
      {align,flush-left,preserve,infer}); default: infer;
    --named_port_indentation (Indent named port connections: {indent,wrap});
      default: wrap;
    --net_variable_alignment (Format net/variable declarations:
      {align,flush-left,preserve,infer}); default: infer;
    --port_declarations_alignment (Format port declarations:
      {align,flush-left,preserve,infer}); default: infer;
    --port_declarations_indentation (Indent port declarations: {indent,wrap});
      default: wrap;
    --show_equally_optimal_wrappings (If true, print when multiple optimal
      solutions are found (stderr), but continue to operate normally.);
      default: false;
    --show_inter_token_info (If true, along with show_token_partition_tree,
      include inter-token information such as spacing and break penalties.);
      default: false;
    --show_largest_token_partitions (If > 0, print token partitioning and then
      exit without formatting output.); default: 0;
    --show_token_partition_tree (If true, print diagnostics after token
      partitioning and then exit without formatting output.); default: false;
    --stdin_name (When using '-' to read from stdin, this gives an alternate
      name for diagnostic purposes. Otherwise this is ignored.);
      default: "<stdin>";
    --try_wrap_long_lines (If true, let the formatter attempt to optimize line
      wrapping decisions where wrapping is needed, else leave them unformatted.
      This is a short-term measure to reduce risk-of-harm.); default: false;
    --verbose (Be more verbose.); default: false;
    --verify_convergence (If true, and not incrementally formatting with
      --lines, verify that re-formatting the formatted output yields no further
      changes, i.e. formatting is convergent.); default: true;
```

## Version

Generated on 2021-04-20 07:43:08 -0700 from [6b827de](https://github.com/google/verible/commit/6b827debc9dbd7da2d1a4adad8a16113e0083ac0)
