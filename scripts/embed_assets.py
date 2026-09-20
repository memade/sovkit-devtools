#!/usr/bin/env python3
"""Embed UTF-8 assets in bounded C++ literals (also compatible with MSVC)."""
import argparse
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('banner', type=Path)
parser.add_argument('documentation', type=Path)
parser.add_argument('output', type=Path)
parser.add_argument('--workbench', type=Path)
args = parser.parse_args()
lines = ['#pragma once']
assets = [('kBanner', args.banner), ('kSdkDocs', args.documentation)]
if args.workbench:
    assets.append(('kWorkbench', args.workbench))
for name, source in assets:
    data = source.read_text(encoding='utf-8').encode('utf-8')
    lines.append('inline constexpr char ' + name + '[] =')
    # Short ASCII-only tokens avoid both source-codepage issues and a raw
    # string delimiter occurring in Markdown supplied with a future SDK.
    for offset in range(0, len(data), 1024):
        lines.append('"' + ''.join('\\x%02x' % byte for byte in data[offset:offset+1024]) + '"')
    if not data:
        lines.append('""')
    lines.append(';')
args.output.write_text('\n'.join(lines) + '\n', encoding='utf-8')
