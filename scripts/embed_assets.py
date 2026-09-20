#!/usr/bin/env python3
"""Embed named files/directories byte-for-byte into a C++ resource registry."""
import argparse
from pathlib import Path, PurePosixPath


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--directory', action='append', default=[], metavar='PREFIX=PATH')
    parser.add_argument('--file', action='append', default=[], metavar='NAME=PATH')
    args = parser.parse_args()
    assets = {}

    def add(name, source):
        name = name.replace('\\', '/')
        if not name or name.startswith('/') or '..' in PurePosixPath(name).parts:
            parser.error('Invalid resource name: ' + name)
        if name in assets:
            parser.error('Duplicate resource name: ' + name)
        assets[name] = source.read_bytes()

    for mount in args.directory:
        prefix, path = mount.split('=', 1)
        root = Path(path)
        if not root.is_dir():
            parser.error('Missing resource directory: ' + str(root))
        for source in sorted(root.rglob('*')):
            if source.is_file():
                name = '/'.join(part for part in (prefix, source.relative_to(root).as_posix()) if part)
                add(name, source)
    for mount in args.file:
        name, path = mount.split('=', 1)
        add(name, Path(path))
    if not assets:
        parser.error('No resources to embed')

    lines = ['#pragma once', '#include <string>', '#include <string_view>',
             '#include <stdexcept>', 'namespace devtools::assets {',
             'struct Resource { std::string_view name; const unsigned char* data; size_t size; };']
    # Numeric bytes handle NULs, images and large resources without MSVC's
    # concatenated-string literal size limit or source encoding assumptions.
    for index, (name, data) in enumerate(sorted(assets.items())):
        lines.append(f'inline constexpr unsigned char data_{index}[] = {{')
        for offset in range(0, len(data), 32):
            lines.append(','.join(str(byte) for byte in data[offset:offset + 32]) + ',')
        if not data:
            lines.append('0')
        lines.append('};')
    lines.append('inline constexpr Resource resources[] = {')
    for index, (name, data) in enumerate(sorted(assets.items())):
        quoted = '"' + ''.join('\\x%02x' % byte for byte in name.encode('utf-8')) + '"'
        lines.append(f'{{{quoted}, data_{index}, {len(data)}}},')
    lines.extend(['};', '''inline bool Load(const std::string& name, std::string* output) {
    for (const auto& resource : resources) {
        if (resource.name == name) {
            if (output) output->assign(reinterpret_cast<const char*>(resource.data), resource.size);
            return true;
        }
    }
    return false;
}
inline std::string Get(const std::string& name) {
    std::string data;
    if (!Load(name, &data)) throw std::runtime_error("Missing embedded resource: " + name);
    return data;
}
inline std::string Notices() {
    std::string text;
    for (const auto& resource : resources) {
        if (resource.name == "LICENSE" || resource.name == "NOTICE.md" ||
            resource.name.substr(0, 9) == "licenses/") {
            text += "\\n\\n=== " + std::string(resource.name) + " ===\\n\\n";
            text.append(reinterpret_cast<const char*>(resource.data), resource.size);
        }
    }
    return text;
}
} // namespace devtools::assets'''])
    text = '\n'.join(lines) + '\n'
    args.output.parent.mkdir(parents=True, exist_ok=True)
    if not args.output.exists() or args.output.read_text(encoding='utf-8') != text:
        args.output.write_text(text, encoding='utf-8')
    print(f'Embedded {len(assets)} resources ({sum(map(len, assets.values()))} bytes)')


if __name__ == '__main__':
    main()
