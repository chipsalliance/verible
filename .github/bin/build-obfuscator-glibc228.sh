#!/usr/bin/env bash
set -euo pipefail

export PATH="/opt/python/cp312-cp312/bin:$PATH"
source /opt/rh/gcc-toolset-14/enable
export CC="$(command -v gcc)"
export CXX="$(command -v g++)"
# Bazel's GCC toolchain adds -lstdc++ explicitly, so the GCC-driver option
# -static-libstdc++ alone does not control that dependency. Configure the
# toolchain's runtime libraries directly, leaving libc and libm dynamic.
export BAZEL_LINKLIBS='-Wl,-Bstatic:-lstdc++:-Wl,-Bdynamic:-lm'
git config --global --add safe.directory "$PWD"

out="$PWD/easyobf-glibc228"
package="$out/verible-obfuscator-linux-x86_64-glibc2.28"
mkdir -p "$out" "$package/bin" "$package/examples"
{
  cat /etc/os-release
  getconf GNU_LIBC_VERSION
  gcc --version
  ld --version
  git rev-parse HEAD
  printf 'Build image: %s\n' "$BUILD_IMAGE"
} > "$out/environment.txt"
test "$(getconf GNU_LIBC_VERSION)" = 'glibc 2.28'

mkdir -p /tmp/bazel-download
pushd /tmp/bazel-download
curl -fsSL --retry 3 -O https://github.com/bazelbuild/bazel/releases/download/8.4.2/bazel-8.4.2-linux-x86_64
curl -fsSL --retry 3 -O https://github.com/bazelbuild/bazel/releases/download/8.4.2/bazel-8.4.2-linux-x86_64.sha256
sha256sum -c bazel-8.4.2-linux-x86_64.sha256
install -m 755 bazel-8.4.2-linux-x86_64 /usr/local/bin/bazel
popd
bazel --version >> "$out/environment.txt"

options=(-c opt --jobs=4 --copt=-march=x86-64
         --linkopt=-static-libstdc++ --linkopt=-static-libgcc)
bazel test "${options[@]}" --test_output=errors \
  //verible/common/strings:obfuscator_test \
  //verible/verilog/analysis:verilog-equivalence_test \
  //verible/verilog/transform:obfuscate_test \
  //verible/verilog/tools/obfuscator:obfuscate_test \
  2>&1 | tee "$out/bazel-tests.log"
bazel build "${options[@]}" \
  //verible/verilog/tools/obfuscator:verible-verilog-obfuscate \
  //verible/verilog/tools/syntax:verible-verilog-syntax \
  2>&1 | tee "$out/bazel-build.log"

install -m 755 bazel-bin/verible/verilog/tools/obfuscator/verible-verilog-obfuscate "$package/bin/"
install -m 755 bazel-bin/verible/verilog/tools/syntax/verible-verilog-syntax "$package/bin/"
strip "$package/bin/"*
python verible/verilog/tools/obfuscator/seed_token_size_test.py \
  --binary "$package/bin/verible-verilog-obfuscate" \
  --syntax "$package/bin/verible-verilog-syntax" \
  --output "$out/cli-tests" | tee "$out/cli-tests.log"
python .github/bin/verify-glibc.py \
  --binary "$package/bin/verible-verilog-obfuscate" \
  --binary "$package/bin/verible-verilog-syntax" \
  --max-version 2.28 --output "$out/abi-report.json"

cp "$out/cli-tests/uniform.input.sv" "$package/examples/original.sv"
cp "$out/cli-tests/uniform.sv" "$package/examples/obfuscated.sv"
cp "$out/cli-tests/uniform.map" "$package/examples/names.map"
cp LICENSE "$package/LICENSE"
cp verible/verilog/tools/obfuscator/README.md "$package/OPTIONS.md"
cp "$out/environment.txt" "$package/BUILD.txt"
cat > "$package/README.txt" <<'EOF'
Verible obfuscator with --seed and --token-size
Platform: Linux x86-64, glibc >= 2.28.
C++ and GCC runtime libraries are linked statically; glibc remains dynamic.

From this directory:
  chmod +x bin/*
  bin/verible-verilog-obfuscate --seed=123 --token-size=12 --save_map=fixed.map < examples/original.sv > output.sv
  bin/verible-verilog-obfuscate --decode --load_map=fixed.map < output.sv > restored.sv

See OPTIONS.md, ABI.json and RUNTIME.txt for options and compatibility evidence.
EOF
(
  cd "$package"
  sha256sum bin/* > SHA256SUMS
)
