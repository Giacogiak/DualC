# The local checks gate — CI for a repo with no remote (#33)

DualC has **no hosted CI**, so nothing but the owner triggers a check. But "there is no CI" is the stated blocker on three
tracked items — [**#33**](../04-engineering-quality.md#33-build--tooling-hardening)
had *"CI being set up"* as its literal trigger (reworded when this gate landed),
[**#31**](../04-engineering-quality.md#31-make-the-cpugpu-parity-gate-binding) wants
a headless-GL job, and [**#32**](../04-engineering-quality.md#32-test-coverage-ledger)
records a temp filename *"two concurrent CI jobs would collide on"*. That premise
deserved separating from the hosting question.

## What CI actually is, split in two

- **(a) One reproducible gate** — a single command running every check, printing
  pass/fail. Needs no server and no network. It carries nearly all the
  value and it did not exist.
- **(b) Unattended triggering by something that is not you.** Genuinely absent
  locally. A git hook is an **honest substitute for the fast checks, not an
  equivalent** — so the build tier has no automatic trigger at all. It stays *"run the one command"*.

Self-hosted Jenkins, a local bare mirror with a `post-receive` hook and a Task
Scheduler job were all considered and rejected: real options, all ceremony for a
one-developer local repo.

**DONE (2026-09-10):** `scripts/check.py`, `scripts/check_data.json`,
`scripts/hooks/pre-commit`.

The page became this folder on 2026-09-21, when it sat 8 bytes under the cap and owed the
`semantic-lint-runs` row ([19/07](../../19-docs-layers/07-record-phase-8.md)); the children
keep every section verbatim, their dated notes with them, and every heading anchor. The
live counters are what `check.py --fast` prints; the check tables are the dated reading
their notes say.

| Section | Child |
| --- | --- |
| The docs checks lead, and that is the point — the nine structural checks (2026-09-11 reading), the build and `--gpu` tiers | [01](01-the-docs-checks.md#the-docs-checks-lead-and-that-is-the-point) |
| The semantic half — the seven checks of the screening, the two corrections, the ten Phase 2 checks, the 27th | [01 § The semantic half](01-the-docs-checks.md#the-semantic-half) |
| Three decisions worth recording — the size ratchet, bytes from content, `ctest` reported / parity asserted | [02](02-three-decisions.md#three-decisions-worth-recording) |
| Every check was proven to fail — nine breakages and the false pass they caught | [03](03-proven-to-fail-and-scope.md#every-check-was-proven-to-fail) |
| Fixed on the way past · Enabling the hook · What this does and does not move (#31, #32, #33) | [03 § Fixed](03-proven-to-fail-and-scope.md#fixed-on-the-way-past) · [§ Hook](03-proven-to-fail-and-scope.md#enabling-the-hook) · [§ Moves](03-proven-to-fail-and-scope.md#what-this-does-and-does-not-move) |

---

← Back to the [audit ledger](../README.md) · the [Roadmap index](../../README.md).
