#!/usr/bin/env python3
"""Report V/E/F, non-manifold and boundary edge counts for OBJ meshes.

Dev utility: `python scripts/count_nm.py mesh.obj [mesh2.obj ...]`.
The library reports the same two counts through dualc::Diagnostics; this is
for looking at a file that is already on disk.
"""
import sys
from collections import Counter

def count(path):
    edges = Counter()
    nfaces = 0
    verts  = set()
    with open(path) as f:
        for line in f:
            if line.startswith('v '):
                verts.add(line)
            elif line.startswith('f '):
                nfaces += 1
                toks = line.split()[1:]
                vs = [int(t.split('/')[0]) for t in toks]
                for i in range(len(vs)):
                    a, b = vs[i], vs[(i + 1) % len(vs)]
                    edges[(min(a, b), max(a, b))] += 1
    nv = len(verts)
    ne = len(edges)
    nm = sum(1 for c in edges.values() if c > 2)
    bd = sum(1 for c in edges.values() if c == 1)
    euler = nv - ne + nfaces
    print(f"{path}: V={nv} E={ne} F={nfaces}  non-manifold={nm}  boundary={bd}  euler={euler}")

for p in sys.argv[1:]:
    count(p)