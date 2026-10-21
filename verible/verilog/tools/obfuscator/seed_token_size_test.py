#!/usr/bin/env python3
"""Real CLI regression tests; all generated files stay in --output."""
import argparse
import json
from pathlib import Path
import subprocess


SOURCE = b'''// count and secret remain in this comment.
module demo #(parameter WIDTH = 8) (
    input logic A,
    input logic [WIDTH-1:0] count,
    output logic [WIDTH-1:0] result
);
    localparam STEP = 1;
    logic [WIDTH-1:0] secret;
    assign secret = count + A + STEP;
    assign result = secret;
    initial $display("count secret");
endmodule
'''
MACRO = b'''`define ADD(a,b) ((a)+(b))
module macro_demo(input logic a,b, output logic y);
assign y = `ADD(a,b);
endmodule
'''


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--syntax', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    binary, syntax, out = (path.resolve() for path in (args.binary, args.syntax, args.output))
    out.mkdir(parents=True, exist_ok=True)
    commands, checks = [], []

    def run(name, source, *flags, executable=binary):
        result = subprocess.run([str(executable), *map(str, flags)], input=source,
                                capture_output=True, timeout=45)
        (out / f'{name}.input.sv').write_bytes(source)
        (out / f'{name}.sv').write_bytes(result.stdout)
        (out / f'{name}.stderr.txt').write_bytes(result.stderr)
        commands.append({'name': name, 'argv': [str(executable), *map(str, flags)],
                         'returncode': result.returncode, 'stdout_bytes': len(result.stdout)})
        return result

    def check(name, passed):
        checks.append({'name': name, 'passed': bool(passed)})
        print(f"{'PASS' if passed else 'FAIL'} {name}")

    def mapping(name):
        return out / f'{name}.map'

    def read_map(name):
        return dict(line.split() for line in mapping(name).read_text().splitlines() if line.strip())

    default = run('default', SOURCE, f'--save_map={mapping("default")}')
    check('default_preserves_identifier_lengths', default.returncode == 0 and all(len(k) == len(v) for k, v in read_map('default').items()))
    first = run('seed_first', SOURCE, '--seed=123', f'--save_map={mapping("seed_first")}')
    second = run('seed_repeat', SOURCE, '--seed=123', f'--save_map={mapping("seed_repeat")}')
    check('seed_repeat_bytes', first.returncode == second.returncode == 0 and first.stdout == second.stdout)
    check('seed_repeat_map', mapping('seed_first').read_bytes() == mapping('seed_repeat').read_bytes())
    different = run('seed_different', SOURCE, '--seed=124')
    check('different_seed_changes_output', different.returncode == 0 and different.stdout != first.stdout)
    zero = run('seed_zero', SOURCE, '--seed=0')
    zero_repeat = run('seed_zero_repeat', SOURCE, '--seed=0')
    check('zero_is_a_valid_fixed_seed', zero.returncode == zero_repeat.returncode == 0 and zero.stdout == zero_repeat.stdout)
    maximum = run('seed_maximum', SOURCE, '--seed=18446744073709551615')
    check('maximum_unsigned_seed', maximum.returncode == 0)

    uniform = run('uniform', SOURCE, '--seed=123', '--token-size=12', f'--save_map={mapping("uniform")}')
    um = read_map('uniform')
    check('uniform_new_names_have_requested_size', uniform.returncode == 0 and all(len(v) == 12 for k, v in um.items() if k != v))
    uniform_repeat = run('uniform_repeat', SOURCE, '--seed=123', '--token-size', '12')
    check('seed_and_size_repeat_bytes', uniform_repeat.returncode == 0 and uniform.stdout == uniform_repeat.stdout)
    underscore = run('underscore_alias', SOURCE, '--seed=123', '--token_size=12')
    check('underscore_alias_matches_hyphen', underscore.returncode == 0 and underscore.stdout == uniform.stdout)
    random_uniform = run('random_uniform', SOURCE, '--token-size=12', f'--save_map={mapping("random_uniform")}')
    check('token_size_without_seed', random_uniform.returncode == 0 and all(len(v) == 12 for k, v in read_map('random_uniform').items() if k != v))
    decode = run('decode', uniform.stdout, '--decode', f'--load_map={mapping("uniform")}')
    check('variable_size_decode_without_repeating_flags', decode.returncode == 0 and decode.stdout == SOURCE)
    replay = run('replay_map', SOURCE, '--seed=999', '--token-size=12', f'--load_map={mapping("uniform")}')
    check('loaded_map_overrides_seed', replay.returncode == 0 and replay.stdout == uniform.stdout)

    interface = run('interface', SOURCE, '--seed=123', '--token-size=12', '--preserve_interface', f'--save_map={mapping("interface")}')
    im = read_map('interface')
    check('interface_names_still_preserved', interface.returncode == 0 and all(im[name] == name for name in ('demo', 'WIDTH', 'A', 'count', 'result')))
    check('internal_names_use_requested_size', all(len(im[name]) == 12 for name in ('STEP', 'secret')))
    check('comment_string_system_call_preserved', SOURCE.splitlines()[0] in uniform.stdout and b'$display("count secret")' in uniform.stdout)
    for name, data in (('syntax_uniform', uniform.stdout), ('syntax_interface', interface.stdout)):
        result = run(name, data, '-', executable=syntax)
        check(name, result.returncode == 0)

    macro = run('macro', MACRO, '--seed=123', '--token-size=9', f'--save_map={mapping("macro")}')
    macro_decode = run('macro_decode', macro.stdout, '--decode', f'--load_map={mapping("macro")}')
    check('macro_roundtrip_with_size_changes', macro.returncode == macro_decode.returncode == 0 and macro_decode.stdout == MACRO)
    macro_syntax = run('macro_syntax', macro.stdout, '-', executable=syntax)
    check('macro_output_parses', macro_syntax.returncode == 0)
    crlf = SOURCE.replace(b'\n', b'\r\n')
    windows = run('crlf', crlf, '--seed=123', '--token-size=7', f'--save_map={mapping("crlf")}')
    windows_decode = run('crlf_decode', windows.stdout, '--decode', f'--load_map={mapping("crlf")}')
    check('crlf_roundtrip', windows.returncode == windows_decode.returncode == 0 and windows_decode.stdout == crlf)

    manual_map = mapping('manual')
    manual_map.write_bytes(b'A manually_renamed\n')
    manual = run('manual', SOURCE, '--seed=123', '--token-size=12', f'--load_map={manual_map}', f'--save_map={mapping("manual_saved")}')
    check('manual_map_may_override_uniform_size', manual.returncode == 0 and read_map('manual_saved')['A'] == 'manually_renamed')
    manual_decode = run('manual_decode', manual.stdout, '--decode', f'--load_map={mapping("manual_saved")}')
    check('manual_variable_size_map_roundtrip', manual_decode.returncode == 0 and manual_decode.stdout == SOURCE)
    default_manual = run('default_rejects_unequal_map', SOURCE, f'--load_map={manual_map}')
    check('default_still_rejects_unequal_map', default_manual.returncode != 0 and not default_manual.stdout)

    for label, flags in (
        ('negative_size', ['--token-size=-1']),
        ('excessive_size', ['--token-size=4097']),
        ('invalid_seed', ['--seed=abc']),
        ('negative_seed', ['--seed=-1']),
        ('overflow_seed', ['--seed=18446744073709551616']),
    ):
        result = run(label, SOURCE, *flags)
        check(label + '_rejected', result.returncode != 0 and not result.stdout)

    short = run('size_one', b'module top; wire signal; endmodule\n', '--seed=123', '--token-size=1', f'--save_map={mapping("size_one")}')
    check('one_character_size_supported', short.returncode == 0 and all(len(v) == 1 for k, v in read_map('size_one').items() if k != v))
    exhausted_source = ('module exhausted; wire ' + ','.join('signal' + str(i) for i in range(60)) + '; endmodule\n').encode()
    exhausted = run('exhausted', exhausted_source, '--seed=123', '--token-size=1')
    check('short_namespace_exhaustion_returns_error', exhausted.returncode != 0 and not exhausted.stdout and b'increase --token-size' in exhausted.stderr)

    passed = sum(c['passed'] for c in checks)
    report = {'checks_passed': passed, 'checks_total': len(checks), 'checks': checks, 'commands': commands,
              'seed_123_size_12_active_map': {k: um[k] for k in ('demo', 'WIDTH', 'A', 'count', 'result', 'STEP', 'secret')}}
    (out / 'report.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(f'{passed}/{len(checks)} checks passed.')
    return 0 if passed == len(checks) else 1


if __name__ == '__main__':
    raise SystemExit(main())
