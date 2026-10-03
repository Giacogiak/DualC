# Analysis: motivation, the unifying insight, the two options, and why graded wavelength is a PDE

Part of [13 — Graded TPMS](README.md). The reasoning that led to the decision: graded
thickness is local and cheap; graded wavelength is a global phase-field solve and out of
scope.

## Motivation

Today every TPMS lattice ([05](../05-tpms-lattices/README.md)) uses **uniform** periodic
parameters across its clipped volume: one `--wavelength λ` and one `--offset T`.
Graded lattices — denser/thicker near a load path or a surface, sparser/thinner
elsewhere — are a standard manufacturing ask and are not expressible. The goal
is a single coherent surface whose wavelength and/or thickness transition
smoothly according to a defined criterion.

## The unifying insight

Two superficially different approaches were proposed:

- **Option A** — one TPMS whose parameters are graded by a criterion (e.g.
  distance from the bounding surface, or from a point).
- **Option B** — two simple TPMS generated in two intersecting volumes, fed into
  a "blend" operation with falloff parameters, yielding one transitioned surface.

**Both are the same operation underneath: "modulate a TPMS's parameters by a
scalar control value `s(p)`."** The only differences are *what defines `s(p)`*
and *how the result is assembled*. Critically, **the criterion is just another
field, remapped**:

| Criterion | Control field `s(p)` |
| --- | --- |
| Distance from the bounding surface | the clip volume's own SDF (zero extra input) |
| Distance from a point | a sphere / point-distance SDF centred there |
| Linear along an axis | a plane SDF |
| Arbitrary | any field node in the graph |

This reduces the whole feature to: **a graded TPMS node that reads a control
field, remaps it (`control range → parameter range`), and uses the result as a
local wavelength and/or thickness.** Recommended input modality: the control is
a **graph child** (most composable, on-philosophy), with an implicit "distance to
bounds" default for zero-config first validation.

## Evaluation of the two options

**Option A — single graded TPMS. RECOMMENDED.** Produces *one coherent surface*
with locally-varying parameters. No interference artifacts. Fits the field-graph
(control = a child node) and live-edits well (remap min/max become uniforms →
instant slider feedback, no recompile).

**Option B — blend two TPMS. NOT RECOMMENDED.** "Blend" is undefined here, and
every cheap definition fails:

- **Value-lerp** of two gyroids of different wavelength → **beating / moiré
  interference** in the transition band. Two incommensurate frequencies summed
  produce beats; the zero-set of a lerp is *not* the lerp of the zero-sets.
- **Hard spatial select** → a **seam discontinuity** at the boundary.
- The only blend that *would* be clean is Option A's phase-modulation machinery
  applied to one field — i.e. it collapses back into A.

So B is easier to wire (reuse `smooth-union`) but yields a *worse* artifact than
A and is hard to make watertight in the transition.

## Complexity — the honest headline

- **Thickness grading: LOW.** A clean decorator — `|f| − t(p)` where `t(p)` is
  the remapped control. Trivial in C++ and GLSL; the natural **first
  deliverable** and an immediate visual win.
- **Wavelength grading: MODERATE, with a math caveat.** The naive `k → k(p)`
  substitution is easy but introduces **chirp distortion**: writing the phase as
  `φ₁ = k(p)·x` gives an instantaneous frequency `∇φ₁ = k(p)·(1,0,0) + x·∇k(p)`,
  so the realized wavelength is *not* `k(p)` — it is contaminated by the `x·∇k`
  term. The distortion is small for gentle gradients (the usual graded-lattice
  regime) and objectionable for steep ones. This is what essentially every
  graded-TPMS tool ships, and it is the realistic v1.

There is **no cheap middle ground**: the "integrated-phase" trick
(`x → ∫k dx`) only works for grading along a *single axis* on a *separable*
field. The gyroid is coupled (sin·cos cross-terms) and the first-validation
criterion (distance-from-surface) is not axis-aligned, so the integral does not
separate. We jump straight from "naive with bounded chirp" to the full
phase-field solve below.

## Why the fully-correct version is a PDE phase-field solve — and out of scope

The correct formulation does not prescribe the phase as a formula; it prescribes
the phase's **gradient magnitude** and solves for the phase. We want a field
`φ(p)` with `|∇φ(p)| = k_target(p)` everywhere (optionally with a prescribed
direction). That equation is the **eikonal equation** — the same PDE a distance
field satisfies. Four reasons it is out of scope for v1:

1. **It is a global PDE solve, not a local function — incompatible with the
   pipeline.** Every field-graph node is a pure local `valueAt(p)`, which is
   exactly what lets `field_glsl` compile each node to a closed-form
   `float fN(vec3 p)` GLSL snippet. The eikonal solution depends on boundary
   conditions and the `k`-field *everywhere*, not just at `p`. It must be solved
   offline (fast marching / sweeping / optimization) and baked into a 3D texture
   — the deferred `GridField` machinery, but heavier — throwing away the
   analytic, exact, recompile-free, live-editable properties that motivate doing
   this in `dualc_field_view`.

2. **A gyroid needs three coupled phase fields, and they generally don't
   exist.** The surface is three sinusoids whose gradients form an orthonormal
   frame scaled by `k`. Grading correctly needs three scalar functions whose
   gradients stay mutually orthogonal with magnitude `k(p)`. A gradient field
   must be curl-free (`∇×∇φ = 0`), so an arbitrary orthonormal-frame-scaled-by-`k`
   usually has *no* corresponding potentials. You don't solve it — you *relax* it
   (minimize deviation from the ideal frame). This is the **frame-field /
   seamless-parameterization** problem from quad-meshing research: an
   optimization with its own literature, not a formula.

3. **Frame fields have singularities and orientation/BC handling.** The frame
   cannot always be combed consistently across the domain (singularities, like
   cowlicks), and boundary alignment (lattice aligned to the surface? to an
   axis?) adds boundary-condition handling. Robustly handling these is active
   research.

4. **No cheap middle ground for the stated case.** As above, integrated-phase
   only separates for a single axis on a separable field; the coupled gyroid
   under a non-axis-aligned criterion does not qualify, so there is no
   analytic-but-correct version between "naive with chirp" and the full solve.

**Takeaway.** "Out of scope" is not "too much code" — it is *a different kind of
problem* (global PDE + frame-field optimization + baked artifacts) that
contradicts the very properties — local, analytic, live, exact — that make
`dualc_field_view` worth targeting. Naive `k(p)` stays a pure local analytic
node (live-editable, parity-testable) with bounded distortion that is small in
the normal graded-lattice regime; the eikonal/frame-field version only earns its
keep if a real engineering requirement for *dimensionally accurate* graded cell
sizes (e.g. a stiffness-gradient spec) appears — not for visualization.

---

← Back to the [topic README](README.md) · the [Roadmap index](../README.md).
