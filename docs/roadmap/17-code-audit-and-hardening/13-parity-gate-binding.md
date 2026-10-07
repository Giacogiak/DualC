# #31 — the parity gate made binding

The delivery record of [04 #31](04-engineering-quality.md#31-make-the-cpugpu-parity-gate-binding):
the CPU/GPU parity harness became a required CI job on 2026-10-06, Phase 2 of the
[hosted-CI plan](../../raw/2026-10-05-hosted-ci-plan.md). The job itself was born allowed to
fail with [20 #51](../20-public-delivery/03-hosted-ci.md#51-hosted-ci--the-gate-as-a-github-actions-job).

## The blocker check

The trigger was *"headless-GL CI becoming available"*. The question was whether Mesa's
llvmpipe can run the harness at all. It exposes OpenGL 4.5 core, and `R32F` is
colour-renderable since GL 3.0, so the harness's FBO needs nothing exotic. The evidence is
two complete runs of the `gpu` job on `ci/gate-workflow`, both under `--strict` (a SKIP is a
failure) and both announcing and passing **73/73**:

| Run | Commit | `gpu` job | Parity notice |
| --- | --- | --- | --- |
| [3](https://github.com/Giacogiak/DualC/actions/runs/37446307563) | `f064ea8` | 2 min 58 s | 73/73 passed (7.5 s) |
| [4](https://github.com/Giacogiak/DualC/actions/runs/37447937660) | `301b60c` (the merged tip) | 3 min 0 s | 73/73 passed (11.0 s) |

Nothing of ours needed fixing: the clean-clone `data/` failure of run 1 was fixed in #51.

## What changed

- **`.github/workflows/gate.yml`**: the `gpu` job has no `continue-on-error`, so a red
  `gpu` job turns the run red. The job is unchanged otherwise: Ubuntu 24.04, Xvfb, Mesa with
  `LIBGL_ALWAYS_SOFTWARE=1`, the harness and `dualc_gen_demo` built with
  `-DDUALC_BUILD_TESTS=OFF`, `xvfb-run -a check.py --gpu --strict`.
- **The count is asserted there.** `check_parity` fails when the harness announces a number
  of cases other than `parity_expected_cases` (73), the failure mode decided in
  [09/02](09-local-checks-gate/02-three-decisions.md). Nothing about it changed; its
  docstring no longer calls the item deferred.
- **No CTest entry (the CTest half, decided).** `check.py --gpu` stays the one runner. A
  `ctest` entry would judge the binary by its exit code, and the binary exits 0 when the mesh
  cases skip for a missing `cube.obj`. That is the exact hole the count assertion closes, and
  a second runner would have to repeat it. The reasoning is in the comment on the target in
  `examples/CMakeLists.txt`.
- **Not done: a `--snapshot` smoke of `dualc_raymarch` and `dualc_field_view`** in the same
  job (the plan's optional step 3). [D-49](../../decisions/01-settled.md) puts every check in
  `scripts/check.py`, never in the workflow, so the smoke would be a new `--gpu`-tier check
  with its own proven-to-fail step. It is out of this item's scope and no item tracks it yet.

## Proven to fail

The GLSL `opXor` in `examples/field_glsl.cpp` was changed from
`max(min(a, b), -max(a, b))` to `max(min(a, b), max(a, b))`. This is the divergence the
audit describes: one formula, edited on one side only.

- **Locally** (`build-gl`, Mesa on the owner's X display): the build and `ctest` stayed
  green, because no unit test reads `opXor`. `check.py --gpu` failed with
  `[xor] FAIL maxErr 8.92e-01 (4096/4096 over tol)`, **72/73**.
- **In CI**: commit `239000a` on `ci/parity-gate`,
  [run](https://github.com/Giacogiak/DualC/actions/runs/37458550242). The `gpu` job failed on the
  `Parity` step after 2 min 43 s, with the error annotations `[xor] FAIL maxErr 8.92e-01
  (4096/4096 over tol)` and `72/73 passed`. `docs` and the three `build` jobs passed, which
  shows that only the parity gate sees this class of edit. The run's conclusion was
  `failure`.
- **Reverted** in `a25a94c`. The next run was
  [green](https://github.com/Giacogiak/DualC/actions/runs/37467164857) on every job, with
  `gpu` at 73/73 (1 min 57 s; the harness took 3.3 s).

## What is left for the owner

Add `gpu` to the required status checks of `main`'s branch protection, next to `docs` and the
three `build` jobs. The workflow can make a job count toward the run's conclusion, but only
the GitHub UI can make a merge wait for it.

*2026-10-07:* the `--snapshot` smoke of § What changed is [D-53](../../decisions/README.md),
DEFERRED. Under D-49 it would be a new `check.py` check with its own red run, which is not
worth building speculatively. Its trigger is a viewer regression the parity harness missed,
or a change to the viewers' window or snapshot code.

---

← Back to the [topic README](README.md) · the [Roadmap index](../README.md).
