## Indexing Verible C++ Source Code using Kythe

If you are looking for a searchable/indexed interface into Verible's source
code, this is already deployed at https://cs.opensource.google/verible/verible.

If you would like to learn how manually index the source code yourself, perhaps
for offline work, then read on. The steps mentioned here can be generalized for
indexing Bazel-based projects.

More information about indexing Bazel-based projects using Kythe
[here](https://github.com/kythe/kythe/tree/master/kythe/cxx/extractor#bazel-c-extractor).

Note, if you just want to locally edit the code, no need to index, just use
the compilation DB by calling `.github/bin/make-compilation-db.sh`

### Initializing Kythe

Download the latest Kythe release from https://github.com/kythe/kythe/releases
and then unpack it for a snapshot of Kythe’s toolset.

```bash
tar xzf kythe-v*.tar.gz
rm -rf /opt/kythe
mv kythe-v*/ /opt/kythe
```

More information can be found
[here](https://github.com/kythe/kythe#getting-started).

Clone Kythe from [here](https://github.com/kythe/kythe). Then from within the
Kythe clone, build the web frontend and copy its files into /opt/kythe/

```bash
bazel build //kythe/web/ui
mkdir -p /opt/kythe/web/ui
cp -r bazel-bin/kythe/web/ui/resources/public/* /opt/kythe/web/ui
cp -r kythe/web/ui/resources/public/* /opt/kythe/web/ui
chmod -R 755 /opt/kythe/web/ui
```

More information can be found
[here](https://github.com/google/haskell-indexer#building-from-source).

### Extracting Compilations for Verible

#### VNames.json File

`vnames.json` file is used for renaming certain filepaths during extraction, but
renaming isn’t needed here but you can add the renaming you find suitable.

#### Run the extractor

```bash
# run on all targets
bazel test --experimental_action_listener=:extract_cxx  //...

# run on specific target (e.g. some cc_binary or cc_library)
bazel test --experimental_action_listener=:extract_cxx //verible/verilog/analysis:default_rules
```

Extracted kzip files will be in
bazel-out/local-fastbuild/extra_actions/extractor folder. One kzip file per
target.

```bash
find -L bazel-out -name '*cxx.kzip'
```

More information can be found
[here](https://github.com/kythe/kythe/tree/master/kythe/cxx/extractor).

### Indexing Extracted Kzip Files and Generating GraphStore

For extracting the indexing facts from extracted `kzip` files, run the following
command to apply the indexing for each `kzip` file and generate the result into
`kythe graphstore`.

```bash
for i in $(find -L bazel-out -name '*cxx.kzip'); do
    # Indexing C++ compilations
    /opt/kythe/indexers/cxx_indexer --ignore_unimplemented "$i" > entries

    # Write entry stream into a GraphStore
    /opt/kythe/tools/write_entries --graphstore leveldb:.kythe_graphstore < entries
done
```

### Generating Serving Tables

```bash
# Generate corresponding serving tables
/opt/kythe/tools/write_tables --graphstore .kythe_graphstore --out .kythe_serving
```

### Visualizing Cross-References

Run Kythe's `UI` to visualize cross-reference and code navigation.

```bash
# --listen localhost:8080 allows access from only this machine; change to
# --listen 0.0.0.0:8080 to allow access from any machine
/opt/kythe/tools/http_server \
  --public_resources /opt/kythe/web/ui \
  --listen localhost:8080 \
  --serving_table .kythe_serving
```
