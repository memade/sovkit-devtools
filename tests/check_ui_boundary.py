"""Keep native wxWidgets dependencies inside libwxui, out of application code."""
from pathlib import Path
import re
import sys

root = Path(__file__).resolve().parents[1]
native = re.compile(
    r'#\s*include\s*[<"]wx/|\bwx[A-Z]\w*\b|'
    r'(?:->|\.)\s*(?:GetManager|GetUIManager)\s*\('
)
failures = []
for path in sorted((root / 'src').rglob('*')):
    if path.suffix not in {'.cpp', '.hpp', '.cc', '.h'}:
        continue
    for line, text in enumerate(path.read_text(encoding='utf-8').splitlines(), 1):
        if native.search(text):
            failures.append(f'{path.relative_to(root)}:{line}: {text.strip()}')
if failures:
    print('Application code must use libwxui APIs:\n' + '\n'.join(failures))
    sys.exit(1)
print('Application UI boundary passed')
