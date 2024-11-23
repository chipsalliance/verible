# SystemVerilog Source Code Indexing

<!--*
freshness: { owner: 'hzeller' reviewed: '2020-10-07' }
*-->

`verible-verilog-kythe-extractor` extracts indexing facts from a collection of
SystemVerilog source files, following the [Kythe](http://kythe.io) schema. The
indexing data can drive interactive capabilities such as linking definitions and
cross references for easy navigation, and even name completion in editors.

## Usage

```
verible-verilog-kythe-extractor [options] --file_list_path FILE

    --printextraction (Whether or not to print the extracted general indexing
      facts tree from the middle layer)); default: false;
    --print_kythe_facts (Determines how to print Kythe indexing facts.
                        Options:
                            json: Outputs Kythe facts in JSON format.
                            proto: Outputs Kythe facts in proto format);
                        default: json;
    --file_list_path (The path to the file list which contains the names of SystemVerilog files.
                      The files should be ordered by definition dependencies)
    --file_list_root (The absolute location which we prepend to the files in the file list (where listed files are relative to);
                      default: the place of invocation.
    --include_dir_paths (Comma separated paths of the directories used to look for included files.
                         Note: The order of the files here is important.
                         File search will stop at the the first found among the listed directories.
                         e.g --include_dir_paths directory1,directory2
                         if "A.sv" exists in both "directory1" and "directory2" the one in "directory1" is the one we will use)
```
