# SystemVerilog Source Code Indexing

<!--*
freshness: { owner: 'fangism' reviewed: '2020-10-07' }
*-->

`verible-verilog-kythe-extractor` extracts indexing facts from a collection of
SystemVerilog source files, following the [Kythe](http://kythe.io) schema. The
indexing data can drive interactive capabilities such as linking definitions and
cross references for easy navigation, and even name completion in editors.

## Usage

```
verible-verilog-kythe-extractor [options] <files...>

    --output_path (File path where to write the extracted Kythe facts in JSON
      format.); default: "";
    --printextraction (Whether or not to print the extracted general indexing
      facts from the middle layer)); default: false;
    --print_kythe_facts_json (Whether or not to print the extracted kythe facts);
      default: false;
```
