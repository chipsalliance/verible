---
---

# `verible-verilog-syntax`

Tool for looking at the syntax of Verilog and SystemVerilog code. Part of the
Verible tool suite.

## Command line arguments
```
verible-verilog-syntax: usage: bazel-bin/verilog/tools/syntax/verible-verilog-syntax [options] <file> [<file>...]

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


  Flags from verilog/tools/syntax/verilog_syntax.cc:
    --error_limit (Limit the number of syntax errors reported. (0: unlimited));
      default: 0;
    --export_json (Uses JSON for output. Intended to be used as an input for
      other tools.); default: false;
    --lang (Selects language variant to parse. Options:
      auto: SystemVerilog-2017, but may auto-detect alternate parsing modes
      sv: strict SystemVerilog-2017, with explicit alternate parsing modes
      lib: Verilog library map language (LRM Ch. 33)
      ); default: auto;
    --printrawtokens (Prints all lexed tokens, including filtered ones.);
      default: false;
    --printtokens (Prints all lexed and filtered tokens); default: false;
    --printtree (Whether or not to print the tree); default: false;
    --show_diagnostic_context (prints an additional line on which the diagnostic
      was found,followed by a line with a position marker); default: false;
    --verifytree (Verifies that all tokens are parsed into tree, prints
      unmatched tokens); default: false;
```

## Version

Generated on 2021-04-14 12:26:30 -0700 from [47fbcbb](https://github.com/google/verible/commit/47fbcbb96a51d07e393716817b1d0ca373eaa15b)
