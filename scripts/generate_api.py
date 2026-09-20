#!/usr/bin/env python3
"""Generate DevTools import glue; the SovKit-supplied header stays unchanged."""
import argparse
import pathlib
import re

p = argparse.ArgumentParser()
p.add_argument('header', type=pathlib.Path)
p.add_argument('output', type=pathlib.Path)
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
members, loads, dispatch, names = [], [], [], []
for ret, name, params in decls:
    members.append(f'  decltype(&::{name}) {name} = nullptr;')
    loads.append(f'  {name} = symbol<decltype({name})>("{name}");')
    params = re.sub(r'\s+', ' ', params.strip())
    # Pointer alignment and comma spacing are presentation, not ABI changes.
    # Accept char** out, char **out and char * * out equally.
    params = re.sub(r'\s*\*\s*', '*', params)
    params = re.sub(r'\s*,\s*', ', ', params)
    shape = None
    if name not in reserved and ret.strip() == 'sovkit_error_t':
        if re.fullmatch(r'char\s*\*\*\w+, size_t\s*\*\w+', params):
            shape, call = 'get', f'{name}(&out, &size)'
        elif re.fullmatch(r'const char\s*\*\w+, size_t \w+, char\s*\*\*\w+, size_t\s*\*\w+', params):
            shape, call = 'json', f'{name}(input.data(), input.size(), &out, &size)'
        elif re.fullmatch(r'const char\s*\*\w+, size_t \w+', params):
            shape, call = 'command', f'{name}(input.data(), input.size())'
        elif params == 'void':
            shape, call = 'void', f'{name}()'
        if shape is None:
            raise SystemExit(f'Unrecognized public operation signature: {name}({params}); refusing partial API generation')
    if shape:
        short = name[len('sovkit_'):]
        names.append(f'  {{"{short}", "{shape}"}},')
        dispatch.append(f'  if (op == "{short}") return capture([&](char **unused, size_t *ignored) {{ (void)unused; (void)ignored; return {call}; }}, out, size);')
a.output.mkdir(parents=True, exist_ok=True)
for filename, lines in [('api_members.inc', members), ('api_load.inc', loads), ('api_dispatch.inc', dispatch), ('api_names.inc', names)]:
    (a.output / filename).write_text('\n'.join(lines) + '\n', encoding='utf-8')
(a.output/'api_count.inc').write_text(str(len(decls)), encoding='utf-8')
print(f'DevTools adapter: {len(decls)} typed imports, {len(names)} JSON operations')
