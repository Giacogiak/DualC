#!/usr/bin/env python3
"""bench_e12.py -- the GridField-normal measurement round (roadmap 17 #34, #11).

Runs the record's cases (17/20) through an *old* and a *new* `dualc_field`
binary, times each run (/usr/bin/time: wall median of 3, peak RSS max of 3),
then measures every output with `dualc_mesh_deviation` (-DDUALC_BUILD_BENCH=ON):
|valueAt| of the exact graph at every vertex, the dihedral histogram, the shift
of each vertex between old and new. Prints the three markdown tables in the
record's column layout, so a run pastes into the roadmap unchanged.

    python3 scripts/bench_e12.py --old /path/to/old/dualc_field \\
        --new build/examples/dualc_field [--tool build/examples/dualc_mesh_deviation]
        [--data data] [--out /tmp/bench] [--cases A,B,C,D,E] [--runs 3]

Pass the same binary twice to check the tool against the record's rows.
Stdlib only; Linux/macOS (needs /usr/bin/time -v or -l).
"""
import argparse
import json
import os
import platform
import re
import statistics
import subprocess
import sys

# The record's cases: name, the contoured graph, the exact graph (no bake),
# the octree depth. A..D are 17/20's; E is oversampled (about four DC cells
# per bake cell), where the normal's jump across bake-cell faces is visible.
CASES = [
    ("A", 'difference(mesh(path="opA.obj",bake=128),mesh(path="opB.obj",bake=128))',
          'difference(mesh(path="opA.obj"),mesh(path="opB.obj"))', 7),
    ("B", 'smooth-union(mesh(path="torus.obj",bake=64),mesh(path="cube.obj",bake=64),k=1)',
          'smooth-union(mesh(path="torus.obj"),mesh(path="cube.obj"),k=1)', 6),
    ("C", 'difference(mesh(path="sphere.obj",bake=64),mesh(path="cube.obj",bake=64))',
          'difference(mesh(path="sphere.obj"),mesh(path="cube.obj"))', 6),
    ("D", 'mesh(path="bunny.obj",bake=128)', 'mesh(path="bunny.obj")', 7),
    ("E", 'mesh(path="sphere.obj",bake=16)', 'mesh(path="sphere.obj")', 6),
]
BINS = ["dihedral_lt1", "dihedral_1_5", "dihedral_5_10", "dihedral_10_20",
        "dihedral_20_45", "dihedral_45_90", "dihedral_ge90"]
MB = 1 << 20


def timed_run(binary, expr, depth, out, cwd):
    """One contour run under /usr/bin/time; returns (wall s, peak RSS MB)."""
    mac = platform.system() == "Darwin"
    cmd = ["/usr/bin/time", "-l" if mac else "-v", os.path.abspath(binary),
           "--expr", expr, "--depth", str(depth), "--threads", "0", "-o", out]
    p = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True)
    if p.returncode != 0:
        sys.exit("%s failed:\n%s" % (" ".join(cmd), p.stderr[-2000:]))
    err = p.stderr
    if mac:
        wall = float(re.search(r"([\d.]+)\s+real", err).group(1))
        rss = int(re.search(r"(\d+)\s+maximum resident set size", err).group(1)) / MB
    else:
        m = re.search(r"Elapsed \(wall clock\) time.*?:\s*([\d:.]+)", err)
        parts = [float(x) for x in m.group(1).split(":")]
        wall = sum(v * 60 ** i for i, v in enumerate(reversed(parts)))
        rss = int(re.search(r"Maximum resident set size \(kbytes\):\s*(\d+)", err)
                  .group(1)) * 1024 / MB
    return wall, rss


def measure(tool, mesh, exact, cwd, old=None):
    cmd = [os.path.abspath(tool), mesh, "--expr", exact, "--json"]
    if old:
        cmd += ["--old", old]
    p = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True)
    if p.returncode != 0:
        sys.exit("%s failed:\n%s" % (" ".join(cmd), p.stderr[-2000:]))
    return json.loads(p.stdout)


def fmt(x, digits=4):
    return ("%%.%dg" % digits) % x


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--old", required=True, help="the reference dualc_field binary")
    ap.add_argument("--new", required=True, help="the dualc_field binary under test")
    ap.add_argument("--tool", default="build/examples/dualc_mesh_deviation")
    ap.add_argument("--data", default="data", help="folder holding the input OBJs")
    ap.add_argument("--out", default="/tmp/bench_e12", help="where the meshes land")
    ap.add_argument("--cases", default="A,B,C,D,E")
    ap.add_argument("--runs", type=int, default=3, help="timed runs per binary")
    a = ap.parse_args()

    for b in (a.old, a.new, a.tool):
        if not os.path.isfile(b):
            sys.exit("not a file: %s" % b)
    os.makedirs(a.out, exist_ok=True)
    out = os.path.abspath(a.out)
    want = set(a.cases.split(","))

    rows = []
    for name, graph, exact, depth in CASES:
        if name not in want:
            continue
        print("case %s ..." % name, file=sys.stderr)
        res = {"case": name, "depth": depth}
        meshes = {}
        for tag, binary in (("old", a.old), ("new", a.new)):
            mesh = os.path.join(out, "%s_%s.obj" % (name, tag))
            walls, rsss = [], []
            for _ in range(a.runs):
                w, r = timed_run(binary, graph, depth, mesh, a.data)
                walls.append(w)
                rsss.append(r)
            res[tag] = {"wall": statistics.median(walls), "rss": max(rsss)}
            meshes[tag] = mesh
        res["old"].update(measure(a.tool, meshes["old"], exact, a.data))
        res["new"].update(measure(a.tool, meshes["new"], exact, a.data,
                                  old=meshes["old"]))
        rows.append(res)

    # Table 1: wall / RSS / faces / verts, 17/20's layout.
    print("| Case | Wall s old / new | RSS MB old / new | Faces | Verts | Faces identical |")
    print("| --- | --- | --- | ---: | ---: | --- |")
    for r in rows:
        o, n = r["old"], r["new"]
        print("| %s (depth %d) | %.2f / %.2f | %.0f / %.0f | %s | %s | %s |" % (
            r["case"], r["depth"], o["wall"], n["wall"], o["rss"], n["rss"],
            "{:,}".format(n["faces"]), "{:,}".format(n["vertices"]),
            "yes" if n.get("faces_identical") else "**no**"))
    print()
    # Table 2: deviation and shift.
    print("| Case | Dev max old → new | Dev mean old → new | Shift max / mean |")
    print("| --- | --- | --- | --- |")
    for r in rows:
        o, n = r["old"], r["new"]
        dm = (n["dev_mean"] / o["dev_mean"] - 1.0) * 100.0 if o["dev_mean"] else 0.0
        print("| %s | %s → %s | %s → %s (%+.0f %%) | %s / %s |" % (
            r["case"], fmt(o["dev_max"]), fmt(n["dev_max"]),
            fmt(o["dev_mean"]), fmt(n["dev_mean"]), dm,
            fmt(n.get("shift_max", 0.0), 3), fmt(n.get("shift_mean", 0.0), 3)))
    print()
    # Table 3: dihedral histogram old → new.
    print("| Case | < 1 | 1–5 | 5–10 | 10–20 | 20–45 | 45–90 | ≥ 90 |")
    print("| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |")
    for r in rows:
        o, n = r["old"], r["new"]
        cells = ["{:,} → {:,}".format(o[b], n[b]) for b in BINS]
        print("| %s | %s |" % (r["case"], " | ".join(cells)))
    print()
    print("raw: %s" % json.dumps(rows))


if __name__ == "__main__":
    main()
