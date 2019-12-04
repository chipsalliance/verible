#!/bin/bash

mkdir -p "$KOKORO_ARTIFACTS_DIR/bazel_test_logs"
# rename all test.log to sponge_log.log and then copy them to the kokoro
# artifacts directory.
find -L . -name "test.log" -exec rename 's/test.log/sponge_log.log/' {} \;
find -L . -name "sponge_log.log" -exec cp --parents {} "$KOKORO_ARTIFACTS_DIR/bazel_test_logs" \;
# rename all test.xml to sponge_log.xml and then copy them to kokoro
# artifacts directory.
find -L . -name "test.xml" -exec rename 's/test.xml/sponge_log.xml/' {} \;
find -L . -name "sponge_log.xml" -exec cp --parents {} "$KOKORO_ARTIFACTS_DIR/bazel_test_logs" \;
