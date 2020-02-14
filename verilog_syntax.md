---
---

# `verilog_syntax`

Tool for looking at the syntax of Verilog and SystemVerilog code. Part of the
verible tool suite.

## Command line arguments
```
verilog_syntax: usage: bazel-bin/verilog/tools/syntax/verilog_syntax [options] <file> [<file>...]

  Flags from external/com_google_absl/absl/flags/parse.cc:
    -flagfile (comma-separated list of files to load flags from); default: ;
    -fromenv (comma-separated list of flags to set from the environment [use
      'export FLAGS_flag1=value']); default: ;
    -tryfromenv (comma-separated list of flags to try to set from the
      environment if present); default: ;
    -undefok (comma-separated list of flag names that it is okay to specify on
      the command line even if the program does not define a flag with that
      name); default: ;


  Flags from external/com_google_absl/absl/flags/internal/usage.cc:
    -help (show help on important flags for this binary [tip: all flags can have
      two dashes]); default: false;
    -helpfull (show help on all flags); default: false; currently: true;
    -helpmatch (show help on modules whose name contains the specified substr);
      default: "";
    -helpon (show help on the modules named by this flag value); default: "";
    -helppackage (show help on all modules in the main package); default: false;
    -helpshort (show help on only the main module for this program);
      default: false;
    -only_check_args (exit after checking all flags); default: false;
    -version (show version and build info and exit); default: false;


  Flags from verilog/parser/verilog_parser.cc:
    -verilog_trace_parser (Trace verilog parser); default: false;


  Flags from verilog/tools/syntax/verilog_syntax.cc:
    -printrawtokens (Prints all lexed tokens, including filtered ones.);
      default: false;
    -printtokens (Prints all lexed and filtered tokens); default: false;
    -printtree (Whether or not to print the tree); default: false;
    -verifytree (Verifies that all tokens are parsed into tree, prints unmatched
      tokens); default: false;
```

## Version

Generated on 2020-02-14 12:21:18 -0800 from [v0.0-213-gf78b424](https://github.com/google/verible/commit/f78b4245d54dfd0941c05bfe232a02ec5df31516)
