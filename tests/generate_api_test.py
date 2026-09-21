#!/usr/bin/env python3
"""Formatting must not change the generated public operation catalogue."""
import pathlib
import subprocess
import sys
import tempfile
import unittest


GENERATOR = pathlib.Path(__file__).resolve().parents[1] / 'scripts/generate_api.py'


class GenerationTest(unittest.TestCase):
    def generate(self, root, pointers, comma=', ', broken=False):
        def declaration(name, params, ret='sovkit_error_t'):
            params = params.replace('**', '@double@').replace('*', pointers[0])
            params = params.replace('@double@', pointers[1]).replace(', ', comma)
            return f'SOVKIT_API {ret} SOVKIT_CALL sovkit_{name}({params});\n'

        # Meet the public-header completeness threshold without duplicating the
        # production SDK. These scalar functions are imported but not JSON APIs.
        header = ''.join(declaration(f'metadata_{i}', 'void', 'uint32_t') for i in range(70))
        header += declaration('info', 'char **out, size_t *size')
        header += declaration('message_send', 'const char *data, size_t len, char **out, size_t *size')
        header += declaration('network_paths_update', 'const char *data, size_t len')
        header += declaration('discovery_stop', 'void')
        header += declaration('event_poll', 'char **out, size_t *size')
        header += declaration('store_close_v1', 'void')
        header += declaration('storage_protect', 'const uint8_t *key, const uint8_t *data, size_t len, char **out, size_t *size')
        if broken:
            header += declaration('new_operation', 'int unsupported_argument')
        source = root / 'sovkit.h'
        source.write_text(header, encoding='utf-8')
        return subprocess.run([sys.executable, str(GENERATOR), str(source), str(root / 'generated')],
                              capture_output=True, text=True)

    def test_pointer_alignment_and_line_breaks(self):
        baseline = None
        for pointers, comma in [(('*', '**'), ', '), (('* ', '** '), ','),
                                ((' * ', ' * * '), ' ,\n\t'), (('\t*\t', '*\n*\t'), ',\n')]:
            with self.subTest(pointers=pointers), tempfile.TemporaryDirectory() as directory:
                root = pathlib.Path(directory)
                result = self.generate(root, pointers, comma)
                self.assertEqual(result.returncode, 0, result.stderr)
                output = {p.name: p.read_text(encoding='utf-8') for p in (root / 'generated').iterdir()}
                self.assertEqual(output['api_names.inc'],
                                 '  {"info", "get"},\n'
                                 '  {"message_send", "json"},\n'
                                 '  {"network_paths_update", "command"},\n'
                                 '  {"discovery_stop", "void"},\n')
                self.assertEqual(output['api_count.inc'], '77')
                self.assertEqual(set(output), {'api_names.inc', 'api_exports.inc', 'api_count.inc'})
                if baseline is None:
                    baseline = output
                self.assertEqual(output, baseline)

    def test_unknown_operation_fails_before_writing_partial_output(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            result = self.generate(root, ('* ', '** '), broken=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('sovkit_new_operation', result.stderr)
            self.assertFalse((root / 'generated').exists())

    def test_missing_checked_in_route_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            self.assertEqual(self.generate(root, ('*', '**')).returncode, 0)
            router = root / 'operations.cpp'
            names = ['stop', 'events', 'selftest', 'info', 'message_send',
                     'network_paths_update', 'discovery_stop']
            router.write_text('\n'.join(f'if (operation == "{name}") {{}}' for name in names), encoding='utf-8')
            command = [sys.executable, str(GENERATOR), str(root / 'sovkit.h'),
                       str(root / 'checked'), '--dispatch', str(router)]
            self.assertEqual(subprocess.run(command, capture_output=True).returncode, 0)
            router.write_text(router.read_text(encoding='utf-8').replace('message_send', 'typo'), encoding='utf-8')
            result = subprocess.run(command, capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('message_send', result.stderr)


if __name__ == '__main__':
    unittest.main()
