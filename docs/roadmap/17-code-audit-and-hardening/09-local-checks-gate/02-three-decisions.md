# The local checks gate — three decisions worth recording

Part of [the local checks gate](README.md) (#33), the record of `scripts/check.py`; the page became a folder on 2026-09-21 and the sections are verbatim.

## Three decisions worth recording

**The size contract is a ratchet, not an allowlist.** Eight files are declared
frozen in [the roadmap index](../../README.md) and
[the command-reference index](../../../command_reference/README.md), and all of
`docs/study/` is a verbatim snapshot — those are exempt. But five *live* files
were already over a cap, all of them on the **byte** axis only while comfortably
under lines and words (these docs are dense in em-dashes, which cost three bytes
each). `check_data.json` records their exact size; the check fails if a recorded
file **grows** or an unrecorded one **crosses**. Growing one anyway means editing
the number in the same commit — the diff is the point. The recorded number is a
**ceiling, not a ratchet**: the check does not notice a file that shrinks, so lowering
an entry (or deleting it once the file is under the cap) is a manual step at the
session close. The list is the documentation-debt ledger in machine-readable form, and
the intent is that it only ever shrinks.

*(2026-09-17: "eight files are declared frozen" is dated — nothing is frozen since 2026-09-11, [10](../10-docs-system-screening.md); the declaring paragraphs were removed by [19](../../19-docs-layers/README.md) Phase 0. `docs/study/` and `docs/raw/` stay exempt as immutable inputs.)*

*(2026-09-20: the pack is `docs/raw/study/` since [19](../../19-docs-layers/README.md) Phase 7; the one
exempt prefix and its never-edited rule are stated once, in [`docs/raw/README.md`](../../../raw/README.md).)*

Its **scope is `docs/`**, because that is where the contract is declared. Root
files are deliberately outside it — `STRUCTURE.md` is itself ~18 KB and is the
`structure` check's oracle, so it grows by design every time a file is added;
ratcheting the thing that must grow would be self-defeating.

It fired twice on the very commit that introduced it, both times on an **index**:
`04-engineering-quality.md` and `docs/roadmap/README.md` each needed a status
line for this work. Both baselines were raised deliberately, in that commit —
which is the mechanism working, and also its first real finding. Two index files
are structurally over the byte cap, and *every* status update pushes them
further. The contract's own remedy is the extraction pass that is still deferred;
until it happens the ratchet will keep asking, in the diff, every time.

**Bytes are counted from content, never from disk.** `os.path.getsize()` counts
CRLF as two bytes, so re-saving a file with different line endings would trip the
ratchet with zero content change. The count is `len(text.encode("utf-8"))` after
a universal-newline read.

**`ctest`'s total is reported; `parity`'s is asserted.** Deliberate asymmetry.
The test count legitimately grows (231 on 2026-09-01, 253 today *(2026-09-18: 253 was
the 2026-09-10 reading; the live count has one home, [README § Latest verification](../README.md#tracked-items--status-at-a-glance); 231 held
from the 2026-08-31 batch (`ca0fd66`) through #28 and `FieldPtr` (`1492647`, `d48ad98`,
2026-09-01) until #23 (`71b8018`, the same day) took it to 235)*), so hardcoding
it would just be a chore. `dualc_glsl_parity` is the opposite: its mesh/winding
cases resolve `cube.obj` by bare filename and **skip themselves when it is
absent, with the binary still exiting 0**. Trusting that exit code would accept a
69-case run as a pass, so the announced case count is asserted against 73.

*2026-10-07: the serial `ctest` is **kept**, its reason changed. C43 is closed — each Catch
case runs in a working directory of its own and `ctest -j 8` passed three times in a row — so
`-j` is no longer unsafe. It is not worth it: 86 s → 57 s on 8 cores, because the engine already
parallelises inside each case, and oversubscribed cores are what expose the timing-dependent
cancel/progress tests ([14](../14-build-hardening-ci.md) findings 8 and 9). `ctest -j` by hand is
fine — [15](../15-test-coverage-batches.md).*

---

← Back to [09 — the local checks gate](README.md) · the [audit ledger](../README.md) · the [Roadmap index](../../README.md).
