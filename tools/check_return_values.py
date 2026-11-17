#!/usr/bin/env python3 -I
"""Finds calls whose result the N64 took from whatever v0 held: callers that use the value of a function that does not
return one.

The decomp keeps IDO-era code where a function has no return statement (or is defined void) while callers use its
result. On the N64 the caller then reads v0, which still holds what the callee's last call (or last v0 temporary) left
there; on the PC the value is garbage. BattleCalc_EstimateDamage (PR #36) made the COM pick different moves this way.

Shapes checked, across every game translation unit (prototypes and func_X aliases from zero_aliases.ld resolved):
  missing  a non-void definition that clang's -Wreturn-type flags (falls off the end on some path)
  void     a void definition that another file declares non-void, or whose address is stored or passed as a
           function pointer that returns a value (e.g. a frame function cast for BgStage_RunUntilCondition)

Every direct call site is classified as using the value or discarding it. A function with a site that uses the value,
or stored as a value-returning pointer, fails the check unless its definition (or the 8 lines above it) carries a
`// PORT: v0` comment saying why that is fine. Fix one by returning what the N64 left in v0 (disassemble the matched
decomp: usually the last call's result), and mark it `// PORT: v0 ...`. Functions nobody reads the value of are listed
as harmless and left as the decomp has them.

The report also lists calls through function pointers that use the result, for checking the targets of a new
dispatcher by hand (targets that come from ROM data tables are not typed in the source).

Usage: tools/check_return_values.py [--jobs N] [--report FILE] [files...]   exit status 1 on a failure.
Needs clang (-ast-dump=json: clang 9 or newer) and the libultraship submodule (its headers).
"""
import argparse, collections, json, multiprocessing, os, pathlib, re, subprocess, sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
FLAGS = [f for f in (ROOT / "tools/compile_flags.txt").read_text().split("\n") if f]
WARN = re.compile(r"^(game/[^:]+):(\d+):\d+: warning: non-void function does not return a value( in all control paths)?")
DISCARD_PARENTS = {"CompoundStmt", "LabelStmt", "CaseStmt", "DefaultStmt", "AttributedStmt"}
PASS = {"ParenExpr", "ImplicitCastExpr", "FullExpr", "ConstantExpr"}


def sources():
    out = [p for p in (ROOT / "game/src").rglob("*.c") if "libultra" not in p.relative_to(ROOT / "game/src").parts[:1]]
    return sorted(str(p.relative_to(ROOT)) for p in out)


def aliases():
    table = {}
    path = ROOT / "src/port/generated/zero_aliases.ld"
    if path.exists():
        for m in re.finditer(r"^(\w+)\s*=\s*(\w+)\s*;", path.read_text(), re.M):
            table[m.group(1)] = m.group(2)
    return table


def ret_type(qual):
    return qual.split("(", 1)[0].strip()


def walk_function(body, fname, calls):
    """Records (callee, used, line) for each direct call in a function body."""
    line = [0]

    def visit(node, ctx):  # ctx: "used" or "discard"
        if not isinstance(node, dict) or not node:
            return
        loc = node.get("loc") or (node.get("range") or {}).get("begin") or {}
        if "line" in loc and "includedFrom" not in loc and "spellingLoc" not in loc:
            line[0] = loc["line"]
        elif "expansionLoc" in loc and "line" in loc["expansionLoc"]:
            line[0] = loc["expansionLoc"]["line"]
        kind = node.get("kind", "")
        inner = node.get("inner", [])
        if kind == "CallExpr" and inner:
            callee = inner[0]
            while callee.get("kind") in PASS and callee.get("inner"):
                callee = callee["inner"][0]
            ref = callee.get("referencedDecl", {})
            if callee.get("kind") == "DeclRefExpr" and ref.get("kind") == "FunctionDecl":
                calls.append((ref["name"], ctx == "used", line[0], fname))
            elif ctx == "used" and node.get("type", {}).get("qualType") != "void":
                calls.append(("<indirect> " + node.get("type", {}).get("qualType", "?"), True, line[0], fname))
            for c in inner:
                visit(c, "used")
            return
        if kind in PASS:
            for c in inner:
                visit(c, ctx)
            return
        if kind == "CStyleCastExpr":
            sub = "discard" if node.get("type", {}).get("qualType") == "void" else "used"
            for c in inner:
                visit(c, sub)
            return
        if kind == "BinaryOperator" and node.get("opcode") == ",":
            visit(inner[0], "discard")
            visit(inner[1], ctx)
            return
        if kind in DISCARD_PARENTS:
            for c in inner:
                visit(c, "discard" if c.get("kind", "").endswith("Expr") or c.get("kind") == "CallExpr" else ctx)
            return
        if kind in ("IfStmt", "WhileStmt", "SwitchStmt"):
            for i, c in enumerate(inner):
                visit(c, "used" if i == 0 else "discard")
            return
        if kind == "DoStmt":
            for i, c in enumerate(inner):
                visit(c, "used" if i == len(inner) - 1 else "discard")
            return
        if kind == "ForStmt":  # init, condition variable, cond, inc, body ({} when absent)
            for i, c in enumerate(inner):
                visit(c, "used" if i == 2 else "discard")
            return
        if kind.endswith("Stmt") and kind != "ReturnStmt":
            for c in inner:
                visit(c, "discard")
            return
        for c in inner:
            visit(c, "used")

    visit(body, "discard")


def collect_refs(node, out, cast=None):
    """Functions whose address is taken (a DeclRefExpr that is not called), with the type the outermost cast around it
    converts the pointer to (the slot's function-pointer type when it is stored or passed with another signature)."""
    if not isinstance(node, dict):
        return
    kind = node.get("kind")
    inner = node.get("inner", [])
    if kind == "CallExpr" and inner:
        callee = inner[0]
        while callee.get("kind") in PASS and callee.get("inner"):
            callee = callee["inner"][0]
        for c in (inner[1:] if callee.get("kind") == "DeclRefExpr" else inner):
            collect_refs(c, out)
        return
    if kind == "DeclRefExpr":
        ref = node.get("referencedDecl", {})
        if ref.get("kind") == "FunctionDecl":
            out.add((ref["name"], cast or ref.get("type", {}).get("qualType", ""), node.get("_line", 0)))
        return
    if kind in ("ImplicitCastExpr", "CStyleCastExpr", "ParenExpr"):
        t = node.get("type", {})
        here = (t.get("desugaredQualType") or t.get("qualType")) if kind != "ParenExpr" else None
        for c in inner:
            collect_refs(c, out, cast or here)
        return
    for c in inner:
        collect_refs(c, out)


def analyse(src):
    cmd = [os.environ.get("CLANG", "clang"), "-fsyntax-only", *FLAGS, "-Wno-everything", "-Wreturn-type",
           "-Xclang", "-ast-dump=json", src]
    proc = subprocess.run(cmd, cwd=ROOT, capture_output=True)
    warns = []
    for l in proc.stderr.decode(errors="replace").splitlines():
        m = WARN.match(l)
        if m:
            warns.append((m.group(1), int(m.group(2)), bool(m.group(3))))
    errors = [l for l in proc.stderr.decode(errors="replace").splitlines() if ": error:" in l]
    tree = json.loads(proc.stdout) if proc.stdout else {"inner": []}
    defs, decls, calls, refs = [], collections.defaultdict(set), [], set()
    cur_file = src

    def scan(nodes):
        nonlocal cur_file
        for n in nodes:
            loc = n.get("loc", {})
            if "file" in loc:
                cur_file = loc["file"]
            elif "spellingLoc" in loc and "file" in loc["spellingLoc"]:
                cur_file = loc["spellingLoc"]["file"]
            if n.get("kind") == "FunctionDecl":
                qual = n.get("type", {}).get("qualType", "")
                decls[n["name"]].add(ret_type(qual))
                body = [c for c in n.get("inner", []) if c.get("kind") == "CompoundStmt"]
                if body and cur_file == src:
                    end = (n.get("range", {}).get("end") or {})
                    defs.append({"name": n["name"], "ret": ret_type(qual), "file": src, "line": loc.get("line"),
                                 "end": end.get("line"), "static": n.get("storageClass") == "static"})
                    walk_function(body[0], n["name"], calls)
                    collect_refs(body[0], refs)
                    # local (block-scope) prototypes
                    def local_decls(x):
                        for c in x.get("inner", []) if isinstance(x, dict) else []:
                            if c.get("kind") == "FunctionDecl":
                                decls[c["name"]].add(ret_type(c.get("type", {}).get("qualType", "")))
                            local_decls(c)
                    local_decls(body[0])
            elif n.get("kind") == "VarDecl" and cur_file == src:
                collect_refs(n, refs)
            elif n.get("kind") in ("LinkageSpecDecl",):
                scan(n.get("inner", []))

    scan(tree.get("inner", []))
    # each definition's end line, for matching -Wreturn-type locations (reported at the closing brace)
    return {"src": src, "warns": warns, "defs": defs, "decls": {k: sorted(v) for k, v in decls.items()},
            "calls": calls, "refs": sorted(refs), "errors": errors[:5]}


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--jobs", type=int, default=os.cpu_count())
    ap.add_argument("--report", help="write the full classification (all sites, used or not) here")
    ap.add_argument("files", nargs="*", help="limit to these sources (default: every game source)")
    args = ap.parse_args()
    if not (ROOT / "libultraship/include/libultraship").exists():
        sys.exit("libultraship submodule missing: git submodule update --init libultraship")
    files = args.files or sources()
    with multiprocessing.Pool(args.jobs) as pool:
        results = pool.map(analyse, files)
    alias = aliases()
    canon = lambda n: alias.get(n, n)
    broken = [r for r in results if r["errors"]]
    for r in broken:
        print(f"{r['src']}: does not compile here ({r['errors'][0]})", file=sys.stderr)

    defs = {}
    for r in results:
        ends = {(f, l): inc for f, l, inc in r["warns"]}
        for d in r["defs"]:
            d["missing"] = None
            for (f, l), inc in ends.items():
                if f == d["file"] and d["line"] and d["end"] and d["line"] <= l <= d["end"]:
                    d["missing"] = "some paths" if inc else "all paths"
            key = (d["file"], d["name"]) if d["static"] else d["name"]
            defs[key] = d
    declared = collections.defaultdict(set)  # canonical name -> return types seen anywhere
    for r in results:
        for n, rets in r["decls"].items():
            declared[canon(n)].update(rets)
    uses = collections.defaultdict(list)
    for r in results:
        for callee, used, line, caller in r["calls"]:
            uses[canon(callee)].append((r["src"], line, caller, used))
    taken = collections.defaultdict(set)  # name -> {(file, pointer type)}
    for r in results:
        for n, t, _ in r["refs"]:
            taken[canon(n)].add((r["src"], t))

    def fptr_ret(t):
        m = re.match(r"^(.*?)\s*\(\*\)\s*\(", t)
        return m.group(1).strip() if m else None

    rows = []
    for key, d in sorted(defs.items(), key=lambda kv: (kv[1]["file"], kv[1]["line"] or 0)):
        name = d["name"]
        stores = sorted({(f, t) for f, t in taken[name] if fptr_ret(t) not in (None, "void")
                         and (not d["static"] or f == d["file"])})
        if d["missing"]:
            shape = "missing (" + d["missing"] + ")"
        elif d["ret"] == "void" and (any(t != "void" for t in declared[name]) or stores):
            shape = "void, " + ", ".join(
                (["declared " + "/".join(sorted(t for t in declared[name] if t != "void"))]
                 if any(t != "void" for t in declared[name]) else []) + (["stored as non-void pointer"] if stores else []))
        else:
            continue
        sites = [u for u in uses[name] if not d["static"] or u[0] == d["file"]]
        if d["ret"] == "void":  # only callers that see a non-void prototype can use the value
            sites = [u for u in sites if u[3]]
        text = (ROOT / d["file"]).read_text(errors="replace").split("\n")
        lo = max(0, (d["line"] or 1) - 8)
        marked = any("PORT: v0" in t for t in text[lo:(d["end"] or d["line"] or 1)])
        rows.append((d, shape, sites, stores, marked))

    bad = 0
    lines = []
    for d, shape, sites, stores, marked in rows:
        used = [s for s in sites if s[3]]
        verdict = ("OK (marked PORT: v0)" if marked else "BUG: value used" if used else
                   "CHECK: called through a pointer that returns a value" if stores else "harmless: no caller uses it")
        if (used or stores) and not marked:
            bad += 1
        lines.append(f"{d['file']}:{d['line']}: {d['name']} [{d['ret']}; {shape}] {verdict}")
        for s in sites:
            lines.append(f"    {'uses   ' if s[3] else 'ignores'} {s[0]}:{s[1]} in {s[2]}")
        for f, t in stores:
            lines.append(f"    stored as {t} in {f}")
    indirect = sorted((s, l, c, n[11:]) for n, sites in uses.items() if n.startswith("<indirect> ")
                      for s, l, c, u in sites if u)
    lines.append("")
    lines.append(f"Calls through function pointers that use the result ({len(indirect)}): their targets must return a value")
    for s, l, c, t in indirect:
        lines.append(f"    {s}:{l} in {c} -> {t}")
    report = "\n".join(lines) + "\n"
    if args.report:
        pathlib.Path(args.report).write_text(report)
    print(report if args.report is None else "\n".join(l for l in lines if "BUG" in l))
    summary = f"{len(rows)} functions without a return value; {bad} have callers that use (or may use) the value"
    print(summary)
    sys.exit(1 if bad or broken else 0)


if __name__ == "__main__":
    main()
