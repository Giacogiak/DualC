# The local checks gate — proven to fail, fixed on the way past, what it moves

Part of [the local checks gate](README.md) (#33), the record of `scripts/check.py`; the page became a folder on 2026-09-21 and the sections are verbatim.

## Every check was proven to fail

Nine checks, nine deliberate breakages, each reverted: a link target renamed, an
anchor renamed, a page unlinked from its index, 400 lines appended to a live doc,
a frozen file dropped from its declaring paragraph, a test file removed from
`STRUCTURE.md`, a license path broken, `#99` inserted above `#27`, and an indent
error introduced into a script. Each produced its own check's failure with a
`path:line:` message, and each check returned to green on revert.

**That is not ceremony — it caught a false pass.** The first `frozen-decl` looked
for the *filename* anywhere in the declaring README and passed unconditionally,
because the roadmap index links every topic from its table anyway. It certified
nothing. The roadmap declares its frozen files as bare numbers (`` `13` ``), not
filenames, so the check now locates the declaring **paragraph** by its marker and
looks for a per-entry token inside it. That is the fourth assurance found this
session to certify less than it appears to, after the dead `nFaces() == 0`
warning ([05](../05-diagnostics-channel.md)), the strut-lattice oracle
([07](../07-repeat-tiling-fix.md)) and `wrapScaleNeg`
([08](../08-argument-validation.md)).

*(2026-09-20: Phase 6 of [19](../../19-docs-layers/README.md) gave `flag-table` its second
direction — every `--flag` a recipe passes to `dualc_<tool>` must head a table cell on
that tool's page and be known to its parser — with the tenth fixture pair,
`scripts/check_fixtures/flag-table/`, proving both failure modes; `sizes` now prints the
exempt-file count where it printed `1 frozen`. Still one CTest entry.)*

## Fixed on the way past

- **`THIRD_PARTY.md` cited a license path that does not exist** — it pointed at
  `examples/third_party/meshoptimizer_LICENSE.txt`; the file is one directory
  deeper. Corrected, which is what let the `vendoring` check ship green.
- **`scripts/count_nm.py` did not parse.** Every line after `import sys` was
  indented two spaces — a chat paste committed verbatim in May and never run
  since. Dedented and given a docstring.
- **Ten files were missing from `STRUCTURE.md`**: four test translation units,
  the `07`/`08` ledger records, `capi/CSHARP_WRAPPER_HANDOFF.md`, and the whole
  vendored `meshoptimizer/` directory.
- **`.gitattributes` added.** `core.autocrlf` is `true` here, which would rewrite
  the hook with CRLF on checkout — and Git-for-Windows' `sh` chokes on
  `#!/bin/sh\r`, so the hook would have silently stopped running in any fresh
  clone.

## Enabling the hook

```
git config core.hooksPath scripts/hooks
```

Versioned in-tree, no installer to drift. It runs `check.py --fast` only; bypass
a single commit with `git commit --no-verify`. Because that is **local config
which does not survive a clone**, `--fast` prints a one-line reminder whenever it
is not set. *(2026-10-02: on a Linux machine, which has `python3` and no `python`, neither
the hook nor the `.claude/settings.json` `Stop` command ran. Both now call
`$(command -v python3 || command -v python)`, and the hook carries its exec bit in git, which a
Windows clone with `core.filemode` false had never recorded. Reported by Boletus's
[roadmap 09/08](../../../../../Boletus/docs/roadmap/09-docs-layers/08-gate-portability.md).)*

Two scope notes, stated rather than left implicit: the checks read the **work
tree** while `git ls-files` reads the **index**, so a partial `git add` can commit
content the gate never saw — accepted for a single-developer repo, and the
index-based file list is what makes a staged new file visible to `structure`. And
the gate assumes an **already-bootstrapped checkout**: a fresh clone additionally
needs the Catch2 fetch and `dualc_gen_demo all --dir data`, which remains open
#33 work *(closed 2026-09-21: the build runs the generator itself, [20 #47](../../20-public-delivery.md))*.

## What this does and does not move

- **#33 — advanced, nothing closed.** Its own trigger is now true. The remaining
  scope is unchanged: the dialect-flag leak into third-party subtrees, the
  undeclared transitive dependencies, sanitizer / `-Werror` / `clang-tidy`
  options, the `LINK_LIBRARIES` layering assertion, and the clean-clone
  bootstrap. The `warnings` check is a placeholder that becomes load-bearing the
  day `/WX` lands — worth knowing that `/W4 /permissive-` is
  `target_compile_options(dualc PRIVATE …)`, so it covers the **core library
  only**; tests and examples build at the MSVC default, and
  `examples/CMakeLists.txt:85` claims otherwise in a comment.
- **#32 — hosted, not advanced.** The gate writes none of the missing tests. It
  provides where they run, encodes the no-`-j` constraint that finding describes
  rather than leaving it to memory, and makes two of the ledger's metrics
  readable via `--metrics`. **#32 stays PLANNED.**
- **#31 — still DEFERRED.** `--gpu` answers the *"not scriptable, no count
  assertion"* half. The actual blocker is untouched: a GL context with an
  R32F-renderable FBO that no unattended local runner provides. Neither candidate
  fix — headless llvmpipe/SwiftShader, or a CPU reference evaluator for the
  emitted AST — is delivered.

*2026-09-20 (19 Phase 8): rows split on unescaped `|` only (`table_cells`); `decisions-index` keeps every row per anchor; `status-vocab` reads the decisions tables (`status_vocab_only`) — [19/07](../../19-docs-layers/07-record-phase-8.md).*
*2026-09-21: no sibling probe; whole-line warning filter — [20 #47](../../20-public-delivery.md).*

---

← Back to [09 — the local checks gate](README.md) · the [audit ledger](../README.md) · the [Roadmap index](../../README.md).
