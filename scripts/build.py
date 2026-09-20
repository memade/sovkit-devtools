#!/usr/bin/env python3
"""One build entry for macOS, Windows Developer PowerShell, and Linux."""
import argparse
import os
import sys
from pathlib import Path
import subprocess
p = argparse.ArgumentParser()
p.add_argument('preset')
p.add_argument('--sdk', type=Path, help='Override the SovKit package (default: 3rdparty/sovkit_sdk/0.1.0)')
p.add_argument('--test', action='store_true')
p.add_argument('--package', action='store_true')
p.add_argument('--jobs', type=int, default=4)
p.add_argument('--cmake-arg', action='append', default=[], help='Additional configure argument; use --cmake-arg=-DNAME=value')
a = p.parse_args()
root = Path(__file__).resolve().parents[1]
build = root / '.build' / a.preset
sdk_package = a.sdk.resolve() if a.sdk else root / '3rdparty/sovkit_sdk/0.1.0'
configure = ['cmake', '--preset', a.preset, '-B', str(build), f'-DSOVKIT_SDK_ROOT={sdk_package}']
if sys.platform == 'darwin':
    # Keep compiler probes (including vcpkg host tools) on the selected Xcode SDK.
    # An unrelated newer Command Line Tools SDK may not match its linker.
    sdkroot = os.environ.get('SDKROOT') or subprocess.check_output(
        ['xcrun', '--sdk', 'macosx', '--show-sdk-path'], text=True).strip()
    os.environ['SDKROOT'] = sdkroot
    if a.preset.startswith('macos-'):
        configure.append(f'-DCMAKE_OSX_SYSROOT={sdkroot}')
configure.extend(a.cmake_arg)
subprocess.run(configure, cwd=root, check=True)
subprocess.run(['cmake', '--build', str(build), '--parallel', str(a.jobs)], check=True)
if a.test:
    subprocess.run(['ctest', '--test-dir', str(build), '--output-on-failure'], check=True)
if a.package:
    subprocess.run(['cpack', '--config', str(build/'CPackConfig.cmake'), '-B', str(root/'.build/packages')], cwd=build, check=True)
