#!/usr/bin/env python3
"""Generate typed dynamic imports from the ONLY public SDK header."""
import argparse
import pathlib
import re

p = argparse.ArgumentParser()
p.add_argument('header', type=pathlib.Path)
p.add_argument('output', type=pathlib.Path)
a = p.parse_args()
s = re.sub(r'/\*.*?\*/|//[^\n]*', '', a.header.read_text(), flags=re.S)
decls = re.findall(r'SOVKIT_API\s+([\w\s*]+?)\s*SOVKIT_CALL\s+(sovkit_\w+)\s*\((.*?)\)\s*;', s, re.S)
expected = set(re.findall(r'SOVKIT_API\s+[^;{}#]*?\b(sovkit_\w+)\s*\(', s))
if len(decls) < 70 or {row[1] for row in decls} != expected:
    raise SystemExit('Unrecognized public header; refusing partial API generation')
reserved = {'sovkit_shutdown', 'sovkit_store_close_v1', 'sovkit_log_shutdown', 'sovkit_event_poll'}
members, loads, dispatch, names = [], [], [], []
for ret, name, params in decls:
    members.append(f'  decltype(&::{name}) {name} = nullptr;')
    loads.append(f'  {name} = symbol<decltype({name})>("{name}");')
    params = re.sub(r'\s+', ' ', params.strip())
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
    if shape:
        short = name.removeprefix('sovkit_')
        names.append(f'  {{"{short}", "{shape}"}},')
        dispatch.append(f'  if (op == "{short}") return capture([&](char **unused, size_t *ignored) {{ (void)unused; (void)ignored; return {call}; }}, out, size);')
a.output.mkdir(parents=True, exist_ok=True)
for filename, lines in [('api_members.inc', members), ('api_load.inc', loads), ('api_dispatch.inc', dispatch), ('api_names.inc', names)]:
    (a.output / filename).write_text('\n'.join(lines) + '\n')
(a.output/'api_count.inc').write_text(str(len(decls)))
print(f'Public contract: {len(decls)} typed imports, {len(names)} JSON operations')
