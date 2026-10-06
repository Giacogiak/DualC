# Public delivery — the repo as others will build it

The record of making DualC buildable by someone who is not this machine: how the one
required dependency, geometry-central, is obtained and pinned, what the library owns
outright, and what a bare clone must do with no manual step. Born 2026-09-21 from the
question "how should geometry-central be delivered with DualC?", whose answer turned out to
need a port first. Headings are frozen at ID + title; status, date and trigger live on the
body lines. The present-tense rules live in [`docs/design/05` § 5](../../design/05-conventions-and-tables.md#5-geometry-central-integration)
and [`THIRD_PARTY.md`](../../../THIRD_PARTY.md); the settled decision is
[D-42](../../decisions/01-settled.md).

## The pages of this block

The block became this folder on 2026-10-06, when it sat 131 bytes under the cap and owed the
#51 entry ([plan § 1](../../raw/2026-10-05-hosted-ci-plan.md#1-repo-facts-the-plan-relies-on));
each child holds one item, its section verbatim, the `#47-…` and `#49-…` heading anchors kept.

| Item | Page | Status |
| --- | --- | --- |
| #47 Pin geometry-central to upstream, own nanort, self-bootstrapping clone — and the 2026-10-03 publication update | [01](01-pin-geometry-central.md#47-pin-geometry-central-to-upstream-own-nanort-self-bootstrapping-clone) | DONE 2026-09-21 |
| #49 Linux as a build host | [02](02-linux-build-host.md#49-linux-as-a-build-host) | DONE 2026-10-03 |
| #51 Hosted CI — the gate as a GitHub Actions job | [03](03-hosted-ci.md#51-hosted-ci--the-gate-as-a-github-actions-job) | DONE 2026-10-06 |

---

← Back to the [Roadmap index](../README.md).
