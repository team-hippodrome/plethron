# Windows ABI audit

The port was developed and tested on Linux and macOS. These checks find code that only works there.

- `layouts.py`: every record whose layout differs between x86_64 Linux and x86_64 MinGW (Windows). It found the
  Microsoft bitfield layout (SequencePlayer), which CMakeLists.txt now turns off for the game (`-mno-ms-bitfields`).
- `abi_calls.py`: direct calls whose arguments arrive under the SysV ABI but not under Win64, most often through a
  `func_X` alias declared with other parameter types than the definition. It found the 119 calls of
  `func_80029028` (`Trade_DrawSolidFillRect`) that passed a 4-byte color where a 3-byte struct was declared: Win64
  passes the 3-byte struct by reference, so the callee read the color as a pointer (the main menu crash).

```sh
ninja -C build -t compdb > /tmp/cdb.json                      # a configured Linux build
MINGW_CC=<llvm-mingw>/bin/x86_64-w64-mingw32-clang python3 tools/abi_audit/layouts.py /tmp/cdb.json
python3 tools/abi_audit/abi_calls.py collect /tmp/cdb.json /tmp/abi.json
python3 tools/abi_audit/abi_calls.py report src/port/generated/zero_aliases.ld /tmp/abi.json | grep WIN64
```

Both should print nothing (after `errors: 0` for the first). Indirect calls (function pointers) are not covered.
