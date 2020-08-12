#!/bin/bash -e
set -o pipefail
BROWSE_PORT="${BROWSE_PORT:-8080}"
# You can find prebuilt binaries at https://github.com/kythe/kythe/releases.
# This script assumes that they are installed to /opt/kythe.
# If you build the tools yourself or install them to a different location,
# make sure to pass the correct public_resources directory to http_server.
rm -f -- graphstore/* tables/*
mkdir -p graphstore tables
bazel-bin/verilog/tools/kythe/verible-verilog-kythe-extractor verilog.v  --printkythefacts > entries
# Read JSON entries from standard in to a graphstore.
/opt/kythe/tools/entrystream --read_json < entries \
  | /opt/kythe/tools/write_entries -graphstore graphstore
# Convert the graphstore to serving tables.
/opt/kythe/tools/write_tables -graphstore graphstore -out=tables
# Host the browser UI.
/opt/kythe/tools/http_server -serving_table tables \
  -public_resources="/opt/kythe/web/ui" \
  -listen="localhost:${BROWSE_PORT}"  # ":${BROWSE_PORT}" allows access from other machines

