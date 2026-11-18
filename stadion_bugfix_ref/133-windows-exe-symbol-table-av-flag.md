# Windows release exe kept its COFF symbol table (read as an overlay, flagged by an AV engine)

- **Category:** platform-build
- **Zero status:** already ported (plethron `tools/release.sh` and `build.yml` use `--strip-all`)
- **stadion:** [PR #133](https://github.com/team-hippodrome/stadion/pull/133), commit(s) `b166ccb`
- **stadion files:** `tools/release.sh`, `.github/workflows/build.yml`

## Symptom
A user got a virus warning downloading the Windows release; VirusTotal tagged a 3.6 MB overlay and one ML engine
flagged the exe.

## Root cause
`llvm-strip --strip-debug` leaves the COFF symbol table after the last section; scanners see it as appended data.
The CI build shipped full debug info.

## Fix in stadion
Shipped exe uses `llvm-strip --strip-all`; a `--strip-debug` copy goes to `releases/v<ver>/symbols/` (unpublished).
CI strips before upload.

## Zero / plethron
Present: `tools/release.sh:140-143` (`--strip-debug` to `symbols/`, `--strip-all` for the shipped exe) and
`.github/workflows/build.yml:155` (`llvm-strip --strip-all build/pokestadium.exe`).

## How to detect
`llvm-objdump -h pokestadium.exe` / VirusTotal "overlay" on the release exe; size drop after `--strip-all`.
