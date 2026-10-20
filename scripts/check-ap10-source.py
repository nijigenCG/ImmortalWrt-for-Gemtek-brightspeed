#!/usr/bin/env python3
"""Check the prepared AP10 kernel sources; never modifies devices or sources."""
import argparse
import hashlib
import json
from pathlib import Path


def verify(kernel, manifest):
    expected = dict(manifest['source_files'])
    expected.update({name: row['after_SHA256'] for name, row in manifest['netfilter_sources'].items()})
    failures = []
    for name, digest in expected.items():
        p = kernel / name
        if not p.is_file() or hashlib.sha256(p.read_bytes()).hexdigest() != digest:
            failures.append(name)
    configs = [kernel / name for name in ('.config', '.config.target', '.config.set')]
    available = [p for p in configs if p.is_file()]
    if not available:
        failures.append('No generated kernel configuration to check')
    else:
        # Kernel prepare may produce .config.target/.config.set before the
        # final .config; prefer the final config whenever it is available.
        config = available[0].read_text()
        for line in ('CONFIG_HZ=1000', 'CONFIG_HZ_1000=y'):
            if line not in config.splitlines():
                failures.append(line)
    return failures


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('kernel', type=Path)
    args = p.parse_args()
    root = Path(__file__).resolve().parent.parent
    manifest = json.loads((root / 'docs/ap10/source-manifest.json').read_text())
    failures = verify(args.kernel, manifest)
    print(json.dumps({'source_and_HZ_check_passed': not failures, 'failures': failures,
                      'compile_or_boot_test': False}, ensure_ascii=False))
    return bool(failures)


if __name__ == '__main__':
    raise SystemExit(main())
