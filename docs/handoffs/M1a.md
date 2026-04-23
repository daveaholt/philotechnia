# Handoff — M1a: Basic VCS (`SourceControlProvider` + `GitProvider`)

*Derived from `docs/philotechnia_spec.md` §15.3. If this document and the spec disagree, the spec wins.*

---

## Mission

Ship the source-control interface layer, the `GitProvider` implementation backed by libgit2, and the project-owned `Error` type they depend on. Record storage, the schema system, and the commit pipeline still sit ahead in M2. Success at M1a is that a test binary (or the M0b stub CLI) can drive the full `GitProvider` surface — initialise, clone, stage, commit, push, **fast-forward** pull, branching, history, stash, auth — against a scratch on-disk repo. Conflict handling (`get_conflicts` / `resolve`) and the full `WorkflowMetadata.recovery_flows` are explicitly M1b.

**Predecessor:** M0b — storage primitives, `manifest.json`, `LocalProfile` in OS app data. **Successor:** M1b — conflict interception, rule-6 exhaustiveness check, populated `WorkflowMetadata`.

---

## Foundation

### Project in one paragraph

Philotechnia is a cross-platform C++ desktop application for structured record management. Projects are version-controlled directories of plain JSON files (one file per record, hash-prefix directories). The application includes a first-party source control UI built on an abstract `SourceControlProvider` interface — Git via libgit2 ships in v1; SVN and Perforce are later milestones. The UI is Qt. JSON is yyjson. All undo/redo and dirty-state tracking flows through a custom `CommandStack`, not Qt's built-in undo framework.

### Non-negotiable architectural rules (from CLAUDE.md)

Rules active in M1a:

1. **The UI layer must never depend on a concrete `SourceControlProvider` implementation.** All source control calls from UI or application core go through the abstract `SourceControlProvider` interface only. Never `#include` a provider header from UI code. A compile-only test at M1a confirms `src/vcs/provider.hpp` has no libgit2, yyjson, or Qt dependency.
3. **Qt must be dynamically linked.** Already enforced in `CMakeLists.txt`.
4. **All record writes use atomic rename.** Already provided by `src/storage/atomic_write.hpp` from M0b; reuse it from M1b's `resolve()` path.
6. **The application must be fully self-sufficient for all source control states it creates.** The `ErrorState` enum you define in `src/core/workflow.hpp` is the type-level expression of this rule. `GitProvider::get_workflow_metadata()` returns an empty `recovery_flows` vector at M1a; M1b populates it. CI enforcement of rule-6 coverage (decision #27, constituent 1) lands at M1b.
7. **The source control UI is provider-aware and guides users toward correct workflows.** Not visible in M1a (no UI), but the `WorkflowMetadata` / `ActionId` scaffolding you ship here is what lets it happen.

Rules 2, 5, 8, 9 are forward-looking at M1a — do not close off the paths they require.

### Tech stack slice

| Layer | Technology | Where it lands |
|-------|-----------|----------------|
| Source control v1 | libgit2 | `src/vcs/git/git_provider.cpp` (+ credentials shims) |
| Auth | OS credential store (Keychain / Credential Manager / libsecret) | `src/vcs/git/credentials_{mac,win,linux}.cpp` |
| Testing | Catch2 v3 | `tests/vcs/git/`, `tests/core/workflow_*` |
| Test double | `TestProvider` | `tests/support/test_provider.{hpp,cpp}` compiled into `test_support` static library |

### Error type (§8.7, verbatim)

```cpp
struct Error {
    enum class Category {
        Io,              // filesystem I/O failure (read, write, rename, permissions)
        NotFound,        // an expected record, entity type, branch, or file is absent
        Parse,           // yyjson or other format parse failure; message cites location
        Validation,      // SchemaValidator reported a Blocking issue (see §6.10)
        SourceControl,   // provider-specific failure not captured by a more specific category
        Authentication,  // credentials rejected or unavailable (see §13.10, §13.12)
        Network,         // remote unreachable, DNS failure, or timeout
        Conflict,        // unresolved merge conflict blocks the requested operation
        FormatVersion,   // on-disk format_version exceeds the running build (see §7.2)
        Cancelled,       // user-initiated cancellation of a long-running operation
        Internal,        // invariant surfaced as a recoverable error rather than a crash
    };

    Category    category;
    std::string message;   // user-presentable; short and actionable. Shown inline in UI.

    struct ProviderError {
        std::string source;   // "libgit2", "posix", "win32", "yyjson", ...
        int         code;     // provider-native numeric code
        std::string detail;   // provider-native message, if any
    };
    std::optional<ProviderError> underlying;
};
```

**Category boundaries** (spec §8.7): `Io` / `Parse` / `Internal` surface as inline error states; `NotFound` usually flows through `std::expected`; `Validation` is raised by the commit pipeline when `SchemaValidator` blocks (M2a); `SourceControl` / `Authentication` / `Network` route into the recovery UI via the appropriate `ErrorState`; `Conflict` signals that an operation is blocked pending conflict resolution; `FormatVersion` triggers the "newer version required" dialog; `Cancelled` unwinds cancelled operations silently.

Helper constructors (`Error::io(msg)`, `Error::from_libgit2(int code, int klass, const char* msg)`, `Error::validation(msg)`, etc.) live in `error.hpp` and are the preferred call sites — raw brace-initialisation is discouraged because it skips the underlying-diagnostic capture logic.

### Storage format (§7.1 summary)

Project = version-controlled directory with `manifest.json`, `schema/entity_types/{uuid}.json`, `schema/enums/{uuid}.json`, and `records/{entity-type-id}/{xx}/{record-uuid}.json` (hash-prefix directories). All filenames are UUIDs. The storage format is VCS-agnostic; `.git/` is provider-managed and not part of the format. Atomic writes via `src/storage/atomic_write.hpp` — never direct writes to the final path.

---

## Spec cross-references

### §6.6 Collaboration & Sync (verbatim)

The configured source control provider is the collaboration mechanism. There is no custom sync protocol or server.

- **Sharing:** The project directory is pushed to a shared remote. Team members clone or check out the project locally and open it in Philotechnia.
- **Committing:** Changes accumulate in the working state (dirty state tracking, §6.8) until the user explicitly commits via the source control panel. The logged-in Philotechnia user is mapped to the commit author identity. For centralized providers, committing requires network connectivity.
- **Pushing and pulling:** Initiated from the source control panel. Pull fetches and merges remote changes; conflicts are surfaced in the conflict resolution UI (§6.9) rather than as raw file-level conflict markers.
- **Branching:** Where the provider supports it, users can create and switch branches from the source control panel.

### §8.4 `SourceControlProvider` interface (verbatim — this is the shape you are shipping)

```cpp
struct PendingChange {
    enum class Type { Added, Modified, Deleted };
    std::string file_path;
    Type        type;
    std::string description;  // human-readable, e.g. "Record 'Acme Corp' modified"
};

struct HistoryEntry {
    std::string id;           // commit hash, revision number, changelist ID, etc.
    std::string author;
    std::string message;
    std::chrono::system_clock::time_point timestamp;
};

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

struct RemoteStatus {
    bool available;       // false if provider cannot determine (e.g., no network)
    int  local_only;      // local commits / changes not yet on remote
    int  remote_only;     // remote changes not yet local
};

struct StashEntry {
    std::string id;        // provider-specific reference (e.g. "stash@{0}")
    std::string message;
    std::chrono::system_clock::time_point timestamp;
};

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

class SourceControlProvider {
public:
    virtual ~SourceControlProvider() = default;

    // Capability flags — pure accessors; cannot fail.
    virtual bool supports_staging() const = 0;
    virtual bool supports_offline_commits() const = 0;
    virtual bool supports_branching() const = 0;
    virtual bool supports_stashing() const = 0;

    // Provider-supplied UI labels.
    virtual std::string commit_action_label()  const = 0;
    virtual std::string sync_action_label()    const = 0;
    virtual std::string receive_action_label() const = 0;

    // Lifecycle
    virtual std::expected<void, Error> initialise(const std::filesystem::path& dir) = 0;
    virtual std::expected<void, Error> clone(const std::string& url,
                                             const std::filesystem::path& dest) = 0;

    // Working state
    virtual std::expected<std::vector<PendingChange>, Error> get_pending_changes() = 0;
    virtual std::expected<void, Error> stage(
        const std::vector<std::filesystem::path>& files) = 0;
    virtual std::expected<void, Error> commit(const std::string& message,
                                              const std::string& author) = 0;

    // Remote sync
    virtual std::expected<void, Error>         push() = 0;
    virtual std::expected<void, Error>         pull() = 0;
    virtual std::expected<RemoteStatus, Error> remote_status() = 0;

    // Branching
    virtual std::expected<std::vector<std::string>, Error> get_branches() = 0;
    virtual std::expected<std::string, Error>              current_branch() = 0;
    virtual std::expected<void, Error>                     create_branch(const std::string& name) = 0;
    virtual std::expected<void, Error>                     switch_branch(const std::string& name) = 0;

    // Stashing
    virtual std::expected<void, Error>                    stash(const std::string& message) = 0;
    virtual std::expected<std::vector<StashEntry>, Error> get_stashes() = 0;
    virtual std::expected<void, Error>                    apply_stash(const std::string& stash_id) = 0;
    virtual std::expected<void, Error>                    drop_stash(const std::string& stash_id) = 0;

    // History
    virtual std::expected<std::vector<HistoryEntry>, Error> get_history(
        const std::filesystem::path& file, int limit = 100) = 0;

    // Conflict resolution — wired at M1b; at M1a these return Error::internal(...)
    virtual std::expected<std::vector<Conflict>, Error> get_conflicts() = 0;
    virtual std::expected<void, Error> resolve(const std::filesystem::path& file,
                                               std::optional<std::string> resolved_content) = 0;

    // Workflow guidance — pure accessors over provider-static metadata; cannot fail.
    virtual WorkflowMetadata get_workflow_metadata() const = 0;
    virtual std::string      suggested_primary_branch_name() const = 0;
};
```

**Resolution semantics** (§8.4): `resolve()` is called once per conflicted file after the application core has reconciled the three blobs into a user-approved result. `resolved_content` present = full post-merge UTF-8 file contents; `nullopt` = accept delete (for `ModifiedVsDeleted` / `DeletedVsModified`). Provider does not produce the merge commit — the application invokes the normal commit pipeline after every conflict has been resolved. That code path lands in M1b + M2a; at M1a both methods return `Error::internal("not yet implemented at M1a")`.

**Action dispatch** (§8.4): `ActionId` values resolve to behaviour through a single dispatch table in `src/core/workflow.cpp`. UI widgets never `switch` on `ActionId` themselves. At M1a, `dispatch(ActionId) -> std::expected<void, Error>` is a stub returning `Error::internal("ActionId dispatch not yet wired at M1a")`; M4 replaces the stub with a compile-exhaustive `switch`.

**Rule-6 exhaustiveness — staged enforcement** (§8.4, decision #27):

1. `ErrorState` → `RecoveryFlow` coverage test lands at **M1b**. Not in M1a.
2. `ErrorState` → renderer coverage lands at **M3–M4**.
3. `ActionId` → dispatch entry coverage (compile-time) lands at **M4**.

### §8.4a `TestProvider` (verbatim — ship scaffolding now; tests consume from M1a)

`TestProvider` is the test double for `SourceControlProvider`. It lives under `tests/support/test_provider.hpp` / `.cpp`, is compiled into a `test_support` static library, and is linked only by test binaries — never by the shipping application. M0b ships the scaffolding; tests consume it from M1a onward as real code that takes a `SourceControlProvider&` comes online.

**Role.** Integration tests of the commit pipeline (§7.2), recovery-flow dispatch (§8.4), and conflict resolution (§6.9) all need a provider they can drive deterministically without a real git repo. `TestProvider` provides that surface. It is explicitly **not** a shipped provider and is excluded by construction from the rule-6 exhaustiveness checks: check (1) walks shipped providers registered under `src/vcs/`, and `TestProvider` lives under `tests/support/`.

**Programmable state.** Every piece of state a provider surfaces is a public, mutable member. Tests pre-load whatever shape they need — pending changes, branches, stashes, history, conflicts, remote status — before driving the code under test. Defaults are "git-like and healthy": all capability flags true, one branch named `main`, no pending changes, no conflicts, `RemoteStatus{available=true, 0, 0}`.

**Fault injection.** A per-operation FIFO queue of `Error` values. The next call to each `Operation` pops its queued failure (if any) and returns it; an empty queue means the call performs the default action against the programmable state.

**Call recording.** Every invocation is captured in a `calls` vector for test assertions. Successful commits additionally populate a `commits` vector with `{message, author, files}` tuples.

```cpp
// tests/support/test_provider.hpp (abridged — full form in spec §8.4a)
class TestProvider final : public SourceControlProvider {
public:
    bool supports_staging_         = true;
    bool supports_offline_commits_ = true;
    bool supports_branching_       = true;
    bool supports_stashing_        = true;

    std::string commit_action_label_  = "Commit";
    std::string sync_action_label_    = "Push";
    std::string receive_action_label_ = "Pull";

    std::vector<PendingChange> pending_changes;
    std::vector<std::string>   branches        = {"main"};
    std::string                current_branch_ = "main";
    std::vector<Conflict>      conflicts;
    std::vector<HistoryEntry>  history;
    std::vector<StashEntry>    stashes;
    RemoteStatus               remote_status_  = {true, 0, 0};
    WorkflowMetadata           workflow_metadata_;
    std::string                suggested_primary_branch_name_ = "main";

    enum class Operation {
        Initialise, Clone,
        GetPendingChanges, Stage, Commit,
        Push, Pull, GetRemoteStatus,
        GetBranches, CurrentBranch, CreateBranch, SwitchBranch,
        Stash, GetStashes, ApplyStash, DropStash,
        GetHistory, GetConflicts, Resolve,
    };
    void enqueue_failure(Operation op, Error err);

    struct Call { Operation op; std::vector<std::string> string_args; std::vector<std::filesystem::path> path_args; };
    std::vector<Call> calls;

    struct RecordedCommit { std::string message; std::string author; std::vector<std::filesystem::path> files; };
    std::vector<RecordedCommit> commits;

    // Conflict-seeding helpers (one per Conflict::Kind) — full signatures in spec §8.4a.
};
```

### §6.9 excerpt — source control guidance and branching defaults (verbatim)

Source control is surfaced as a first-party UI feature through the abstract `SourceControlProvider` interface. The UI presents Philotechnia's vocabulary — pending changes, history, conflicts — regardless of which provider is active. Action labels (e.g., "Push" vs "Submit") are supplied by the provider via its label methods so the UI layer never needs to know the concrete provider type.

**Primary branch** is configured at project creation time — the user confirms the name (git default: "main"; SVN default: "trunk"; provider suggests via `suggested_primary_branch_name()`). The primary branch name is stored in `manifest.json`. For projects opened from an existing repository that predates Philotechnia, the app detects the current default branch and prompts the user to confirm it before the first session begins.

---

## Relevant resolved decisions (from `docs/decisions.md`)

| # | Decision | Bearing on M1a |
|---|----------|----------------|
| 1 | **Sync / collaboration architecture:** `SourceControlProvider` abstract interface. Git (libgit2) in v1. | The interface you are defining. |
| 6 | **Source control abstraction:** Strategy pattern; UI and core depend only on the interface. Capability flags drive feature visibility. | Compile-only test asserts `provider.hpp` has no libgit2/yyjson/Qt includes. |
| 11 | **Offline-first scope:** Applies to distributed providers (git) only. | `GitProvider::supports_offline_commits()` returns true. |
| 17 | **Provider selection UX:** Auto-detect on open (no confirmation dialog). Active provider in title bar. | `open_project()` detects `.git/` silently. |
| 18 | **Branch strategy UX:** Branch-first default; primary branch name stored in `manifest.json`; stashing where supported. | All capability flags true; label overrides `"Commit"` / `"Push"` / `"Pull"`; `suggested_primary_branch_name()` returns `"main"`. |
| 19 | **Authentication & identity model:** Machine-local profile supplies commit author. No project user registry. | `GitProvider` ctor takes a `LocalProfile`; uses `username` + `email` as the `git_signature`. |
| 21 | **Git hosting compatibility (v1):** GitHub, GitLab, Gitea. SSH + HTTPS. Credentials via OS credential store. | Per-platform credential shims in `src/vcs/git/credentials_*.cpp`; SSH key settings read-only at M1a, UI arrives in M3. |
| 27 | **Typed workflow-metadata handshake:** `ErrorState`, `ActionId` typed enums; single dispatch table; three CI-enforced rule-6 constituents. | At M1a: `kUnshippedErrorStates` is empty; `dispatch()` is a stub; rule-6 constituent (1) lands in M1b. |
| 28 | **User identity key:** `LocalProfile.username` is the sole identity key. | `LocalProfile.username` is the `git_signature` author name. |

---

## Deliverable detail (§15.3, verbatim)

M1a adds the source-control interface layer, the `GitProvider` implementation, and the `Error` type they depend on. Record storage, the schema system, and the commit pipeline still sit ahead in M2; M1a's success criterion is that a test binary (or the M0b stub CLI) can drive the full `GitProvider` surface — initialise, clone, stage, commit, push, fast-forward pull, branching, history, stash, auth — against a scratch on-disk repo. Concrete deliverables:

**`src/core/error.hpp` / `.cpp`** — the common error type (spec §8.7):

- `struct Error` with nested `enum class Category` exactly as specified in §8.7: `Io`, `NotFound`, `Parse`, `Validation`, `SourceControl`, `Authentication`, `Network`, `Conflict`, `FormatVersion`, `Cancelled`, `Internal`.
- Nested `struct ProviderError { std::string source; int code; std::string detail; }`; `Error::underlying` is `std::optional<ProviderError>`.
- Helper constructors in `error.hpp` (preferred over brace-initialisation per the Conventions section of CLAUDE.md): `Error::io(std::string msg)`, `Error::io_from_posix(int errno_value, std::string msg)`, `Error::io_from_win32(uint32_t code, std::string msg)`, `Error::parse(std::string msg, size_t byte_offset)`, `Error::validation(std::string msg)`, `Error::source_control(std::string msg)`, `Error::authentication(std::string msg)`, `Error::network(std::string msg)`, `Error::conflict(std::string msg)`, `Error::format_version(int on_disk, int running)`, `Error::cancelled()`, `Error::internal(std::string msg)`, plus `Error::from_libgit2(int code, int klass, const char* libgit2_msg)` which routes the libgit2 diagnostic into `Category::SourceControl` / `Authentication` / `Network` / `Conflict` based on the libgit2 class and code.
- Logging helper `format_error(const Error&) -> std::string` renders `"[{source} {code}] {detail}"` when `underlying` is populated (§8.7).
- Unit tests round-trip each category through its helper; `from_libgit2` maps `GIT_EAUTH` → `Category::Authentication`, `GIT_EUNMERGED` → `Category::Conflict`, `GIT_ELOCKED` → `Category::SourceControl`, `GIT_ENOTFOUND` → `Category::NotFound`.

**`src/core/workflow.hpp` / `.cpp`** — typed workflow-metadata scaffolding (spec §8.4, decision #27):

- `enum class ErrorState` with the nine values in §8.4: `DetachedHead`, `IncompleteMerge`, `RepositoryLocked`, `AuthenticationRequired`, `RemoteUnreachable`, `PushRejected`, `DirtyWorkingCopyOnBranchSwitch`, `CentralizedCommitBehind`, `DivergentBranches`.
- `enum class ActionId` with the sixteen values in §8.4: `Retry`, `Cancel`, `Pull`, `Push`, `Sync`, `Stash`, `ApplyStash`, `DropStash`, `AbortMerge`, `ContinueMerge`, `OpenConflictResolver`, `ReattachHead`, `CreateBranchHere`, `Reauthenticate`, `EditCredentials`, `OpenSettings`.
- `struct WorkflowMetadata` with `branching_model_name`, `branching_model_description`, and nested `RecoveryFlow` + `Step` exactly as defined in §8.4.
- `constexpr std::array<ErrorState, N> kUnshippedErrorStates` — empty at M1a; M1b adds `CentralizedCommitBehind`.
- `src/core/workflow.cpp` ships a stub `dispatch(ActionId) -> std::expected<void, Error>` function that returns `Error::internal("ActionId dispatch not yet wired at M1a")` for any value. The M4 deliverable replaces the stub with a `switch` over every `ActionId` enumerator with no `default` — that is what gives rule-6 constituent (3) its compile-time enforcement (decision #27). Landing the file now gives the future switch a home, which means adding an `ActionId` value in M2–M3 surfaces as a runtime-test failure rather than a missing file.
- Unit tests at M1a: `kUnshippedErrorStates` contains only valid `ErrorState` values; `dispatch(...)` returns the documented stub error for every enumerator. Rule-6 constituents (1), (2), (3) each land with their dependencies — (1) at M1b, (2) at M3–M4, (3) at M4.

**`src/vcs/provider.hpp`** — the abstract `SourceControlProvider` interface (spec §8.4, reproduced in CLAUDE.md):

- Exactly the interface defined in §8.4 — capability flags, labels, lifecycle, working state, remote sync, branching, stashing, history, conflicts, workflow metadata.
- Supporting structs (`PendingChange`, `RemoteStatus`, `HistoryEntry`, `StashEntry`, `Conflict` with nested `enum class Kind { BothModified, BothAdded, ModifiedVsDeleted, DeletedVsModified }` and three `std::vector<std::byte>` blobs for ancestor / local / incoming) live in `src/vcs/provider.hpp` or in a sibling `src/vcs/types.hpp` if the header threatens to grow past ~200 lines.
- No libgit2, yyjson, or Qt includes in this header. A compile-only test confirms the property — a minimal TU includes `src/vcs/provider.hpp` plus `<QtCore/QObject>` and compiles without `find_package(unofficial-git2)` having been called.

**`src/vcs/git/git_provider.hpp` / `.cpp`** — the libgit2-backed provider:

- `class GitProvider : public SourceControlProvider`; constructor takes a `std::filesystem::path` (project root) and a `LocalProfile` (commit author identity, read from M0b's `local_profile.json`).
- libgit2 lifecycle: a refcounted singleton wraps `git_libgit2_init()` / `git_libgit2_shutdown()` so multiple providers in the same process share one initialisation.
- Every libgit2 call goes through a thin helper `check(int rc) -> std::expected<void, Error>` that calls `git_error_last()` on non-zero, producing `Error::from_libgit2(rc, err->klass, err->message)`.
- **Operations shipped at M1a:** `initialise`, `clone`, `get_pending_changes`, `stage`, `commit` (using `LocalProfile.username` + `.email` as `git_signature`), `push`, `pull` **fast-forward only** (see below), `remote_status`, `get_branches`, `current_branch`, `create_branch`, `switch_branch`, `stash` / `get_stashes` / `apply_stash` / `drop_stash`, `get_history`.
- **`pull()` scope at M1a.** Implemented as `git_remote_fetch` followed by a fast-forward-only merge. If the incoming branch would require a real merge (non-FF, no conflicts) or surfaces any conflict, `pull()` returns `Error::source_control("divergent branches; conflict handling requires M1b")` with `underlying` carrying the libgit2 code. The integration test for this behaviour is explicitly paired to a failing test that flips green once M1b lands — a regression-protection pairing that both documents the M1a boundary and catches any M1b work that silently re-regresses it.
- **Deferred to M1b:** `get_conflicts` and `resolve` return `Error::internal("not yet implemented at M1a")`. `get_workflow_metadata()` returns a `WorkflowMetadata` with `branching_model_name = "Feature branches"`, a one-sentence `branching_model_description`, and an empty `recovery_flows` vector — populating the flows is M1b's defining work.
- Capability flags: `supports_staging()`, `supports_offline_commits()`, `supports_branching()`, `supports_stashing()` all return `true`. Label overrides: `"Commit"`, `"Push"`, `"Pull"`.
- `suggested_primary_branch_name()` returns `"main"`.
- **Auth.** libgit2 callbacks route credentials through the OS credential store behind a common header `src/vcs/git/credentials.hpp` with per-platform implementations `credentials_mac.cpp` (Keychain via `SecItemCopyMatching`), `credentials_win.cpp` (Credential Manager via `CredReadW`), `credentials_linux.cpp` (Secret Service via libsecret). HTTPS credentials go through `git_cred_userpass_plaintext_new`; SSH keys go through `git_cred_ssh_key_new` with key path and passphrase read from app settings (`ssh_key_path`, `ssh_key_passphrase`) — the read path ships at M1a, the settings UI to edit them lands in M3.

**`src/core/project.hpp` / `.cpp` updates** — provider integration:

- `struct Project` gains `std::unique_ptr<SourceControlProvider> provider;` (was absent at M0b per the §15.2 note).
- `open_project()` auto-detects silently (decision #17): a `.git/` subdirectory under the project root selects `GitProvider`; future providers add their own detectors to a small registry in `src/vcs/detect.cpp`. A project with no recognised VCS returns `Error::source_control("unrecognised or absent source control; v1 supports git")`.
- `create_project()` at M1a initialises a git repository via `GitProvider::initialise` before returning, then stages and commits `manifest.json` with the commit message `"Initial project"` and the `LocalProfile` as author — giving every new Philotechnia project a clean initial commit on its primary branch.

**`tests/support/test_provider.hpp` / `.cpp`** — the test double (spec §8.4a):

- `class TestProvider : public SourceControlProvider` with every piece of programmable state as a public mutable member (`std::vector<PendingChange> pending`, `std::vector<std::string> branches = {"main"}`, `RemoteStatus remote = {true, 0, 0}`, etc.).
- FIFO `std::queue<Error>` fault slot per `Operation` enumerator — each provider method, on entry, checks its slot and returns the popped `Error` if any.
- Call log `std::vector<Call>` for tests that assert call sequences or parameter values.
- Compiled into a `test_support` static library via `tests/support/CMakeLists.txt`; the shipping binary never links it (enforced by a CMake assertion that the `philotechnia` target does not transitively depend on `test_support`).
- `TestProvider`'s own unit tests cover FIFO fault semantics, default-state round-tripping, and capability-flag overrides.

**Integration tests under `tests/vcs/git/`.** Each test creates a fresh repository in `std::filesystem::temp_directory_path() / unique-name`, drives the provider, and cleans up on completion. Scenarios:

- `initialise` produces a valid `.git/`; subsequent `open_project` auto-detects and returns a working provider.
- `clone` against a local bare repo set up in-test via `git_repository_init_ext` with `GIT_REPOSITORY_INIT_BARE` reproduces the remote's history.
- Commit round-trip: atomic-write a file, `stage`, `commit`, assert `get_pending_changes()` is empty and `get_history()` shows the new commit with the injected author.
- Branch create / switch / list; stash create / apply / drop.
- Push / fetch / fast-forward pull against a scratch bare remote.
- Divergent pull returns the documented M1a error (paired test asserts success at M1b).
- HTTPS auth against a credential-callback fake that returns a canned credential; SSH auth against an in-test key loaded from `tests/fixtures/ssh/`.

**Exit criteria.** `ctest --output-on-failure` green on all three platforms, including every `GitProvider` integration test listed above. `philotechnia --create-project ./x` produces a directory with a populated `.git/` and an initial commit authored by the `LocalProfile`; `philotechnia --commit ./x -m "msg"` after an edit produces a second commit visible in `get_history`. Divergent `pull()` exits with the documented M1a error. Every `GitProvider` method except `get_conflicts` / `resolve` has ≥1 integration test on the happy path and ≥1 fault-injection test on the error path. No UI work is done in M1a.

---

## What M1b adds next

M1b wires conflict interception (`get_conflicts` / `resolve`), promotes `pull()` from fast-forward-only to programmatic three-way merge via `git_merge_trees` (never the default checkout path), populates `GitProvider::get_workflow_metadata().recovery_flows` with one flow per git-applicable `ErrorState`, adds `CentralizedCommitBehind` to `kUnshippedErrorStates`, and lands rule-6 constituent (1) as a CI-enforced unit test. The paired M1a "divergent pull returns error" test flips green as part of the M1b work.

---

## Source docs

- `docs/philotechnia_spec.md` §6.6, §6.9, §8.4, §8.4a, §8.7, §15.3
- `docs/decisions.md` rows 1, 6, 11, 17, 18, 19, 21, 27, 28
- `CLAUDE.md` — rules 1, 3, 4, 6, 7 (active/referenced)
