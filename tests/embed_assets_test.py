#!/usr/bin/env python3
import pathlib
import re
import subprocess
import sys
import tempfile
import unittest

SCRIPT = pathlib.Path(__file__).resolve().parents[1] / 'scripts/embed_assets.py'


class EmbeddedAssetsTest(unittest.TestCase):
    def test_text_binary_empty_and_large_resources(self):
        with tempfile.TemporaryDirectory() as folder:
            root = pathlib.Path(folder)
            inputs = root / 'resources'
            inputs.mkdir()
            expected = {
                'layout.xml': '<Window text="中文"/>\r\n'.encode('utf-8'),
                'nested/像素.bin': bytes(range(256)) * 400,
                'empty.txt': b'',
            }
            for name, data in expected.items():
                path = inputs / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(data)
            output = root / 'assets.hpp'
            command = [sys.executable, str(SCRIPT), '--directory', '=' + str(inputs), '--output', str(output)]
            subprocess.run(command, check=True, capture_output=True)
            generated = output.read_text(encoding='utf-8')
            arrays = {int(index): bytes(int(value) for value in re.findall(r'\d+', body))
                      for index, body in re.findall(r'data_(\d+)\[\] = \{(.*?)\};', generated, re.S)}
            actual = {}
            for name, index, size in re.findall(r'\{"(.*?)", data_(\d+), (\d+)\}', generated):
                name = bytes.fromhex(name.replace('\\x', '')).decode('utf-8')
                actual[name] = arrays[int(index)][:int(size)]
            self.assertEqual(actual, expected)
            modified = output.stat().st_mtime_ns
            subprocess.run(command, check=True, capture_output=True)
            self.assertEqual(output.stat().st_mtime_ns, modified, 'unchanged assets must not trigger recompilation')

    def test_duplicate_names_fail(self):
        with tempfile.TemporaryDirectory() as folder:
            root = pathlib.Path(folder)
            source = root / 'source'
            source.write_bytes(b'data')
            result = subprocess.run([sys.executable, str(SCRIPT), '--output', str(root / 'assets.hpp'),
                                     '--file', 'same=' + str(source), '--file', 'same=' + str(source)],
                                    capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('Duplicate resource name', result.stderr)


if __name__ == '__main__':
    unittest.main()
