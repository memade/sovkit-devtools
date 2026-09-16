#!/usr/bin/env python3
"""One build entry for macOS, Windows Developer PowerShell, and Linux."""
import argparse
from pathlib import Path
import subprocess
p = argparse.ArgumentParser()
p.add_argument('preset')
p.add_argument('--sdk', required=True, type=Path)
p.add_argument('--test', action='store_true')
p.add_argument('--package', action='store_true')
p.add_argument('--jobs', type=int, default=4)
a = p.parse_args()
root = Path(__file__).resolve().parents[1]
build = root / '.build' / a.preset
subprocess.run(['cmake', '--preset', a.preset, f'-DSOVKIT_SDK_ROOT={a.sdk.resolve()}'], cwd=root, check=True)
subprocess.run(['cmake', '--build', str(build), '--parallel', str(a.jobs)], check=True)
if a.test:
    subprocess.run(['ctest', '--test-dir', str(build), '--output-on-failure'], check=True)
if a.package:
    subprocess.run(['cpack', '--config', str(build/'CPackConfig.cmake'), '-B', str(root/'.build/packages')], cwd=build, check=True)
