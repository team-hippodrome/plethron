#!/usr/bin/env python3 -I
"""Runs every route (or the ones named) on the port and on ares, then scores them into one report.

Usage: tools/screen_diff/run_all.py <out dir> [route ...] [--jobs N] [--compare-only]
Routes default to tools/screen_diff/routes/*.route. Each route's pictures go to <out>/<route name>/, the ranked report
to <out>/report/report.md (with report.json and the port | ares | difference pictures in <out>/report/cmp/).
--jobs: routes run at the same time (default 3; each ares run takes one core, the port another).
Environment as tools/screen_diff/run.py (ROM, ARES_REF_BUILD, ARES_REF_SRC, GB_ROM_GREEN, GB_SAV_GREEN, ...).
"""
import glob, os, subprocess, sys
from concurrent.futures import ThreadPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))


def main():
    args = sys.argv[1:]
    if not args:
        sys.exit(__doc__)
    jobs = 3
    if "--jobs" in args:
        i = args.index("--jobs")
        jobs = int(args[i + 1])
        del args[i:i + 2]
    compare_only = "--compare-only" in args
    args = [a for a in args if a != "--compare-only"]
    out = os.path.abspath(args[0])
    routes = args[1:] or sorted(glob.glob(os.path.join(HERE, "routes", "*.route")))
    os.makedirs(out, exist_ok=True)
    runs = [os.path.join(out, os.path.splitext(os.path.basename(r))[0]) for r in routes]

    def one(route, run):
        with open(run + ".log", "w") as log:
            p = subprocess.run([sys.executable, "-I", os.path.join(HERE, "run.py"), route, run], stdout=log,
                               stderr=subprocess.STDOUT)
        print(open(run + ".log").read().strip().splitlines()[-1] if p.returncode == 0 else "%s: failed (%s.log)" % (route, run))

    if not compare_only:
        with ThreadPoolExecutor(jobs) as ex:
            list(ex.map(one, routes, runs))
    subprocess.run([sys.executable, "-I", os.path.join(HERE, "compare.py"), os.path.join(out, "report")] +
                   [r for r in runs if os.path.exists(os.path.join(r, "route.txt"))], check=True)


if __name__ == "__main__":
    main()
