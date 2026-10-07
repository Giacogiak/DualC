# Public delivery — the hosted-CI plan's run

Part of [public delivery](README.md) (block 20). The
[hosted-CI plan](../../raw/2026-10-05-hosted-ci-plan.md) was run by the owner's `plan-run`
driver as five unattended units, 2026-10-06 … 2026-10-07, each a fresh session that closed
with a handoff file under `plan/handoffs/`. Each item's record already holds its own facts
(below); this page is the harvest of what the handoffs held *across* units — how the run
worked, what it taught, and what it left open — after which `plan/` was retired
([D-51](../../decisions/01-settled.md)).

## The hosted-CI plan's run — P01 to P04, harvested

**DONE — 2026-10-07.** All four phases closed; NEXT returns to the engine ledger
([roadmap README](../README.md#next-up--dualcs-own-roadmap)).

| Unit | Item | Branch | Record | Green run the record names |
| --- | --- | --- | --- | --- |
| P01.1 | #51 hosted CI | `ci/gate-workflow` | [20/03](03-hosted-ci.md) | [37446307563](https://github.com/Giacogiak/DualC/actions/runs/37446307563) |
| P02.1 | #31 parity gate binding | `ci/parity-gate` | [17/13](../17-code-audit-and-hardening/13-parity-gate-binding.md) | [37467164857](https://github.com/Giacogiak/DualC/actions/runs/37467164857) |
| P03.1 | #33 build hardening | `ci/build-hardening` | [17/14](../17-code-audit-and-hardening/14-build-hardening-ci.md) | [37518512354](https://github.com/Giacogiak/DualC/actions/runs/37518512354) |
| P04.1 | #32 Batch A | `ci/test-coverage` | [17/15 § A](../17-code-audit-and-hardening/15-test-coverage-batches.md#batch-a--correct-the-record-then-the-mechanical-fixes) | [37584887029](https://github.com/Giacogiak/DualC/actions/runs/37584887029) |
| P04.2 | #32 Batch B | `ci/test-coverage` | [17/15 § B](../17-code-audit-and-hardening/15-test-coverage-batches.md#batch-b--the-missing-tests) | [37591221067](https://github.com/Giacogiak/DualC/actions/runs/37591221067) |

Each unit's final commit (its handoff) triggered one more run that the handoff could not
quote — writing the URL down would take another commit and so another run; the driver's
`remote-run` check verified it instead.

**How the run worked.**

- **The push rule** (the owner's decision, 2026-10-06, carried in the driver's manifest): a
  unit pushes the branch its plan file names, as often as it needs a CI run; it never pushes
  `main` and never force-pushes; the owner merges at the driver's approve gate.
- **The branches are stacked.** Each unit branched from local `main`, which already held the
  previous unit's merge, so every branch carries its predecessors. All four `ci/*` branches
  are merged into local `main` by fast-forward; `ci/test-coverage`'s pushed head `934cb7e` is
  `main` less the snapshot commit `177b875`. **`main` was never pushed:** on 2026-10-07
  `origin/main` is still the initial public release `5989fb5`, 39 commits behind.
- **The `remote-run` check** (`plan/remote_run_check.py`, last at `e1e1f31`) asked the public
  Actions API, with no token, for the newest run of the current branch, waited while it was
  queued or running, and exited 0 only for a run of exactly `HEAD` concluding `success`. It
  polled every 180 s (the owner's decision, `e1e1f31`): the unauthenticated limit is 60
  requests an hour, shared with the session's own reads. `continue-on-error` jobs do not
  affect a run's conclusion, so an allowed-to-fail experiment never turned it red.
- **Proven red in public.** Every check a unit made binding was shown to fail on a pushed
  commit titled `DELIBERATE BREAK`, then reverted: `239000a` (#31), `ddf20fb`, `0ce9103` /
  `0647e85` and `7eb0f52` (#33). History keeps them on purpose; each record names its red run.
- **The plan files were corrected by sessions, the raw plan never:** the FetchContent cache
  paths (P01, 20/03 § Caching) and the #32 record number, 17/15 not 17/13 (P04.1). P04.1's
  advice to split 17/04 into a folder was not followed: it would have broken three links in
  the immutable raw plan, so P04.2 moved #33's history paragraph to 17/12 instead
  ([17/15](../17-code-audit-and-hardening/15-test-coverage-batches.md#04-was-condensed-not-split)).

**What the run taught, for the next one.** Each lesson cost a unit time once:

- *A job's log needs a login; its annotations do not.* The API ran out mid-P01; the public
  run page (`/actions/runs/<id>`) lists the annotations and `/job/<id>` the job's name and
  duration. That is why `annotate.py` re-prints the gate's report as notices, and why a CI
  failure is reproduced locally or named by job and step, never guessed.
- *A green job proves little where SKIP is green.* P01's run 2 was green in 4–5 min with
  nothing readable; `--strict` on the `gpu` job is the fix (20/03).
- *An unreadable red is not a proof.* P03's first layering break went red on every job with
  the CMake error pushed past the ten annotations CI keeps; `b1cdfc5` puts `CMake Error`
  blocks first, and the break was re-run.
- *Keep a break commit alone.* P03 pushed one together with the `sanitize` job's flip to
  required, and an unrelated test defect (17/14 finding 9) surfaced on a run meant only to go
  red.
- *Write a commit message after the evidence.* P02's revert said "the gpu job went red on it"
  before the red run had finished; it held, and was not pushed until it had been read.
- *`check.py --clean` reconfigures, it does not rebuild.* A meaningful warnings scan needs
  `cmake --build build --target clean` first; an incremental build makes the scan SKIP.
- *Measure before pinning.* P04.2 guessed the 256-configuration census and the first run
  failed on it; the pinned census is the measured one (17/15).
- *Two pages sit at the cap.* On 2026-10-07 `docs/decisions/README.md` is at 15,346 and
  `docs/roadmap/README.md` at 15,339 of 15,360 bytes: the next row on either needs a trim or
  a split first, so this run's settled decision went to `01-settled.md`.

**Open after the run.** None is a tracked item yet; each has the home named.

| What | State | Home |
| --- | --- | --- |
| Push `main` to `origin` | the owner's; nothing in the run did it | this page |
| Branch protection on `main`, requiring `docs`, the three `build` jobs, `sanitize` and `gpu` | the owner's, GitHub UI only — only it makes a merge wait for a job | [20/03](03-hosted-ci.md), [17/13](../17-code-audit-and-hardening/13-parity-gate-binding.md#what-is-left-for-the-owner) |
| `dualc_gen_demo all --dir <missing>` aborts (uncaught `std::runtime_error`, exit 134), so `AGENTS.md`'s `--dir data` recipe fails on a fresh clone | worked around twice: the `gpu` job's `mkdir -p data`, and `cli_gen_demo_out/` created at configure | [20/03](03-hosted-ci.md) |
| `-DDUALC_BUILD_C_ABI=ON` does not link on Linux (`R_X86_64_TPOFF32 against __tls_guard`, non-PIC static libs) | predates the run | [17/14](../17-code-audit-and-hardening/14-build-hardening-ci.md#findings-and-what-was-done-with-each) finding 7 |
| The macOS `parallelForPolled` flake, once in 0.01 s | not reproduced in 550 Linux runs | 17/14 finding 8 |
| A `--gpu`-tier `--snapshot` smoke of `dualc_raymarch` and `dualc_field_view` | not done: D-49 makes it a `check.py` check with its own red run | [17/13](../17-code-audit-and-hardening/13-parity-gate-binding.md#what-changed) |
| Why the `RepeatField` fix moves vertices within two cells of the root box, when the values are bit-identical in the interior probe | not established; the widened `cellOverlaps` is the suspect, untested | [17/15](../17-code-audit-and-hardening/15-test-coverage-batches.md#the-strut-lattice-oracle-certified-what-it-claimed), input to [#35](../17-code-audit-and-hardening/04-engineering-quality.md#35-repeatfield-neighbour-set-optimisation) |
| `ubuntu-latest` becomes Ubuntu 26 on 2026-10-19 (GitHub's notice) | only the `docs` job uses the label, and it needs only Python | [20/03](03-hosted-ci.md) |
| A release workflow publishing the C ABI library per OS | out of scope by the plan (§ 6); it would fire [#8](../../decisions/README.md)'s "CI artifact" trigger and give Boletus its re-vendor artifact | [plan § 6](../../raw/2026-10-05-hosted-ci-plan.md#6-after-the-four-phases-and-what-stays-out) |

**`plan/` retired.** The five handoffs are harvested here and in the records above, the plan
files' corrections are recorded, and the raw plan is the immutable input; so `plan/` — the
phase files, `run.manifest.json`, `remote_run_check.py` and `handoffs/` — is deleted from the
tree, recoverable from history at `177b875` ([D-51](../../decisions/01-settled.md)). The
driver's state directory `.plan-run/` stays gitignored for the next run, which writes its own
`plan/`.

*2026-10-07, the loss audit.* The retirement above was committed (`8d7926c`) before
`scripts/docs_loss_audit.py` had been run, and the owner asked. It was run afterwards, with
base `177b875`, head `8d7926c`, scope `docs/` and `plan/`, and `plan/` as the annex. It
used a scratch triage file with an empty audit; none was committed, so
`scripts/docs_loss_audit.json` is unchanged.

- **The annex.** Of `plan/`'s units, 509 of 519 sentences, 182 of 356 evidence units, 49 of
  51 headings and 29 of 590 identifiers came back residual. The sentences were expected: the
  handoffs were paraphrased, not copied.
- **What triage left as lost.** The rest were link-tier pointers, commit hashes (git keeps
  them) and the deleted paths themselves. Lost were the measurements listed in
  [17/15 § Harvested](../17-code-audit-and-hardening/15-test-coverage-batches.md#harvested-from-the-handoffs)
  and the dialect rationale in [17/14](../17-code-audit-and-hardening/14-build-hardening-ci.md).
  Both are now routed there, in the commit after `8d7926c`.
- **The `docs/` side.** Its one real residual is the trimmed Current-focus sentence on the
  roadmap README. The cut words survive in that page's milestones table. The audit's other
  `docs/` rows are the short-line noise of unchanged pages, which the 3d73220 audit absorbed
  with bulk rules.
- **The dump is not kept.** The run's 18,069-line report is not in `docs/raw/`: nearly all
  of it is that noise, and the counts above are what it established.

*2026-10-07, the open items given homes.* At the owner's request, each row of *Open after the
run* now has an ID or is done:

- **`main` is pushed:** `origin/main` = `7fd9066`.
- **Branch protection** is the owner's, in the GitHub UI.
- **`dualc_gen_demo --dir`:** DONE as [10 #54](../10-infrastructure-and-integration.md). The
  `gpu` job's `mkdir -p data` is gone.
- **The Linux C ABI link:** tracked as [#53](../14-c-abi/06-linux-shared-library.md),
  PLANNED. Boletus already works around it.
- **The macOS flake:** [D-52](../../decisions/README.md), DEFERRED.
- **The `--snapshot` smoke:** [D-53](../../decisions/README.md), DEFERRED.
- **The `RepeatField` vertex movement:** an open line on
  [#35](../17-code-audit-and-hardening/04-engineering-quality.md#35-repeatfield-neighbour-set-optimisation).
- **`ubuntu-latest`:** the `docs` job is pinned to `ubuntu-24.04`.
- **The release workflow:** still out of scope, waiting on #8's trigger.

17/04 is now at 15,355 of 15,360 bytes, so its next entry needs the split first.

---

← Back to [20 — public delivery](README.md) · the [Roadmap index](../README.md).
