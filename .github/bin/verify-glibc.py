#!/usr/bin/env python3
"""Check ELF architecture, imported GLIBC versions and runtime dependencies."""
import argparse
import json
import os
from pathlib import Path
import re
import struct
import subprocess


def command(*args):
    return subprocess.run(args, check=True, capture_output=True, text=True,
                          env={**os.environ, 'LC_ALL': 'C'}).stdout


def version(text):
    return tuple(int(part) for part in text.split('.'))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--binary', type=Path, action='append', required=True)
    parser.add_argument('--max-version', default='2.28')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    runtime = command('getconf', 'GNU_LIBC_VERSION').strip()
    assert runtime == 'glibc 2.28', runtime
    reports = []
    allowed = {'libc.so.6', 'libm.so.6', 'libpthread.so.0', 'libdl.so.2',
               'librt.so.1', 'ld-linux-x86-64.so.2'}
    for binary in args.binary:
        header = binary.read_bytes()[:20]
        assert header[:6] == b'\x7fELF\x02\x01', 'Expected little-endian ELF64'
        assert struct.unpack_from('<H', header, 18)[0] == 62, 'Expected x86-64'
        symbols = command('readelf', '--version-info', '--wide', str(binary))
        required = sorted(set(re.findall(r'\bGLIBC_(\d+(?:\.\d+)+)\b', symbols)), key=version)
        assert required, f'No dynamic GLIBC symbols found in {binary}'
        assert all(version(item) <= version(args.max_version) for item in required), required
        assert 'GLIBC_PRIVATE' not in symbols, symbols
        dynamic = command('readelf', '--dynamic', '--wide', str(binary))
        dependencies = re.findall(r'\(NEEDED\).*?\[(.*?)\]', dynamic)
        assert dependencies and set(dependencies) <= allowed, dependencies
        linked = command('ldd', str(binary))
        assert 'not found' not in linked, linked
        run_version = command(str(binary.resolve()), '--version')
        reports.append({'binary': binary.name, 'architecture': 'x86_64',
                        'glibc_versions': required, 'maximum_glibc': required[-1],
                        'needed_libraries': dependencies, 'ldd': linked,
                        'program_version': run_version})
    report = {'build_runtime_glibc': runtime, 'requested_maximum': args.max_version,
              'all_passed': True, 'binaries': reports}
    args.output.write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
