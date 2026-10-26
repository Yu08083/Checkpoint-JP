import collections
import json
from pathlib import Path
import re

root = Path(__file__).resolve().parents[2]


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f'Duplicate JSON key: {key}')
        result[key] = value
    return result


catalog = json.loads(
    (root / '3ds/assets/romfs/i18n.json').read_text(encoding='utf-8'),
    object_pairs_hook=unique_object,
)
for key, languages in catalog.items():
    assert isinstance(languages.get('ja'), str) and languages['ja'].strip(), key
    english = collections.Counter(re.findall(r'\{\d+\}', languages['en']))
    japanese = collections.Counter(re.findall(r'\{\d+\}', languages['ja']))
    assert english == japanese, f'{key}: placeholder mismatch'
    assert '\ufffd' not in languages['ja'], f'{key}: invalid replacement character'

for source in (root / '3ds').rglob('*'):
    if source.suffix not in {'.cpp', '.hpp'}:
        continue
    for key in re.findall(r'i18n::t\("([^"\n]+)"', source.read_text(encoding='utf-8')):
        assert key in catalog, f'{source}: missing {key}'

config = json.loads((root / '3ds/assets/romfs/config.json').read_text(encoding='utf-8'))
assert config['language'] == 'ja'
assert config['auto_update'] is False
assert config['confirm_restore'] is True
print(f'3DS Japanese catalog passed: {len(catalog)} entries; placeholders and source keys verified')
