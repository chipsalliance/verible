# SystemVerilog Project Multi-tool

<!--*
freshness: { owner: 'hzeller' reviewed: '2020-12-21' }
*-->

`verible-verilog-project` is a multi-tool that operates on whole Verilog
projects, consisting of a file list and related configurations. This serves as a
diagnostic tool for analyzing (and potentially transforming) project-level
sources.

## Usage

```
verible-verilog-project COMMAND [options...]

available commands:
  file-deps
  help
  symbol-table-defs
  symbol-table-refs

  Flags from verilog/tools/project/project_tool.cc:
    --file_list_path (The path to the file list which contains the names of
      SystemVerilog files.
      The files should be ordered by definition dependencies.); default: "";
    --file_list_root (The absolute location which we prepend to the files in the
      file list (where listed files are relative to).); default: ".";
    --include_dir_paths (Comma separated paths of the directories used to look
      for included files.
      Note: The order of the files here is important.
      File search will stop at the the first found among the listed directories.
      e.g --include_dir_paths directory1,directory2
      if "A.sv" exists in both "directory1" and "directory2" the one in
      "directory1" is the one we will use.
      ); default: ;
```

## Commands

### `symbol-table-defs`

Builds a unified symbol table over all project files, and prints a
human-readable representation. This does not show nor attempt to resolve symbol
references.

### `symbol-table-refs`

Builds a unified symbol table over all project files, attempts to resolve all
symbol references to definitions, and prints a human-readable representation of
the references.

### `file-deps`

Prints inter-file dependencies.

Example output:

```
"foo.sv" depends on "bar.sv" for symbols { bar baz }
"bar.sv" depends on "baz.sv" for symbols { quux }
```
