#!/bin/bash
# Copyright 2020 The Verible Authors.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# GitHub Pages
# ---------------
# Generate the GitHub pages to deploy

GIT_DATE=$(git show -s --format=%ci)
GIT_VERSION=$(git describe --match=v*)
GIT_HASH=$(git rev-parse HEAD)

PAGES_DIR=/tmp/pages
rm -rf $PAGES_DIR
mkdir -p $PAGES_DIR

# Copy across the current markdown files
# --------------------------------
cp -a *.md $PAGES_DIR/
cp -a AUTHORS $PAGES_DIR/AUTHORS.md

# Generate an index page
# --------------------------------
INDEX_DOC=$PAGES_DIR/index.md

# Copy the top of the README.md
sed -e '/## /,$d' README.md > $INDEX_DOC
# Add links to other documents
cat >> $INDEX_DOC <<EOF

See the [README file for further information.](README.md)

## Tools

 * [verilog_lint Info](verilog_lint.md)
 * [verilog_format Info](verilog_format.md)
 * [verilog_syntax Info](verilog_syntax.md)

## Information

 * [Code - https://github.com/$TRAVIS_REPO_SLUG](https://github.com/$TRAVIS_REPO_SLUG)
 * [Binaries - https://github.com/$TRAVIS_REPO_SLUG/releases](https://github.com/$TRAVIS_REPO_SLUG/releases)
 * [Bug Reports - https://github.com/$TRAVIS_REPO_SLUG/issues/new](https://github.com/$TRAVIS_REPO_SLUG/issues/new)
 * [Lint Rules](lint.md)
 * [Further Information](README.md)

## Authors

 * [Contributing](CONTRIBUTING.md)
 * [Authors](AUTHORS.md)
 * [License Information](license.md)

## Version

Generated on $GIT_DATE from [$GIT_VERSION](https://github.com/$TRAVIS_REPO_SLUG/commit/$GIT_HASH)
EOF

# Add markdown version of Apache 2.0 license
# --------------------------------
wget https://raw.githubusercontent.com/IQAndreas/markdown-licenses/master/apache-v2.0.md -O $PAGES_DIR/license.md

# Generate lint rules documentation
# --------------------------------
bazel-bin/verilog/tools/lint/verilog_lint -generate_markdown > $PAGES_DIR/lint.md
cat >> $PAGES_DIR/lint.md <<EOF

## Version

Generated on $GIT_DATE from [$GIT_VERSION](https://github.com/$TRAVIS_REPO_SLUG/commit/$GIT_HASH)
EOF

# Generate docs for verilog_syntax
# --------------------------------
SYNTAX_DOC=$PAGES_DIR/verilog_syntax.md
cat > $SYNTAX_DOC <<EOF
# \`verilog_syntax\`

Tool for looking at the syntax of Verilog and SystemVerilog code. Part of the
verible tool suite.

## Command line arguments
\`\`\`
EOF
bazel-bin/verilog/tools/syntax/verilog_syntax -helpfull >> $SYNTAX_DOC
cat >> $SYNTAX_DOC <<EOF
\`\`\`

## Version

Generated on $GIT_DATE from [$GIT_VERSION](https://github.com/$TRAVIS_REPO_SLUG/commit/$GIT_HASH)
EOF


# Generate docs for verilog_lint
# --------------------------------
LINT_DOC=$PAGES_DIR/verilog_lint.md
cat > $LINT_DOC <<EOF
# \`verilog_lint\`

Tool for formatting Verilog and SystemVerilog code. Part of the verible tool
suite.

## Command line arguments

\`\`\`
EOF
bazel-bin/verilog/tools/lint/verilog_lint -helpfull >> $LINT_DOC
cat >> $LINT_DOC <<'EOF'
```

## Lint Rules

EOF
bazel-bin/verilog/tools/lint/verilog_lint -generate_markdown >> $LINT_DOC
cat >> $LINT_DOC <<EOF

## Version

Generated on $GIT_DATE from [$GIT_VERSION](https://github.com/$TRAVIS_REPO_SLUG/commit/$GIT_HASH)
EOF

# Generate docs for verilog_format
# --------------------------------
FORMAT_DOC=$PAGES_DIR/verilog_format.md
cat > $FORMAT_DOC <<EOF
# \`verilog_format\`

Tool for formatting Verilog and SystemVerilog code. Part of the verible tool
suite.

## Command line arguments
\`\`\`
EOF
bazel-bin/verilog/tools/formatter/verilog_format -helpfull >> $FORMAT_DOC
cat >> $FORMAT_DOC <<EOF
\`\`\`

## Version

Generated on $GIT_DATE from [$GIT_VERSION](https://github.com/$TRAVIS_REPO_SLUG/commit/$GIT_HASH)
EOF

# Add jekyll front matter to all markdown pages.
# ----------------------------------------------
for MD in $PAGES_DIR/*; do
    cat > $MD.new <<EOF
---
---

EOF
    cat $MD >> $MD.new
    mv $MD.new $MD
done
