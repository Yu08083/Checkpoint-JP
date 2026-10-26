import hashlib
from pathlib import Path
import shutil
import struct
import subprocess
import zipfile

root = Path(__file__).resolve().parents[2]
output = root / 'release'
output.mkdir(exist_ok=True)
cia = (root / '3ds/out/Checkpoint.cia').read_bytes()
header, kind, version, certificates, ticket_size, tmd_size, meta_size, content_size = struct.unpack_from('<IHHIIIIQ', cia)


def align(size):
    return (size + 63) & ~63


ticket_offset = align(header) + align(certificates)
tmd_offset = ticket_offset + align(ticket_size)
assert tmd_offset + align(tmd_size) + align(content_size) + meta_size == len(cia)
signature_sizes = {0x10000: 0x240, 0x10001: 0x140, 0x10002: 0x80, 0x10003: 0x240, 0x10004: 0x140, 0x10005: 0x80}
tmd_body = tmd_offset + signature_sizes[struct.unpack_from('>I', cia, tmd_offset)[0]]
title_id = struct.unpack_from('>Q', cia, tmd_body + 0x4c)[0]
title_version = struct.unpack_from('>H', cia, tmd_body + 0x9c)[0]
assert title_id == 0x000400000BCFFF00, f'Title ID: {title_id:016X}'
assert title_version == 5168, f'Title version: {title_version}'
assert (root / '3ds/out/Checkpoint.3dsx').read_bytes()[:4] == b'3DSX'

for name in ['Checkpoint.cia', 'Checkpoint.3dsx', 'Checkpoint.smdh']:
    shutil.copyfile(root / '3ds/out' / name, output / name)

paths = subprocess.check_output(['git', 'ls-files', '--recurse-submodules', '-z'], cwd=root).decode('utf-8').split('\0')
with zipfile.ZipFile(output / 'Checkpoint-JP-source.zip', 'w', zipfile.ZIP_DEFLATED) as archive:
    for name in paths:
        if name and (root / name).is_file():
            archive.write(root / name, 'Checkpoint-JP/' + name)

files = sorted(path for path in output.iterdir() if path.is_file() and path.name != 'SHA256SUMS')
(output / 'SHA256SUMS').write_text(''.join(hashlib.sha256(path.read_bytes()).hexdigest() + '  ' + path.name + '\n' for path in files), encoding='utf-8')
print(f'CIA verified: {title_id:016X}, version {title_version}, {len(cia)} bytes')
