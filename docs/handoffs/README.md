# Handoffs — per-milestone briefings for Philotechnia

This directory contains one self-contained handoff document per milestone of the Philotechnia v1 plan. Each file is a focused briefing an assistant or contributor can open, read end-to-end, and start work from — without needing the full 1,600-line spec in context. If you're picking up a milestone cold, open the matching handoff first.

---

## Authoritative sources

The handoffs are derivative. If any handoff contradicts either of these, the source wins:

- [`docs/philotechnia_spec.md`](../philotechnia_spec.md) — the full specification. Every handoff is a view onto a subset of this document.
- [`docs/decisions.md`](../decisions.md) — the 28 numbered architectural decisions. Each handoff's "Relevant resolved decisions" table pulls the rows that bear on its milestone.

The top-level [`CLAUDE.md`](../../CLAUDE.md) carries the non-negotiable architectural rules (1–9) and the conventions that apply across every milestone; each handoff re-embeds the rules that are live at that milestone, but the repo-root file remains the single source for the full set.

---

## Milestone index

| # | Handoff | Scope (one line) | Spec source |
|---|---|---|---|
| M0a | [M0a.md](M0a.md) | Infrastructure — CMake + Ninja + vcpkg manifest, CI matrix, empty stub binary that builds and packages on macOS / Windows / Linux. | §15.1 (verbatim detail) |
| M0b | [M0b.md](M0b.md) | Storage foundation — atomic writes, hash-prefix directory layout, manifest, project create/open/close lifecycle. | §15.2 (verbatim detail) |
| M1a | [M1a.md](M1a.md) | `SourceControlProvider` interface + `GitProvider` over libgit2 — init, clone, stage, commit, push, pull, branches, stashes, history. | §15.3 (verbatim detail) |
| M1b | [M1b.md](M1b.md) | Conflict handling — `get_conflicts()`, `resolve()`, three-way merge via `git_merge_trees`; rule-6 exhaustiveness constituent (1) (`RecoveryFlow` coverage CI check). | §15.4 (verbatim detail) |
| M2a | [M2a.md](M2a.md) | Schema, records, commands, commit pipeline — `RecordManager`, `RecordWorkingCopy`, `CommandStack`, `SchemaValidator`, atomic batch commit. | §15.5 (verbatim detail) |
| M2b | [M2b.md](M2b.md) | Query, search, index, parallel project open, open cache — performance-critical M2 half. | §15.6 (verbatim detail) |
| M3 | [M3.md](M3.md) | UI alpha — `QTableView` record grid, detail view, schema builder, `QListView` history bound to `CommandStack::history()`, UI-thread bridge goes live. | §15 milestone table row (synthesis) |
| M4 | [M4.md](M4.md) | Source control UI — pending changes, pre-commit diff, commit panel with `ValidationIssue` surfacing, push/pull, branches, history, conflict resolver; rule-6 constituents (2) and (3). | §15 milestone table row (synthesis) |
| M5 | [M5.md](M5.md) | Polish & performance — profiling pass, conditional FTS persistence, network-filesystem accommodations, installer polish, licensing instrument finalised, E2E testing. | §15 milestone table row (synthesis) |
| M6 | [M6.md](M6.md) | v1.0 stable release — version bump, release notes, deferred-item audit, signed installers across all three platforms. | §15 milestone table row (synthesis) |

---

## Verbatim vs synthesis

The spec's §15 has a milestones table followed by per-milestone *deliverable detail* subsections (§15.1 through §15.6). Those subsections exist only for M0a through M2b. The later milestones get one-line table rows and no dedicated subsection.

The handoffs track that asymmetry:

- **M0a through M2b** quote their §15.x deliverable detail verbatim in the "Deliverable detail" section, with cross-referenced context prepended. The spec is the source of truth for the concrete task list.
- **M3 through M6** each carry a "Note on synthesis" callout at the top that quotes the milestone's table row verbatim and explicitly flags the per-deliverable breakdown as extrapolation from the spec's feature sections (§6, §7, §8, §11, §12) rather than quoted spec text. When in doubt about one of these milestones, escalate rather than guess.

---

## Standard template

Every handoff follows the same shape:

1. **Title + provenance line** — which §15 section it derives from, and the "if this disagrees with the spec, the spec wins" disclaimer.
2. **Mission** — one paragraph stating what the milestone delivers and what it does not, plus predecessor and successor pointers.
3. **Foundation** — the project paragraph, the CLAUDE.md rules that are *live* at that milestone, the tech stack slice, and (where relevant) the storage layout and `Error` type.
4. **Spec cross-references** — verbatim blocks from the spec sections the milestone depends on, pulled in so the handoff stands alone.
5. **Relevant resolved decisions** — a table pulling the rows from `docs/decisions.md` that bear on this milestone, each with a "Bearing on Mx" column stating why it matters here.
6. **Deliverable detail** — the concrete task list: for M0a–M2b this is the §15.x block verbatim; for M3–M6 it's a synthesised breakdown.
7. **What Mx+1 adds next** — a short preview of the successor so the reader understands what is *deliberately* out of scope now.
8. **Source docs** — an index of the spec sections, decision rows, and CLAUDE.md rules the handoff draws on.

---

## Using the handoffs

**Picking up a milestone.** Open the matching file. Read it end-to-end once. The "Foundation" and "Spec cross-references" sections are meant to be enough to start work without opening the full spec; the "Source docs" list at the end is what you go to when you need to double-check a detail.

**Dependencies between milestones.** Each handoff states its predecessor in the Mission paragraph. Don't start Mx without having finished Mx-1 (or having a concrete, recorded reason for working out of order). Some milestones are tightly paired — M0a↔M0b, M1a↔M1b, M2a↔M2b — and can reasonably be read together.

**When a handoff and the spec disagree.** The spec wins. File an issue noting which section of which handoff drifted, and which spec section it disagreed with; the handoff gets corrected in a dedicated commit.

**Updating a handoff.** When the spec changes, update the affected handoffs in the same commit (or an adjacent commit in the same PR). Don't let handoffs drift — their value comes from being a faithful compressed view of the spec at the same point in time.

---

## What this directory is not

- **Not a substitute for the spec.** The spec remains the place for new architectural decisions, detailed design discussion, and resolved/open questions. The handoffs only re-present what the spec already says.
- **Not a task tracker.** The deliverable lists inside each handoff are *scope*, not a live todo list. Progress tracking lives wherever the project's actual issue tracker lives.
- **Not an onboarding guide.** A first-time contributor should read `README.md` at the repo root and `CLAUDE.md` first. The handoffs are for people who already know what Philotechnia is and need to go deep on one milestone.
