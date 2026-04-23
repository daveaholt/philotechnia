# Handoff — M1b: Conflict Handling & Rule-6 Exhaustiveness Check

*Derived from `docs/philotechnia_spec.md` §15.4. If this document and the spec disagree, the spec wins.*

---

## Mission

Wire conflict interception and the full `WorkflowMetadata` surface for `GitProvider`, and land the first of the three CI-enforced rule-6 exhaustiveness checks (decision #27, constituent 1). A throwaway prototype precedes the milestone; production work derives from its findings. After M1b, a divergent `pull()` produces conflicts surfaced as three-blob `Conflict` entries without ever writing `<<<<<<<` / `=======` / `>>>>>>>` markers to the working copy, and adding a new `ErrorState` value to `src/core/workflow.hpp` without matching provider coverage fails CI.

**Predecessor:** M1a — `SourceControlProvider` interface, `GitProvider` (FF-only pull, stubbed conflict methods), `Error` type, `TestProvider`. **Successor:** M2a — schema engine, records, commands, commit pipeline, `SchemaValidator`.

---

## Foundation

### Project in one paragraph

Philotechnia is a cross-platform C++ desktop application for structured record management. Projects are version-controlled directories of plain JSON files (one file per record, hash-prefix directories). The application includes a first-party source control UI built on an abstract `SourceControlProvider` interface — Git via libgit2 ships in v1; SVN and Perforce are later milestones. The UI is Qt. JSON is yyjson. All undo/redo and dirty-state tracking flows through a custom `CommandStack`, not Qt's built-in undo framework.

### Non-negotiable architectural rules (from CLAUDE.md)

Rules active in M1b:

1. **The UI layer must never depend on a concrete `SourceControlProvider` implementation.** The rule-6 exhaustiveness check you add this milestone lives in `tests/core/`, not in any UI code, and consumes providers only through the abstract interface.
4. **All record writes use atomic rename.** The conflict-resolution write path (`resolve()` with a present `resolved_content`) writes via `atomic_write` from M0b — never via `git_checkout_tree` / `git_checkout_index` / `git_checkout_head`.
6. **The application must be fully self-sufficient for all source control states it creates.** This milestone is the first enforcement of rule 6 as CI-checked code: the coverage unit test lands here. Every git-applicable `ErrorState` must be covered by `GitProvider::get_workflow_metadata().recovery_flows` or listed in `kUnshippedErrorStates`.
9. **Commands mutate only in-memory state; disk I/O happens exclusively in the commit pipeline.** The merge-commit produced after conflict resolution goes through the normal commit pipeline — provider does not produce the merge commit itself.

Rules 2, 3, 5, 7, 8 are not active (either already enforced or forward-looking).

### Tech stack slice

| Layer | Technology | Where it lands |
|-------|-----------|----------------|
| Source control | libgit2 `git_merge_trees` + manual index | `src/vcs/git/merge.{hpp,cpp}` |
| Storage | `atomic_write` from M0b | called from `resolve()` |
| Test support | `TestProvider` from M1a (seeding helpers) | `tests/vcs/git/merge/` |
| Enum iteration | `magic_enum` (header-only; test-only vcpkg dep) or X-macro fallback | `tests/core/workflow_rule_6_coverage_test.cpp` |

### Error type (§8.7 summary)

`Error::Category` enum includes `SourceControl`, `Authentication`, `Network`, `Conflict`. `Error::from_libgit2(code, klass, msg)` routes libgit2 diagnostics to the right category. `Category::Conflict` signals that an operation is blocked pending conflict resolution — the UI will route to the conflict resolution panel (M4); for M1b it is observable through integration tests. `Error::underlying` carries the libgit2 `{source, code, detail}` for "show details" affordances.

### Storage format (§7.1 summary)

Project = directory with `manifest.json`, `schema/...`, `records/{entity-type-id}/{xx}/{record-uuid}.json` (hash-prefix dirs). `.git/` is provider-managed; libgit2's merge state under `.git/` survives process restart, which is why `get_conflicts()` still returns the conflict list after close/reopen.

---

## Spec cross-references

### §6.9 excerpt — conflict resolution UI and interception (verbatim)

**Conflict resolution UI** — when a sync produces conflicts, affected records are shown side-by-side (local vs. incoming), field by field, with options to accept either version or manually set a value; raw file-level conflict markers are never exposed to the user. Interception works as follows: the provider performs a programmatic three-way merge (for `GitProvider`, `git_remote_fetch` followed by `git_merge_trees` with manual index handling — never the default checkout path that writes conflict markers to disk) and surfaces each conflict as three byte blobs (ancestor, local, incoming) via `get_conflicts()`. The JSON-aware field-level merge logic lives in the application core, not in the provider — the provider is deliberately unaware of JSON structure so that future SVN/Perforce providers surface the same three-blob shape with no provider-side JSON handling. This is a significant implementation requirement (see §15 M1b).

### §8.4 excerpt — `ErrorState`, `RecoveryFlow`, resolution semantics, rule-6 staged enforcement (verbatim)

```cpp
enum class ErrorState {
    DetachedHead,                   // HEAD is not on a branch (git)
    IncompleteMerge,                // merge in progress with unresolved conflicts
    RepositoryLocked,               // index lock or provider lock preventing writes
    AuthenticationRequired,         // remote rejected credentials or no credentials available
    RemoteUnreachable,              // network/DNS failure talking to the remote
    PushRejected,                   // non-fast-forward, hook rejection, or equivalent submit failure
    DirtyWorkingCopyOnBranchSwitch, // branch switch attempted with uncommitted changes
    CentralizedCommitBehind,        // SVN/Perforce "update before commit" state
    DivergentBranches,              // local and remote have diverged; pull required before push
};

enum class ActionId {
    Retry, Cancel,
    Pull, Push, Sync,
    Stash, ApplyStash, DropStash,
    AbortMerge, ContinueMerge, OpenConflictResolver,
    ReattachHead, CreateBranchHere,
    Reauthenticate, EditCredentials,
    OpenSettings,
};

struct WorkflowMetadata {
    std::string branching_model_name;
    std::string branching_model_description;
    struct RecoveryFlow {
        ErrorState  error_state;
        std::string title;
        std::string description;
        struct Step {
            std::string label;
            std::string description;
            ActionId    action_id;
        };
        std::vector<Step> steps;
    };
    std::vector<RecoveryFlow> recovery_flows;
};
```

**Conflict surface.**

```cpp
struct Conflict {
    enum class Kind {
        BothModified,        // ancestor, local, and incoming all present; both sides diverged
        BothAdded,           // no ancestor; both sides added differing content at the same path
        ModifiedVsDeleted,   // local modified; incoming deleted
        DeletedVsModified,   // local deleted; incoming modified
    };
    std::filesystem::path       file_path;
    Kind                        kind;
    std::optional<std::string>  base_content;      // ancestor blob; nullopt for BothAdded
    std::optional<std::string>  local_content;     // nullopt when kind == DeletedVsModified
    std::optional<std::string>  incoming_content;  // nullopt when kind == ModifiedVsDeleted
};
// The three byte blobs are surfaced unparsed. JSON-aware field-level merge logic
// lives in the application core (see §6.9) — the provider is deliberately unaware
// of JSON structure so that future providers (SVN, Perforce) surface the same
// three-blob shape with no provider-side JSON handling.
```

**Resolution semantics.** `resolve()` is called once per conflicted file after the application core has reconciled the three blobs surfaced by `get_conflicts()` into a user-approved result. The `resolved_content` parameter is the complete post-merge contents of the file as UTF-8 bytes — for record and schema files this is the fully merged JSON, already shaped as the provider would find it on disk after a clean commit. A present value instructs the provider to write the file (via the same atomic temp-file-then-rename path as §7.1) and clear the conflict from its index; `std::nullopt` instructs the provider to accept a delete (the correct resolution for `ModifiedVsDeleted` and `DeletedVsModified` conflicts where the user chooses the deletion side) — the provider removes the working-copy file if present and marks the path resolved. The provider does not produce a merge commit itself; after every conflict has been resolved, the application invokes the normal commit pipeline (§7.2) which writes any remaining dirty working copies, runs `SchemaValidator`, and calls `stage()` + `commit()` with a merge-commit message. Partial resolution is supported: the UI may call `resolve()` for some files and defer others; `get_conflicts()` continues to report the unresolved set until all are handled, and `commit()` fails with a `Category::Conflict` error if invoked while conflicts remain.

**Rule-6 exhaustiveness — staged enforcement.** CLAUDE.md rule 6 requires the application to guide the user out of every state it can create. Decision #27 expresses this as code through three CI-enforced checks; each lands with the code it depends on, so the rule-6 guarantee is enforced at the earliest moment each piece is meaningful.

1. **`ErrorState` → `RecoveryFlow` coverage (lands at M1b — this milestone).** A unit test walks `enum class ErrorState`; for each value, it passes if *some* shipped provider's `WorkflowMetadata::recovery_flows` contains a `RecoveryFlow` whose `error_state` equals that value, or if the value is listed in the `kUnshippedErrorStates` array in `src/core/workflow.hpp`. Otherwise it fails. `kUnshippedErrorStates` is the narrow, visible escape hatch for states that belong to providers not yet shipped — v1 uses it for `ErrorState::CentralizedCommitBehind`, which is SVN/Perforce-only. When SvnProvider ships, the same commit that adds the provider also removes `CentralizedCommitBehind` from `kUnshippedErrorStates` (and so the test now requires a flow for it). Adding a new `ErrorState` value therefore forces the author to either supply a `RecoveryFlow` from a shipped provider in the same commit or add the value to `kUnshippedErrorStates` — both reviewer-visible.
2. **`ErrorState` → renderer coverage (lands at M3–M4).**
3. **`ActionId` → dispatch entry coverage (lands at M4 with the dispatch table).**

`TestProvider` (§8.4a) lives under `tests/support/` and is deliberately excluded from check (1) — the test walks shipped providers registered under `src/vcs/`, not test doubles.

### §8.4a TestProvider conflict seeding helpers (verbatim)

Conflict-seeding helpers — one per `Conflict::Kind` variant. These append a fully-shaped `Conflict` to the conflicts vector so tests exercising the JSON-aware merge logic in the application core (§6.9) never hand-roll `Conflict` structs; the shape stays consistent as `Conflict` evolves.

```cpp
void seed_conflict_both_modified(std::filesystem::path file,
                                 std::string base,
                                 std::string local,
                                 std::string incoming);
void seed_conflict_both_added(std::filesystem::path file,
                              std::string local,
                              std::string incoming);
void seed_conflict_modified_vs_deleted(std::filesystem::path file,
                                       std::string base,
                                       std::string local);
void seed_conflict_deleted_vs_modified(std::filesystem::path file,
                                       std::string base,
                                       std::string incoming);
```

---

## Relevant resolved decisions (from `docs/decisions.md`)

| # | Decision | Bearing on M1b |
|---|----------|----------------|
| 23 | **Pull strategy (distributed providers):** Merge-on-pull is the v1 default. Implemented as `git_remote_fetch` + programmatic merge with manual index handling — the default checkout path that would write conflict markers to disk is never invoked. Each conflict surfaces as three byte blobs (ancestor, local, incoming); JSON-aware field-level merge logic lives in the application core. Rebase-on-pull is deferred. | The load-bearing decision for this milestone. |
| 27 | **Typed workflow-metadata handshake:** Rule-6 exhaustiveness expressed as three CI-checked constituents. (1) lands in M1b. | The exhaustiveness test `tests/core/workflow_rule_6_coverage_test.cpp` lands this milestone. |

---

## Deliverable detail (§15.4, verbatim)

M1b adds conflict interception, the full `WorkflowMetadata` surface for `GitProvider`, and the first of the three rule-6 exhaustiveness CI checks (decision #27, constituent 1). A throwaway prototype precedes the milestone; the production work derives from the prototype's findings.

**Pre-milestone prototype.** Before M1b opens, a scratch branch (not merged to main) exercises `git_merge_trees` + manual index handling against a hand-authored divergent repository, proving that no code path in the chosen sequence causes libgit2 to invoke the default checkout — which would write `<<<<<<<` / `=======` / `>>>>>>>` conflict markers to the working-copy files on disk. The prototype is deliberately single-file, ~200 LOC, and is discarded once its findings inform the production implementation. Deliverable: a short write-up `docs/prototypes/m1b_merge_without_checkout.md` summarising the API sequence used, the libgit2 version tested, and any surprises. The document stays in the repo long-term — future provider work or libgit2 upgrades that need to re-verify the behaviour re-run the sequence against the then-current libgit2. `docs/prototypes/` is a new subdirectory introduced at M1b.

**`src/vcs/git/merge.hpp` / `.cpp`** — the in-process three-way merge:

- `std::expected<MergeResult, Error> merge_trees(git_repository*, git_oid ancestor_oid, git_oid local_oid, git_oid incoming_oid)`.
- Builds a merged `git_index` via `git_merge_trees`; walks the index and classifies each entry by its stage triple into an auto-merged entry or one of the four `Conflict::Kind` values.
- For auto-merged entries, flushes the merged blob to disk via the M0b `atomic_write` — never via `git_checkout_tree`, `git_checkout_index`, or `git_checkout_head`.
- For conflicting entries, extracts the three stage blobs (`ancestor`, `local`, `incoming`) as `std::vector<std::byte>` and accumulates a `Conflict` per path.
- A CI-level symbol check (`nm` / `dumpbin` scan of the `vcs_git` library) asserts that `git_checkout_tree`, `git_checkout_index`, and `git_checkout_head` are not referenced from this translation unit — making accidental reintroduction a build-time failure.
- Unit tests cover every `Conflict::Kind`, plus the auto-merge paths (three-side equality; two-side equality; disjoint line-range modifications of the same file).

**`GitProvider` updates:**

- `get_conflicts()` returns the conflict list produced by the most recent pull that encountered divergence. Conflicts persist across process restarts — they live in libgit2's merge state on disk, and the M0b orphan-tempfile sweep leaves that state alone.
- `resolve(path, std::optional<std::string> resolved_content)` writes the resolved content via `atomic_write` (or deletes the file if `nullopt`), updates the libgit2 merge state, and when the last conflict is resolved, produces the merge commit through the normal commit pipeline (rule 9 — resolution stages the fix, commit produces the persistent artifact).
- `pull()` is extended: on non-FF, it calls `merge_trees`, stages the auto-merged files, and leaves any conflicts visible via `get_conflicts()`. The M1a error on divergence is removed. The paired M1a test that asserted the divergence error now flips green in the same commit that lands this change, confirming the cross-milestone contract.
- `get_workflow_metadata()` now returns a populated `WorkflowMetadata`:
  - `branching_model_name = "Feature branches"`, `branching_model_description` expanded to the full two-sentence explanation.
  - `recovery_flows` contains a `RecoveryFlow` for every `ErrorState` value except those in `kUnshippedErrorStates`. M1b adds `CentralizedCommitBehind` to `kUnshippedErrorStates`; it comes back out when `SvnProvider` ships.
- Each `RecoveryFlow` has a title, a description, and 2–5 `Step`s. Every `Step.action_id` is a real `ActionId` (Step is required-action by spec §8.4 — pure guidance lives in the flow-level description or step descriptions). Example mappings: `DetachedHead` → `ReattachHead` + `CreateBranchHere`; `IncompleteMerge` → `OpenConflictResolver` + `AbortMerge` + `ContinueMerge`; `AuthenticationRequired` → `Reauthenticate` + `EditCredentials`; `PushRejected` → `Pull` + `Push`; `DirtyWorkingCopyOnBranchSwitch` → `Stash` + `ApplyStash`.

**Rule-6 exhaustiveness check (1) — CI enforced from M1b.**

- New unit test `tests/core/workflow_rule_6_coverage_test.cpp`: construct each shipped provider (only `GitProvider` at M1b, via a local-repo fixture), collect the `ErrorState` values covered by its `WorkflowMetadata.recovery_flows`, and assert every `ErrorState` enum value is either covered by some shipped provider or listed in `kUnshippedErrorStates`.
- Enum iteration: `magic_enum` is the recommended approach (header-only, wide platform support, added to `vcpkg.json` at M1b as a test-only dependency); an X-macro over `ErrorState` is an acceptable alternative if `magic_enum` is rejected for any reason. The test API is the same either way.
- `TestProvider` is excluded from this iteration — the check walks shipped providers registered under `src/vcs/`; `TestProvider` lives under `tests/support/` and is not reachable from that walk (spec §8.4a, decision #27).
- CI failure mode: adding an `ErrorState` value without a matching `RecoveryFlow` in any shipped provider or a `kUnshippedErrorStates` entry breaks the build at the test stage — not at runtime, not at pull time. This is the M1b expression of CLAUDE.md rule 6.

**Integration tests under `tests/vcs/git/merge/`.** Build a divergent scratch repository in-test (two commits off a shared ancestor, varying what is touched on each side), pull the second branch into a working copy on the first, and assert:

- No conflict markers are written to disk. A recursive byte-scan of every file under the project tree after `pull()` finds zero occurrences of `<<<<<<<`, `=======`, or `>>>>>>>` on their own line.
- `get_conflicts()` returns the expected `Conflict` list with the right `Kind` for each path and the correct ancestor / local / incoming blobs.
- `resolve(path, merged_content)` for each conflict produces a valid merge commit with two parents once the last conflict is resolved; `get_pending_changes()` is empty afterward.
- An unresolved merge left pending across a `Project` close-reopen cycle re-surfaces the same conflict list via `get_conflicts()` — durable merge state survives process restarts.
- Every `Conflict::Kind` has at least one passing integration test (`BothModified`, `BothAdded`, `ModifiedVsDeleted`, `DeletedVsModified`).

**Exit criteria.** Every `Conflict::Kind` has a passing integration test. `get_workflow_metadata()` returns a `RecoveryFlow` for every `ErrorState` not in `kUnshippedErrorStates`. The rule-6 CI check is green and demonstrably fails on an introduced gap (validated by a local repro that adds a new `ErrorState` value without coverage and watches the test reject it). The M1a-era divergence error on `pull()` is gone and the paired test is green. No UI work is done in M1b — every flow is exercised through integration tests. The prototype document is committed under `docs/prototypes/`.

---

## What M2a adds next

M2a is where the data plane arrives: the schema engine, record CRUD with stateless diff-based migration at load time, `RecordWorkingCopy`, the custom `CommandStack`, concrete `Command`s (`CreateRecord`, `SetFieldValue`, `DeleteRecord`), the commit pipeline, and `SchemaValidator` with all four pre-commit validation categories. The merge-commit path you wire at M1b (`resolve()` → normal commit pipeline) only becomes real once M2a ships the pipeline; M1b proves the provider side in isolation via integration tests that drive atomic writes directly.

---

## Source docs

- `docs/philotechnia_spec.md` §6.9, §8.4, §8.4a, §15.4
- `docs/decisions.md` rows 23, 27
- `CLAUDE.md` — rules 1, 4, 6, 9
