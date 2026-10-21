#!/usr/bin/env bash
set -euo pipefail

out="$PWD/easyobf-glibc228"
package="$out/verible-obfuscator-linux-x86_64-glibc2.28"
exec > >(tee "$out/runtime.log") 2>&1

# Minimal AlmaLinux does not include diffutils/cmp. Use its existing coreutils
# SHA-256 implementation to compare outputs without installing extra packages.
same_file() {
  local left right
  left="$(sha256sum "$1")"
  right="$(sha256sum "$2")"
  test "${left%% *}" = "${right%% *}"
}

cat /etc/os-release
getconf GNU_LIBC_VERSION
test "$(getconf GNU_LIBC_VERSION)" = 'glibc 2.28'
for program in "$package/bin/"*; do
  ldd "$program"
  "$program" --version
done
(
  cd "$package"
  sha256sum -c SHA256SUMS
)
"$package/bin/verible-verilog-obfuscate" --seed=123 --token-size=12 \
  --save_map="$out/runtime.map" < "$package/examples/original.sv" > "$out/runtime.sv"
same_file "$out/runtime.sv" "$package/examples/obfuscated.sv"
same_file "$out/runtime.map" "$package/examples/names.map"
"$package/bin/verible-verilog-obfuscate" --decode --load_map="$out/runtime.map" \
  < "$out/runtime.sv" > "$out/runtime-restored.sv"
same_file "$out/runtime-restored.sv" "$package/examples/original.sv"
"$package/bin/verible-verilog-syntax" - < "$out/runtime.sv"
printf 'PASS: clean AlmaLinux 8 glibc 2.28 runtime; seeded output, map, syntax and exact restoration.\n'
