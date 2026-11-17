# macOS crash at start: Metal shaders lost the `thread` qualifier on reference parameters

- **Category:** platform-build
- **Zero status:** already ported (plethron's `tools/make_o2r.py:30` has stadion's narrowed regex)
- **stadion:** [PR #5](https://github.com/team-hippodrome/stadion/pull/5), commit(s) `442af54`
- **stadion files:** `tools/make_o2r.py`

## Symptom
The CI-built macOS app (and the v0.0.1 download) aborted at frame 2: its Metal shaders failed to compile with
"reference type must have explicit address space qualifier".

## Root cause
`tools/make_o2r.py` strips `thread ` from every `.metal` shader packed into the .o2r, because the macOS 27 compiler
rejects address-space qualifiers on by-value helper parameters. Binaries built with an older SDK (macOS 15 CI runner)
compile shaders under an older Metal language version, which *requires* `thread` on reference parameters. Stripping
it everywhere broke them.

## Fix in stadion
Strip only before a by-value parameter (`thread const T name`), keep it on references (`thread const T& name`):
```python
text = re.sub(r"\bthread (const \w+(?:<\w+>)? )(?=\w)", r"\1", text)
```

## Zero / plethron
`tools/make_o2r.py:23-30` in plethron already uses the narrowed regex (copied with the port). Nothing to do.

## How to detect
Build the macOS binary on an older SDK and run once; Metal compile errors are logged by libultraship at shader
creation. `grep -n "thread" tools/make_o2r.py` should show the `(?=\w)` lookahead form.
