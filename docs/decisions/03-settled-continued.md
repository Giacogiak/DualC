# Settled decisions, continued — from D-67 on

Part of the [decisions index](README.md). The settled table of
[01-settled.md](01-settled.md), continued: same columns, same rule (a row here never changes
status; a reversed decision gets a new row on the README), the same ID sequence. Opened
2026-10-21 because 01 had reached the byte cap (D-75); its rows stay where they are.

| ID | Decision | Status | Date | Where recorded |
| --- | --- | --- | --- | --- |
| D-67 | The porous node's cells are both, `cells=voronoi\|delaunay`, `voronoi` the default; one Delaunay tetrahedralization gives both | DONE | 2026-10-19 | [23/02 § The owner's decisions](../roadmap/23-porous-shell/02-record.md#the-owners-decisions) · [raw plan § 1](../raw/2026-10-19-porous-shell-plan.md#1-the-owners-decisions) |
| D-68 | Two porosities, `mode=walls` (sealed cells) and `mode=struts` (open-cell foam); perforated walls out of scope | DONE | 2026-10-19 | [23/02 § The owner's decisions](../roadmap/23-porous-shell/02-record.md#the-owners-decisions) |
| D-69 | The thickness reuses `onion` / `graded-onion` / `graded-offset` unchanged; the node is an unsigned-distance source, as a TPMS is | DONE | 2026-10-19 | [23/02 § The owner's decisions](../roadmap/23-porous-shell/02-record.md#the-owners-decisions) |
| D-70 | Corner smoothing is implicit, in the node: a polynomial smooth-min of radius `smooth=k`, `k = 0` sharp | DONE | 2026-10-19 | [23/02 § The owner's decisions](../roadmap/23-porous-shell/02-record.md#the-owners-decisions) |
| D-71 | Several extensible distributions: `poisson` (default), `jitter`, `random`, `relax=N` Lloyd, a graded density over a control with a `linear\|smooth\|gaussian` ramp | DONE | 2026-10-19 | [23/02 § The owner's decisions](../roadmap/23-porous-shell/02-record.md#the-owners-decisions) |
| D-72 | Boletus gets a plan document only, on its own branch; nothing implemented there | DONE | 2026-10-19 | [23/02 § The owner's decisions](../roadmap/23-porous-shell/02-record.md#the-owners-decisions) |
| D-73 | The porous run is unattended: every gate `auto`, no stop on a question (in-run choices marked for review), the heavy checks at the end (P08) | DONE | 2026-10-19 | [23/02 § The owner's decisions](../roadmap/23-porous-shell/02-record.md#the-owners-decisions) |
| D-74 | The Delaunay engine is our own sequential Bowyer–Watson on Shewchuk's public-domain predicates; geogram's PSM is rejected for its unlocked process-wide RNG and its call-to-call tet order | DONE — **Chosen in-run (pending owner review)** at P01 | 2026-10-21 | [23/02 § The engine choice](../roadmap/23-porous-shell/02-record.md#the-engine-choice) · [23/01 § (a)](../roadmap/23-porous-shell/01-study.md#a-the-delaunay-engine) |
| D-75 | Settled rows from D-67 on go to this child page, 01-settled.md being at the byte cap; the others: split 01 into a folder (about 40 inbound links), or tighten its wording | DONE — **Chosen in-run (pending owner review)** at P01 | 2026-10-21 | [23/02 § The engine choice](../roadmap/23-porous-shell/02-record.md#the-engine-choice) |

---

← Back to the [decisions index](README.md) · the [docs index](../README.md).
