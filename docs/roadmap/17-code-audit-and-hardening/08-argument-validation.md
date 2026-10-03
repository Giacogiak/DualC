# Argument validation and the `BBox{}` sentinel (#34)

The full record for the second batch taken out of
[**#34**](04-engineering-quality.md#34-api-hygiene-batch): the audit's
*"no argument validation anywhere"* bullet, the default-constructed-`BBox`
trap, and one defect the audit had not named that adding the guards exposed.
The first batch — the `repeat` tiling fix — is
[07](07-repeat-tiling-fix.md).

## Validation — fail fast instead of returning garbage

**DONE (2026-09-10).** Every case the audit named now throws
`std::invalid_argument` **at construction**, following the precedent
`GridField` and `MeshSource::bakeToGrid` already set rather than inventing a
second convention:

| Call | Was | Now |
| --- | --- | --- |
| `SphereField(c, r)`, `r < 0` | no zero crossing anywhere; contours to nothing | throws |
| `scaled(f, 0)` | division by zero in every query | throws |
| six TPMS constructors, `wavelength <= 0` | `k = 2π/L` infinite → every `sin`/`cos` NaN | throws |
| `repeatedLimited(count < 1)` | `clampId` folded 0 to one copy | throws |
| `repeated(period < 0)` | every `s > 0` test read it as "axis not repeated" | throws |
| `transformed(f, M)`, `M` not rigid | `inverseRigid()` silently wrong | throws |
| `SamplerParams` depths negative or inverted | built anyway | throws |

The `transformed` check is `R·Rᵀ = I` to 1e-9 plus `det(R) > 0` — exactly the
precondition `Mat4::inverseRigid()`'s `[Rᵀ | −Rᵀt]` assumes. A reflection
(`det = −1`) inverts correctly as a matrix but flips inside for outside, so it
is rejected too; `mirrored()` is the operation for that.

### Three judgement calls, recorded because each could have gone the other way

- **`radius == 0` stays legal.** It is the distance-to-centre control field in
  four documented graded-lattice recipes (`sphere(radius=0)` in
  [cmd-ref 11](../../command_reference/11-dualc_field/README.md)). Rejecting
  "degenerate" radii would have broken every one of them. Only a *negative*
  radius throws. Checking the recipes before writing the guard is what caught
  this.
- **No upper cap on `maxDepth`.** The audit wanted `maxDepth = 20` to fail fast
  instead of exhausting memory. But a deep `maxDepth` is legitimate under the
  tiled export path, and any fixed ceiling here is arbitrary policy in the
  wrong layer — the sampler cannot know what the caller's RAM budget is, which
  is precisely what `--mem` exists for. Only the *incoherent* cases (negative,
  or `minDepth > maxDepth`) throw.
- **A period component of `0` stays legal.** It is the documented way to tile
  along one or two axes only. Only a negative component throws.

### The CLI had to learn to report them

`dualc_field` caught only `GraphError`, so a throw from a factory during graph
*build* would have escaped and terminated the process with a bare `what()`.
`buildField` now wraps `std::invalid_argument` in a `GraphError` carrying the
offending node's locator — and only that type, so a child's already-located
error passes through un-rewrapped (`GraphError` derives from
`std::runtime_error`, which makes the distinction free).

Verified end to end:

```
$ dualc_field --expr "union(sphere(radius=1),gyroid(wavelength=0,thickness=0.1))" ...
[dualc_field] field-graph error at 23: GyroidField: wavelength must be > 0 (got 0.000000)
$ echo $?
2
```

Offset 23 is the `gyroid`, not the enclosing `union` — the locator points at
the node that is actually wrong.

## The `BBox{}` sentinel was reachable, not latent

**DONE (2026-09-10).** A default-constructed `BBox` is the degenerate box at
the origin: `min == max == (0,0,0)`, every component finite, `max >= min` — so
`isValid()` returns **true**. Any emptiness guard written as `!b.isValid()`
therefore never fires for it.

The audit filed this as a latent trap. It was not: `RevolveField::bounds` and
`ExtrudeField::bounds` returned exactly `BBox{}` as their invalid-profile
sentinel (`src/implicit/lift.cpp:50,76`), so **the sentinel was unrecognisable
by the very predicate meant to catch it**. Two consequences, both silent:
`bboxUnion` unioned the degenerate box and dragged the origin into the result,
and — because `isValid()` was true — the sampler would not have raised
[#26](03-correctness-and-robustness/03-diagnostics-item.md#26-a-diagnostics-channel-for-the-silent-failure-surface)'s
`boundsFallback` either. A brand-new diagnostic would have stayed quiet for
exactly the case it was built to catch.

Fixed by adding `BBox::empty()` — an inverted box (`min = +inf`, `max = −inf`)
which `isValid()` rejects and which `isInfinite()` does not claim — and
pointing both lift sites at it.

**`isValid()`'s own semantics were deliberately left alone.** Tightening it to
require a positive extent would reclassify every legitimately degenerate box,
such as the flat result of `bboxIntersection` on two touching boxes. The trap
is instead pinned by a test that asserts `BBox{}.isValid()` is *still true*, so
the reason `BBox::empty()` exists cannot be quietly forgotten and someone
"simplifying" the sentinel away would fail the suite.

## Found by the guards: `scaled(f, s)` with negative `s` returned the complement

**DONE (2026-09-10).** Not an audit finding — it surfaced because the first
version of the `scaled` guard rejected `s <= 0`, and an existing test
(`wrapScaleNeg`, `scaled(f, -1.7)`) failed. Investigating why that test existed
showed the operation was doubly broken:

- `valueAt` was `s * child(p/s)`. A negative `s` **negates the child's value**,
  swapping inside for outside — the field returned was the complement of the
  solid asked for.
- `bounds()` mapped `min` above `max` on every axis, so it reported an
  **invalid** box, which would have sent the sampler to its `BBox::unit()`
  fallback.

Neither was caught because the only test exercising negative scale checks
`cellOverlaps` conservativeness, which is blind to the value's sign — another
instance of a test that certifies less than its presence suggests
([#32](04-engineering-quality.md#32-test-coverage-ledger)).

**Fixed rather than rejected.** A negative factor is a scale composed with a
point reflection through the origin, which is distance-preserving and a
perfectly reasonable operation to offer. `valueAt` now multiplies by `|s|`;
`gradientAt` flips the direction when `s < 0` (the gradient of `|s|·child(p/s)`
is `sign(s)·grad child(p/s)`, and the child returns a unit direction);
`bounds()` re-orders per component. Only `s == 0` — the audit's actual claim, a
division by zero — throws.

Pinned by asserting that `scaled(unitSphere, −2)` agrees with
`scaled(unitSphere, +2)` at every probe (the unit sphere is symmetric under
point reflection, so they must be identical), that the resulting bounds are
valid, and that an *off-centre* child really is reflected through the origin
rather than merely scaled.

## Verification

`ctest` **253/253**, up from 246 — seven new `[validation]` cases. No
DualC-origin compiler warnings. The CLI behaviour above was checked by hand,
including the exit code.

## The earlier batch: `FieldPtr` const-ness

**DONE (2026-09-01).** #34's first landed bullet — ~~`FieldPtr` is `shared_ptr<ImplicitField>`,
not `shared_ptr<const ImplicitField>`~~ — predates this page. `FieldPtr` is now `shared_ptr<const ImplicitField>`, so the
"immutable expression tree" the adjacent comment describes is enforced by the type system
rather than asserted. The audit's claim that this "compiles unchanged" did **not** hold:
the whole library and C ABI built untouched, but four host-side
`dynamic_cast<dualc::GridField*>` sites failed (`examples/field_glsl.cpp` ×2,
`examples/dualc_raymarch.cpp`, `tests/test_field_glsl.cpp`). Every one was a pure read
whose accessors (`values()`, `resolution()`, `bounds()`) are already `const`, and
`emitBakedGrid` already took a `const GridField*`, so adding `const` to the cast target
was the entire fix. Verified by a full build and `ctest` **231/231**.
*(Moved here verbatim from #34's entry in [04](04-engineering-quality.md#34-api-hygiene-batch)
on 2026-09-18, when that page was brought under the size cap.)*

## Still open in #34

The three documentation-only bullets (the last of them a small feature rather than
hygiene) stay on the ledger entry, [04 #34](04-engineering-quality.md#34-api-hygiene-batch),
the one place their status is kept. *(2026-09-18: the list restated here became this link.)*

---

← Back to the [topic README](README.md) · the [Roadmap index](../README.md).
