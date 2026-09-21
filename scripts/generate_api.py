#!/usr/bin/env python3
"""Generate catalogue metadata only; all SDK calls live in checked-in C++ sources."""
import argparse
import pathlib
import re

p = argparse.ArgumentParser()
p.add_argument('header', type=pathlib.Path)
p.add_argument('output', type=pathlib.Path)
p.add_argument('--dispatch', type=pathlib.Path, help='Validate the checked-in operation router')
a = p.parse_args()
s = re.sub(r'/\*.*?\*/|//[^\n]*', '', a.header.read_text(encoding="utf-8"), flags=re.S)
decls = re.findall(r'SOVKIT_API\s+([\w\s*]+?)\s*SOVKIT_CALL\s+(sovkit_\w+)\s*\((.*?)\)\s*;', s, re.S)
expected = set(re.findall(r'SOVKIT_API\s+[^;{}#]*?\b(sovkit_\w+)\s*\(', s))
if len(decls) < 70 or {row[1] for row in decls} != expected:
    raise SystemExit('Unrecognized public header; refusing partial API generation')
# These APIs use host-owned lifecycle, callbacks, keys or binary buffers rather
# than the workbench's JSON request/response contract. Everything else returning
# sovkit_error_t must be recognized; never silently publish a partial catalogue.
reserved = {
    'sovkit_register_result_cb', 'sovkit_event_poll', 'sovkit_register_log_cb',
    'sovkit_log_flush', 'sovkit_log_shutdown', 'sovkit_store_derive_key',
    'sovkit_store_open_v1', 'sovkit_store_close_v1', 'sovkit_register_keystore',
    'sovkit_init', 'sovkit_init_cpp', 'sovkit_shutdown',
    'sovkit_storage_protect', 'sovkit_storage_unprotect',
    'sovkit_vault_create', 'sovkit_vault_unlock', 'sovkit_vault_rewrap',
}
exports, names, operations = [], [], []
for ret, name, params in decls:
    exports.append(f'  "{name}",')
    params = re.sub(r'\s+', ' ', params.strip())
    # Pointer alignment and comma spacing are presentation, not ABI changes.
    # Accept char** out, char **out and char * * out equally.
    params = re.sub(r'\s*\*\s*', '*', params)
    params = re.sub(r'\s*,\s*', ', ', params)
    shape = None
    if name not in reserved and ret.strip() == 'sovkit_error_t':
        if re.fullmatch(r'char\s*\*\*\w+, size_t\s*\*\w+', params):
            shape = 'get'
        elif re.fullmatch(r'const char\s*\*\w+, size_t \w+, char\s*\*\*\w+, size_t\s*\*\w+', params):
            shape = 'json'
        elif re.fullmatch(r'const char\s*\*\w+, size_t \w+', params):
            shape = 'command'
        elif params == 'void':
            shape = 'void'
        if shape is None:
            raise SystemExit(f'Unrecognized public operation signature: {name}({params}); refusing partial API generation')
    if shape:
        short = name[len('sovkit_'):]
        names.append(f'  {{"{short}", "{shape}"}},')
        operations.append(short)
if a.dispatch:
    source = a.dispatch.read_text(encoding='utf-8')
    implemented = set(re.findall(r'operation\s*==\s*"(\w+)"', source))
    expected_operations = set(operations) | {'stop', 'events', 'selftest'}
    if implemented != expected_operations:
        raise SystemExit('SDK operation router differs from supplied header: missing=' +
                         str(sorted(expected_operations - implemented)) +
                         ', extra=' + str(sorted(implemented - expected_operations)))

a.output.mkdir(parents=True, exist_ok=True)
outputs = {
    'api_exports.inc': '\n'.join(exports) + '\n',
    'api_names.inc': '\n'.join(names) + '\n',
    'api_count.inc': str(len(decls)),
}
for filename, content in outputs.items():
    path = a.output / filename
    # Unchanged metadata must not force every SDK source file to recompile.
    if not path.exists() or path.read_text(encoding='utf-8') != content:
        path.write_text(content, encoding='utf-8')
print(f'DevTools adapter: {len(decls)} checked public exports, {len(names)} JSON operations')
