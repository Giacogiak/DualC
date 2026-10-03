# Tool 8: `dualc_view` (interactive Polyscope viewer)

**Retired 2026-10-03.** `dualc_view`, the Polyscope-based re-contouring viewer, was removed
together with the Polyscope dependency; tool number 8 is never reused. The record of the
tool as it was, and of the decision to retire it, is
[roadmap 07 #50](../roadmap/07-viewer-polyscope.md#50-retire-dualc_view-and-the-polyscope-dependency-own-glfw--glad);
its decision row (D-48) is owed to [the settled decisions](../decisions/01-settled.md).

What replaces each of its uses:

| Was | Now |
| --- | --- |
| Live-tune a lattice, primitive, boolean or CSG recipe and watch the contour | [`dualc_field_view`](12-dualc_field_view/README.md): write the same thing as a field-graph expression, edit the file, the viewer recompiles on save — no re-contour, so it stays live at any density. |
| Produce the mesh you tuned | [`dualc_field`](11-dualc_field/README.md) on the same expression (`-o`). |
| Inspect a demo mesh through the contourer | [`dualc_demo`](01-dualc_demo.md) to a file, then any mesh viewer. |
| Octree-leaf, Hermite-crossing and sign-oracle overlays | No replacement: they drew the contourer's internals, and `dualc_field_view` never contours. |

Its four modes map to expressions `dualc_field_view` takes directly
([op vocabulary](11-dualc_field/01-op-vocabulary.md)), for example:

```bash
# lattice mode: a gyroid shell clipped to the sample cube
dualc_field_view --expr "intersection(mesh(path=\"cube.obj\"),onion(normalize(gyroid(wavelength=0.5)),thickness=0.05))"
# primitive mode with post-ops
dualc_field_view --expr "twist(onion(sphere(radius=1),thickness=0.1),radiansPerUnit=1.5,axis=\"y\")"
# boolean mode
dualc_field_view --expr "smooth-union(mesh(path=\"cube.obj\"),sphere(radius=1.2),k=0.3)"
```

---

← Back to the [Command Reference index](README.md) for the build prelude and common options.
