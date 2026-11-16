#!/bin/sh
# Regenerates the data lifter tables (src/port/generated/zero_lift_data.c, abs_syms.S, zero_aliases.ld).
# Needs: a built pokestadium0 checkout (`make init && make rom`: its per-image link maps), a configured build/ here
# whose libraries are built (`ninja -C build pokestadium` may fail to link; the objects are enough), and libclang
# (`pip install libclang`).
# Usage: tools/zero/regen_lift.sh <pokestadium0 checkout>
set -e
root=$(cd "$(dirname "$0")/../.." && pwd)
tmp=$(mktemp -d)
python3 "$root/tools/zero/read_maps.py" "$1" "$tmp/syms.json"
nm "$root/build/libstadium_game.a" > "$tmp/nm.txt"
awk 'NF==2 && $1=="U"{print $2}' "$tmp/nm.txt" | sort -u > "$tmp/u.txt"
awk 'NF==3 && $2 ~ /[TDBRCSVW]/{print $3}' "$tmp/nm.txt" | sort -u > "$tmp/d.txt"
# Also every ROM symbol the sources name: a reference the optimizer drops at one -O level is kept at another
grep -rhowE '[A-Za-z_][A-Za-z0-9_]*' "$root/game/src" "$root/game/include" 2>/dev/null | sort -u > "$tmp/ids.txt"
python3 -c "
import json,sys
d=json.load(open(sys.argv[1])); names=set()
for v in d.values(): names.update(v.get('defined',{})); names.update(v.get('provided',{}))
ids=set(open(sys.argv[2]).read().split())
print('\\n'.join(sorted(names & ids)))" "$tmp/syms.json" "$tmp/ids.txt" > "$tmp/named.txt"
sort -u "$tmp/u.txt" "$tmp/named.txt" | comm -23 - "$tmp/d.txt" > "$tmp/undef.txt"
# names the rest of the host already defines (everything but the generated storage)
( cd "$root/build" && nm libport_os.a libultraship/src/libultraship.a libultraship/libImGui.a \
    $(find CMakeFiles/pokestadium.dir -name '*.o' ! -name 'zero_lift_data.c.o') libstadium_game.a 2>/dev/null ) |
    awk 'NF==3 && $2 ~ /[TDBRCSVW]/ {print $3}' | sort -u > "$tmp/host_defined.txt"
python3 "$root/tools/zero/collect_decls.py" "$tmp/undef.txt" "$tmp/decls.json"
python3 "$root/tools/zero/gen_lift.py" "$tmp/syms.json" "$tmp/decls.json" "$tmp/nm.txt" "$tmp/undef.txt" \
    "$root/src/port/generated/zero_lift_data.c" "$root/src/port/generated/abs_syms.S" "$tmp/host_defined.txt"
rm -rf "$tmp"
