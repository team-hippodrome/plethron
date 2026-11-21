"""Check that a macOS build placed every alias of src/port/generated/zero_aliases.ld where the GNU script would.

For "a = b;" _a must have _b's address, for "a = b + off;" _b's address + off (tools/gen_macho_aliases.py writes
them for ld64). Reads the linked executable's symbol table with nm.

usage: check_macho_aliases.py <zero_aliases.ld> <executable>
"""
import pathlib
import re
import subprocess
import sys

ld, exe = pathlib.Path(sys.argv[1]), sys.argv[2]
script = re.sub(r'/\*.*?\*/', '', ld.read_text(encoding='utf-8'), flags=re.S)
out = subprocess.run(['nm', '-g', '-U', exe], check=True, capture_output=True, text=True).stdout
addr = {}
for line in out.splitlines():
    parts = line.split()
    if len(parts) == 3:
        addr.setdefault(parts[2], int(parts[0], 16))
bad = checked = 0
for line in script.splitlines():
    m = re.fullmatch(r'\s*(\w+)\s*=\s*(\w+)\s*(?:\+\s*(\d+))?\s*;\s*', line)
    if not m:
        continue
    name, target, off = '_' + m.group(1), '_' + m.group(2), int(m.group(3) or 0)
    if name not in addr or target not in addr:
        print(f'missing: {name if name not in addr else target}')
        bad += 1
    elif addr[name] != addr[target] + off:
        print(f'{name} at {addr[name]:#x}, expected {target} {addr[target]:#x} + {off}')
        bad += 1
    checked += 1
print(f'{checked} aliases checked, {bad} wrong')
sys.exit(1 if bad or not checked else 0)
