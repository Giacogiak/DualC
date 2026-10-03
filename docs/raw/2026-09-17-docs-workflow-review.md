# Chat advice on the DualC documentation workflow

Imported 2026-09-17.

Source: a claude.ai chat in the DualCDoc project, pasted verbatim by Giacomo on 2026-09-17.
This is the **input** the restructuring plan (`restructuring-plan.md`) maps to actions. Kept
immutable; the plan cites its points as A1–A10.

---

I can't reach D:\DualC from here, so I assessed the workflow through the `repo-docs-lifecycle` skill — which is derived from DualC's conventions, so it's a faithful proxy. Here's where it stands, what the field converged on, and my answer on RAG.

## Your workflow: what's strong

You've independently built most of what the field now considers best practice, and several parts are better than average:

* Repo as single source of truth, append-only record, evidence with every DONE, rejected approaches recorded. This is exactly what ADRs and development journals are for, and most teams never get there.
* IDs for life, frozen headings, "link don't duplicate", one home per fact class. These are the rules that stop a doc corpus from rotting. The anchor-stability rule in particular is one most people learn the hard way.
* `DRIFT-PENDING` and `Reconstructed from commit` are the best ideas in the skill. Making documentation debt grep-able and forbidding fabrication is precisely the failure mode agents have.
* Size invariant + legacy freeze. Pragmatic; containment over heroic rewrites is right.

What you've built is essentially Karpathy's "LLM wiki" pattern before he published it. That pattern has the LLM act as a full-time research librarian, compiling, linting, and interlinking markdown files, which he calls the most LLM-friendly and compact format — and the community reaction was that the industry may have over-indexed on vector DBs for problems that are fundamentally about structure, not just similarity.

## Where it's weaker

1. The roadmap conflates "how it is" with "how it got here." Topic files are chronological journals, but an agent starting a task mostly needs the current design: which invariants hold, which data structures exist, what the algorithm does today. Right now it must reconstruct the present by reading a history. Karpathy's split is instructive: `raw/` (immutable) vs `wiki/` (compiled, freely rewritten). You have the raw layer; you're missing a compiled, mutable current-state layer beyond a single STRUCTURE.md. The roadmap README's "Current focus" is a partial substitute, but it's a status snapshot, not a design description.
2. Enforcement lives in prose, not tooling. The session-end workflow is five steps of checklist an agent must remember. Frontier LLMs handle roughly 150–200 instructions consistently, and your skill plus references is already a large instruction budget. Everything mechanical there (index completeness, link validity, file sizes, DRIFT-PENDING sweep, status-column-matches-file, newest doc date vs HEAD) should be a `tools/docs_lint.py` run by a Claude Code hook or pre-commit, so the agent runs one command and fixes a report. The current trend is explicitly "hook-enforced," not "prompt-enforced."
3. No root entry file. The skill is triggered by phrases; nothing guarantees an agent that just opens the repo lands on the docs. Vercel's evals found static AGENTS.md context reached 100% pass rate versus 79% with dynamic skill retrieval; agents fail to fetch context they need 56% of the time. The fix is a short `CLAUDE.md` (with `@AGENTS.md` for other tools) holding build/test commands, the five non-negotiable principles, and the reading order — and nothing else. The 2026 consensus is "as little as possible, but as alive as possible," since every line consumes attention budget.
4. Decisions are buried in chronology. A DROPPED approach in `05-tpms-lattices.md` is invisible from anywhere else. A decisions index (one table: ID, decision, date, topic-file link) costs almost nothing and is the thing a future agent should scan before proposing something already rejected.
5. No place for cross-cutting knowhow. Numerical tolerances, mesh-validity invariants, coordinate conventions, "why we never do X" — these belong neither in a tool page nor in a topic journal. They need a `docs/knowhow/` or `docs/design/` home.

## Vector DB + RAG skill: overkill, yes

Concretely, for this project:

* Your corpus is probably 50–150 files and a few hundred thousand words at most. Karpathy's own setup is around 100 articles and roughly 400,000 words, and at that scale navigating via summaries and index files is sufficient without vector infrastructure. Index + grep already outperforms semantic search for the queries agents actually make in a repo like this: exact flag names, item IDs, function names, error messages.
* Chunked retrieval discards exactly what you invested in: numbering, cross-links, chronology, the "link don't duplicate" discipline. Chunks don't know about each other; a decision in March and a constraint in January don't connect unless both surface in the same retrieval pass.
* An embedding index is one more artifact that drifts from the docs, needs re-ingestion, and hides its staleness.

Where it would pay off: searching across raw material that isn't curated — benchmark logs, experiment dumps, papers, long chat exports. If that pile grows, the middle ground is not a RAG skill but a local hybrid search CLI: Tobi Lütke's QMD does BM25 plus vector search with LLM re-ranking over markdown, is available as a CLI and MCP server, and Karpathy recommends it as the search layer for LLM wikis. Zero pipeline to maintain, and it sits beside grep rather than replacing your structure. Even simpler: ripgrep plus SQLite FTS5 over `docs/` covers 95% of it.

## A layered target structure

```
CLAUDE.md / AGENTS.md      ≤ ~80 lines: build, test, principles, reading order
STRUCTURE.md               codebase map (mutable)
docs/
  design/                  CURRENT STATE — architecture, invariants, algorithms,
                           conventions, glossary. Rewritten freely. Compiled layer.
  decisions/README.md      one-table index of every #ID decision → topic-file anchor
  roadmap/                 as now — append-only RECORD (why/how it got here)
  command_reference/       as now — usage contract
  raw/                     benchmarks, experiment logs, imported notes. Immutable.
tools/docs_lint.py         indexes, links, sizes, DRIFT-PENDING, status sync,
                           last-documented-commit vs HEAD → run by hook
```

The migration is small: the design layer starts as extracted "current state" paragraphs from topic-file intros; the decisions index is a grep for DROPPED/DEFERRED; the lint script is a couple hundred lines Claude Code can write from your integrity-audit checklist. Then the skill shrinks to: run lint, harvest the session, route facts to their layer, run lint again.

One thing worth stealing from Karpathy beyond the folder split: a periodic lint pass in the semantic sense — an agent reads `design/` against the code and flags contradictions, orphan pages, and stale claims. Your DRIFT-PENDING catches undocumented commits; this catches documented-but-wrong, which is the more dangerous failure for a compiled layer.

If you want, next step I can draft the `CLAUDE.md`, the `docs_lint.py` spec, and a revised skill body against this structure.
