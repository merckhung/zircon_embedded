#!/usr/bin/env python3
"""Apply the checked-in patch and build Chromium in an external, pinned checkout."""
import argparse
import fcntl
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parent


def acquire_build_lock(checkout, arch):
    """Keep Ninja and packaging from touching one output directory together."""
    output_root = checkout / 'out'
    output_root.mkdir(parents=True, exist_ok=True)
    stream = (output_root / ('.zircon-' + arch + '.lock')).open('a')
    try:
        fcntl.flock(stream, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except BlockingIOError:
        stream.close()
        raise RuntimeError('another build or packaging process is active for ' + arch)
    return stream


def run(command, checkout, **kwargs):
    return subprocess.run(command, cwd=checkout, check=True, **kwargs)


def remove_empty_compiler_outputs(output):
    # A host interruption can leave zero-byte outputs which Ninja considers
    # up-to-date. Hold the architecture lock and rebuild those edges normally.
    removed = 0
    for path in output.rglob('*'):
        if path.suffix in ('.o', '.a', '.rlib', '.rmeta', '.pcm') and path.is_file():
            if path.stat().st_size == 0:
                path.unlink()
                removed += 1
    if removed:
        print(f'Removed {removed} empty compiler outputs left by an interrupted build', flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--checkout', type=Path, required=True)
    parser.add_argument('--arch', choices=('x64', 'arm64'), required=True)
    parser.add_argument('--jobs', type=int, default=8)
    args = parser.parse_args()
    checkout = args.checkout.resolve()
    build_lock = acquire_build_lock(checkout, args.arch)
    revision = (ROOT / 'patches/REVISION').read_text().strip()
    actual = run(['git', 'rev-parse', 'HEAD'], checkout, capture_output=True, text=True).stdout.strip()
    if actual != revision:
        parser.error('checkout HEAD does not match patches/REVISION')
    patch = str(ROOT / 'patches/chromium_zircon_linux_x64.patch')
    check = subprocess.run(['git', 'apply', '--check', patch], cwd=checkout, capture_output=True)
    if check.returncode == 0:
        run(['git', 'apply', patch], checkout)
    else:
        run(['git', 'apply', '--reverse', '--check', patch], checkout)
    output = Path('out') / ('ZirconLinux_' + args.arch)
    (checkout / output).mkdir(parents=True, exist_ok=True)
    remove_empty_compiler_outputs(checkout / output)
    config = (ROOT / 'patches/args.gn').read_text().replace('target_cpu = "x64"', f'target_cpu = "{args.arch}"')
    (checkout / output / 'args.gn').write_text(config)
    run(['./buildtools/linux64/gn', 'gen', str(output), '--fail-on-unused-args'], checkout)
    run(['./third_party/ninja/ninja', '-C', str(output), '-j', str(args.jobs),
         'chrome', 'chrome_crashpad_handler'], checkout)


if __name__ == '__main__':
    main()
