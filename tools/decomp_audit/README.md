# Decomp audit

Compares the port's copy of the game code (`game/src`) with the decomp it was taken from, to find code the port
deleted or changed by accident. Results of the first run: [docs/decomp-divergence-audit.md](../../docs/decomp-divergence-audit.md).

Setup (once per decomp commit; the commit is in `game/UPSTREAM.md`):

```bash
mkdir -p /tmp/decomp && git -C ../pokestadium0 archive 35d7a61 src linker_scripts | tar -x -C /tmp/decomp
```

- `audit.py /tmp/decomp/src --symbols /tmp/decomp/linker_scripts/jp/symbol_addrs.txt -o build/decomp_audit.txt`
  lists, per function: definitions missing from the port, lost statements/assignments/branches/calls (`structure`),
  added early returns (`returns`), statements that differ only in a constant (`constants`) and decomp globals the
  function no longer names (`globals`). Names are compared through `symbol_addrs.txt`, so `func_8003834C` and its real
  name count as the same call. Pass the `symbol_addrs.txt` of every decomp version whose names the port uses.
  Every `structure` line needs a reason. If it has none, restore the code from the decomp and mark it
  `// PORT: restored`.
- `offsets.py /tmp/decomp/src` checks the rewrites of `M2C_FIELD(p, T*, K)` into `p->field` against the N64 layout
  (clang on a 32-bit big-endian MIPS target, `tools/mips_sysroot`). `FAIL` lines are fields at another offset than
  the decomp read. Some are on purpose (a `u8` read of an s16's low byte), the rest are bugs. Needs clang and
  glibc's headers (`/usr/include/x86_64-linux-gnu`); it writes a temporary `*__offsets_check.c` next to each source
  while it runs.

Both take `--port` to point at another tree.
