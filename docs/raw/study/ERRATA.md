# Errata — the study pack

The pack — `A1`–`A5`, `B1`–`B5`, `T`, the explorer and `figures/` — is immutable: files
under `docs/raw/` are never edited (the rule is stated once, in [`docs/raw/README.md`](../README.md)).
It was written on 2026-08-19 against the code and the docs of that day, and the docs tree it
cites has been restructured twice since: on 2026-09-11 the roadmap pages it names became
folders ([roadmap 17/10](../../roadmap/17-code-audit-and-hardening/10-docs-system-screening.md)),
and on 2026-09-17 `docs/ARCHITECTURE.md` was split into
[`docs/design/`](../../design/README.md) ([roadmap 19/02](../../roadmap/19-docs-layers/02-record-phase-3.md)).
This file is the pack's one writable companion: every reference the pack makes that no
longer resolves, with what it says and where that fact lives now, and the two status notes
the pack's README used to carry. Line numbers are exact — the A/B/T files are byte-identical
to `f03ef3d` (2026-08-21), the commit that landed them, and the 88-column reflow of `3d73220`
excluded the pack (`git diff 3d73220~1 3d73220 --stat -- docs/study` is empty). The paths in
the left-hand columns are quoted as the pack wrote them — they are the defect being recorded,
not typos; the gate's `stale-paths` check skips `docs/raw/` for exactly this reason.

## Folder paths — pages that became folders

Nine sites (S3 § 7 counted twelve; the other three were in the quality report and the root
`README.md`, fixed by roadmap 19 Phases 5 and 1) plus one S3 did not list (`A3:216`).

| File:line | What it says | Where it is now |
| --- | --- | --- |
| `A1:343` | "the authoritative record is `docs/roadmap/01-core-dual-contouring.md`" | [01-core-dual-contouring/](../../roadmap/01-core-dual-contouring/README.md); the pipeline as it stands is [01/01 § 3](../../roadmap/01-core-dual-contouring/01-engineering-record.md#3-pipeline-as-it-stands-today) |
| `A1:416` | same path, same claim | same |
| `A3:214` | `…01-core-dual-contouring.md` § 4.3 — the hard clamp pinned flat-region vertices to cell walls | [01/02 § 4.3](../../roadmap/01-core-dual-contouring/02-bug-catalogue.md#43-output-was-noisy--smooth-tangency-broken) |
| `A3:216` | `docs/report-quality-inspection-contouring.md` Step 4 — `--no-clamp` worsened rim roughness 2.72 % → 2.94 % | [12/08/01 § Step 4](../../roadmap/12-field-graph-and-app/08-quality-inspection-gyroid-shell/01-measurements.md#step-4--quantify-the-rim-contribution-which-knob-if-any) |
| `A4:45` | `…01-core-dual-contouring.md` § 3.2 / § 3.3 "is current" — simplify and the contourer | [01/01 § 3.2](../../roadmap/01-core-dual-contouring/01-engineering-record.md#32-optional-simplify-srccontourercpp-simplifyhermiteoctree) and [§ 3.3](../../roadmap/01-core-dual-contouring/01-engineering-record.md#33-contourer-srccontourercpp); the design statement is [design/03](../../design/03-contourer-recursion.md) |
| `A4:132` | `…01-core-dual-contouring.md` § 4.4 — the coarsest-cell collapse bug on `molde` | [01/02 § 4.4](../../roadmap/01-core-dual-contouring/02-bug-catalogue.md#44-first-adaptive-collapse-attempt-produced-4-7k-boundary-edges--13-14k-non-manifold-edges-on-molde) |
| `A4:248` | `…01-core-dual-contouring.md` "sizes the exact decider as a ≤ 80-LOC follow-up" | [01/README Tier 2 item 4](../../roadmap/01-core-dual-contouring/README.md#4-manifold-dual-contouring-multi-vertex-per-cell), last sentence |
| `A4:400` | `…01-core-dual-contouring.md` § 4.4 — the same `molde` numbers | [01/02 § 4.4](../../roadmap/01-core-dual-contouring/02-bug-catalogue.md#44-first-adaptive-collapse-attempt-produced-4-7k-boundary-edges--13-14k-non-manifold-edges-on-molde) |
| `B5:144` | `docs/roadmap/01-core-dual-contouring.md:16` — "re-derived from cube combinatorics rather than copy" | [01/01 § 2](../../roadmap/01-core-dual-contouring/01-engineering-record.md#2-architectural-decisions-locked-in-early-still-hold), the "Permissive-only vendoring" row (line 16 of that file) |
| `README:11` (moved to § Status notes) | `docs/roadmap/01-core-dual-contouring.md` § 4.9 — what the five fixes changed | [01/02 § 4.9](../../roadmap/01-core-dual-contouring/02-bug-catalogue.md#49-code-screening-batch) |

`B5:84`'s `docs/roadmap/10-infrastructure-and-integration.md` still resolves.

## Line references into the pre-rewrite `ARCHITECTURE.md`

The pack cites `docs/ARCHITECTURE.md` by line as it stood at commit `d6b2808` (2026-07-09,
the last commit before the pack landed in `f03ef3d`). Every ref resolves exactly against
`git show d6b2808:docs/ARCHITECTURE.md`; the file itself was replaced by the pack's corrected
version on 2026-08-21 (`40bfb9b`) and split into `docs/design/` on 2026-09-17, so the paths
are dangling twice over. The claim each line made, and the current statement:

| File:line | Cited | What `d6b2808` said there | Current statement |
| --- | --- | --- | --- |
| `A2:142` | `:274` | `edgeHit` "default: bisection" | [design/08 § 10.1](../../design/08-implicit-field-layer.md#101-implicitfield): Illinois-modified regula falsi, 6-step budget |
| `A2:219` | `:216`, `:248` | `PSEUDONORMAL` is a no-op routed through parity | [design/02 § `PSEUDONORMAL`](../../design/02-sign-oracles.md#pseudonormal--brentzenaans-angle-weighted-pseudonormal) — implemented, as is GWN |
| `B2:24` | `:263-281` | the "3 + 3" `ImplicitField` sketch | [design/08 § 10.1](../../design/08-implicit-field-layer.md#101-implicitfield) — the interface with `closestSurfacePoint` |
| `B5:144`, `B5:232`, `B5:258` | `:236` | the LGPL octree code was deliberately not vendored | [design/06 § 8](../../design/06-parameters-and-vendoring.md#8-vendored-third-party-code) and [`THIRD_PARTY.md`](../../../THIRD_PARTY.md) |

The pack's *argument* that `ARCHITECTURE.md` was stale (`A1:322–344`, `A1:405–416`, `A1:478`,
`A4:3`, `A4:41–45`, `B1:172–174`, `B3:84`, `B3:318`, `README:13–16`, `README:46–71`; 31 mentions
of the file across eight documents) describes the `d6b2808` file. It was true then, it is
history now: the pack's replacement was adopted, and the compiled present is
[`docs/design/`](../../design/README.md) — there is no `docs/ARCHITECTURE.md` any more.

## Counts that moved on

| File:line | What it says | Now |
| --- | --- | --- |
| `README:20` (moved to § Status notes) | "the 13 worth work are tracked items #22–#34" | 14, #22–#35 — correct when written; #35 was born on 2026-09-10 from #34's fix, with no ledger finding behind it ([17/README](../../roadmap/17-code-audit-and-hardening/README.md#tracked-items--status-at-a-glance)) |
| `README:12–14` (moved), `T` throughout | "all 182 cases", "201 Catch2 cases (224 CTest entries)" | the one home of the test and parity counts is [17/README § Latest verification](../../roadmap/17-code-audit-and-hardening/README.md#tracked-items--status-at-a-glance); no number is restated here |

## Status notes

The two notes below headed the pack's `README.md` (its lines 3–29) from 2026-08-21 and
2026-08-31 until 2026-09-20, when they moved here verbatim (the one link keeps the level it
gained in the move) so that the README stays a plain index. The paths and counts they name
are the ones the tables of this file correct.

> **Status note added 2026-08-21.** This pack is a point-in-time audit of the
> code as it stood on 2026-08-19. Five of the defects it raises have since been
> fixed and are no longer open — `dualContourMesh` dropping
> `SamplerParams::signMethod`; the vendored `pinv` truncating large eigenvalues;
> `tryCollapse` thresholding the normal-equation residual instead of the QEF
> energy; `simplifyHermiteOctree` ignoring `qefRegularization`; and the domain
> wrappers not forwarding `cellOverlaps`. The two coverage gaps it ranks first
> (adaptive collapse untested, the six descent tables untested) are closed too.
> See `docs/roadmap/01-core-dual-contouring.md` §4.9 for what changed and the
> measured effect. One figure is simply out of date: **T** maps "all 182 cases",
> and the suite is now 201 Catch2 cases (224 CTest entries including the CLI and
> C-ABI smoke tests). Everything else in here still stands; the text is left
> exactly as written so the reasoning stays intact.
>
> **Where the live status is (added 2026-08-31).** Every finding this pack raises — all
> 87 of them — now has a disposition in
> [`docs/roadmap/17-code-audit-and-hardening/`](../../roadmap/17-code-audit-and-hardening/README.md),
> and the 13 worth work are tracked items #22–#34. Read the pack for the argument; read
> the ledger for what is still true.
>
> One more thing, because it changes how you read the pack: this folder used to
> hold its own `ARCHITECTURE.md`, a proposed replacement for the repo's design
> doc. That replacement **was adopted** — it is now `docs/ARCHITECTURE.md`
> itself — so the copy here was a duplicate and has been deleted. Wherever the
> documents below say the repo's architecture doc is stale, they are describing
> the version that this pack replaced. There is one authoritative architecture
> document now, at `docs/ARCHITECTURE.md`.

---

← Back to the [study pack](README.md) · [`docs/raw/`](../README.md).
