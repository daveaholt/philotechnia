# CLAUDE.md — Philotechnia

This file is the primary handoff for AI assistants working on this codebase. Read it before writing or modifying any code. For full context on any decision, see [`docs/philotechnia_spec.md`](docs/philotechnia_spec.md).

---

## Project in one paragraph

Philotechnia is a cross-platform C++ desktop application for structured record management. Projects are version-controlled directories of plain JSON files (one file per record, hash-prefix directories). The application includes a first-party source control UI built on an abstract `SourceControlProvider` interface — Git via libgit2 ships in v1; SVN and Perforce are later milestones. The UI is Qt. JSON is yyjson. All undo/redo and dirty-state tracking flows through a custom `CommandStack`, not Qt's built-in undo framework.

**Current status:** Pre-implementation. Architecture and tech stack are settled. No production code exists yet. The spec is the authoritative source of truth.

---

## Non-negotiable architectural rules

These are hard constraints. Do not work around them — raise a question if one creates a conflict.

1. **The UI layer must never depend on a concrete `SourceControlProvider` implementation.** All source control calls from UI or application core go through the abstract `SourceControlProvider` interface only. Never `#include` a provider header from UI code.

2. **`CommandStack` owns all undo/redo logic. Qt does not.** `QUndoStack` is not used. Qt's `QListView` renders the history list by binding to `CommandStack::history()`, but the stack itself is a custom C++ class in the application core. Dirty state, clean marking, and commit lifecycle live in `CommandStack` and `RecordWorkingCopy` — not in any Qt class.

3. **Qt must be dynamically linked.** Static linking of Qt is prohibited under LGPL 3.0. All three platform builds ship Qt as shared libraries (`.dylib` / `.dll` / `.so`). This must be enforced in `CMakeLists.txt` from M0a onward.

4. **All record writes use atomic rename.** Write to a temp file, then rename over the target. On POSIX use `rename(2)`. On Windows use `SetFileInformationByHandle` with `FileRenameInfoEx` and the flags `FILE_RENAME_FLAG_POSIX_SEMANTICS | FILE_RENAME_FLAG_REPLACE_IF_EXISTS` — this gives POSIX-equivalent atomic replacement (available since Windows 10 build 14393; the platform minimum of 19041 guarantees availability). Do **not** use the older `MoveFileExW` — it has rename-over-open-file limitations that `FILE_RENAME_FLAG_POSIX_SEMANTICS` specifically removes. Never write directly to the final path. Atomicity is at the filesystem level, not across volumes; on startup, scan for orphaned `.tmp` files from interrupted writes and log/recover them.

5. **Hand-written ASM only with a profiled bottleneck.** C++23 is the default for everything. Hand-written assembly or inline `asm` blocks require a measured, reproducible performance regression to justify them. SIMD via intrinsics or `std::simd` is regular C++ and does not require this justification. When evaluating a new third-party dependency, consider its ASM/SIMD characteristics — portability across x86-64 and ARM, graceful fallback on older hardware, and whether it bundles platform-specific assembly that complicates cross-compilation.

6. **The application must be fully self-sufficient for all source control states it creates.** If Philotechnia can put a repository into a state — conflict, detached HEAD, lock, authentication challenge, incomplete merge — it must be able to guide the user out of that state. No user should ever need an external git, SVN, or Perforce client to resolve a condition the application created. This rule is enforced at the type level by the `ErrorState` and `ActionId` enums in `src/core/workflow.hpp` (see spec §8.4): every `ErrorState` value must have at least one `RecoveryFlow` returned by a shipped provider (or be listed in `kUnshippedErrorStates` for states belonging only to not-yet-shipped providers) and a corresponding renderer in the recovery UI; every `ActionId` must have an entry in the central dispatch table. Three CI-enforced checks expressed the rule as code, each landing with its dependencies: `RecoveryFlow` coverage at M1b, renderer coverage at M3–M4, and compile-time `ActionId` dispatch exhaustiveness at M4. Adding a new state or action therefore requires the corresponding UI and dispatch changes (or an explicit `kUnshippedErrorStates` entry) in the same commit. This is a hard quality bar that applies to every provider implementation.

7. **The source control UI is provider-aware and guides users toward correct workflows.** The `SourceControlProvider` interface exposes workflow metadata beyond operations and capability flags (see spec §8.4) — `get_workflow_metadata()` returns a `WorkflowMetadata` struct with a branching model description and a set of `RecoveryFlow`s keyed by `ErrorState`; `suggested_primary_branch_name()` provides the provider's default. The UI renders provider-appropriate guidance; users who have never used the underlying VCS tool directly must be able to use Philotechnia successfully. UI components in the source control panel may conditionally load based on the active provider's workflow metadata. UI widgets emit `ActionId` values to the central dispatcher (`src/core/workflow.cpp`) and never `switch` on `ActionId` themselves — this preserves rule 1 by keeping UI code free of knowledge about concrete provider methods.

8. **Schema invariants are enforced at commit time, not discovered at pull time.** The `SchemaValidator` runs in the commit pipeline and must reject any commit that would produce schema, record, or referential inconsistencies. See spec §6.10 for the four validation categories (schema internal consistency, schema change compatibility, record integrity, referential integrity). This is defense in depth — on-pull handling exists as a backstop for invalid state arriving from outside the application, but the application itself must never generate such a commit. The check logic and its tests land in M2; the UI surfacing lands in M4.

9. **Commands mutate only in-memory state; disk I/O happens exclusively in the commit pipeline.** `Command::execute()` and `Command::undo()` update `RecordWorkingCopy`, schema working state, or `CommandStack` bookkeeping — never the project files on disk. The only code path that writes record, schema, or project-open cache files is the commit pipeline (spec §7.2; cache write is described in spec §8.6). This makes `Command::undo()` infallible by construction: no I/O means no recoverable failure modes, and `CommandGroup::undo()` can iterate its children in reverse without partial-failure handling. Any command that appears to need disk I/O during execute or undo is a design error — redesign it to stage the change in a `RecordWorkingCopy` or an equivalent in-memory buffer, with the write deferred to commit. See the `CreateRecord` worked example at the end of spec §7.2 for the canonical command shape (identity-supplied-not-generated, state-via-captured-references, commit-pipeline-owns-writes) that every concrete `Command` in `src/commands/` should follow.

---

## Conventions

Defaults for how code in this codebase is written. Deviate only with a reason stated in the commit or PR description.

### Code style

Formatting is governed by `.clang-format` at the repo root (LLVM base, customised as described below). Run `clang-format -i` before committing. CI runs `clang-format --dry-run --Werror` as a lint gate from M0a onward.

**Column limit: 100.** Indent with 4 spaces; no tabs. Attach opening braces to the preceding statement (1TBS / modern K&R) — never Allman, for functions or control flow. Use west-const: `const T&`, not `T const&`. `#pragma once` is the header guard in every `.hpp` — no traditional `#ifndef` guards.

**File naming.** Headers are `.hpp`, implementation files are `.cpp`, both in `snake_case`. Filename matches the primary type it declares: `SourceControlProvider` → `source_control_provider.hpp`. One primary type per file unless the types are tightly coupled and small (e.g., `WorkflowMetadata` and its helper structs share `workflow.hpp`).

**Identifier naming (STL-style).**

| Kind | Convention | Example |
|---|---|---|
| Classes, structs, enum types, type aliases | `PascalCase` | `SourceControlProvider`, `WorkflowMetadata`, `ErrorState` |
| Functions, methods, free functions | `snake_case` | `supports_staging()`, `get_pending_changes()` |
| Local variables, parameters | `snake_case` | `pending_changes`, `resolved_content` |
| Non-static data members | trailing-underscore `snake_case_` | `workflow_metadata_`, `commit_boundary_index_` |
| `constexpr` / `static const` constants | `kPascalCase` | `kUnshippedErrorStates`, `kMaxRecordsPerType` |
| Scoped enum values | `PascalCase`, no `k` prefix | `ErrorState::CentralizedCommitBehind`, `ActionId::ResolveConflict` |
| Namespaces | `snake_case` | `namespace philotechnia::core` |
| Macros (only where unavoidable) | `ALL_CAPS_WITH_UNDERSCORES` | `PHILOTECHNIA_NO_COPY(Type)` |

Acronyms are treated as words, not all-caps: `HttpClient`, `XmlParser`, `JsonReader` — never `HTTPClient` or `XMLParser`.

**Include ordering.** One include per line, a blank line between groups, alphabetical within each group. `clang-format` `IncludeCategories` enforces the order:

1. Matching header (for a `.cpp` file, the corresponding `.hpp`)
2. C system headers (`<unistd.h>`, `<sys/stat.h>`) — rare in application code
3. C++ standard library headers (`<vector>`, `<expected>`, `<filesystem>`)
4. Third-party headers (`<QtCore/QObject>`, `<yyjson.h>`, `<git2.h>`, `<catch2/catch.hpp>`)
5. Project headers using `""` quotes (`"core/workflow.hpp"`, `"vcs/provider.hpp"`)

**`[[nodiscard]]` on fallible returns.** Every function returning `std::expected<T, Error>` is marked `[[nodiscard]]`. Silently dropping an error is a bug, and the attribute makes that a diagnostic. A clang-tidy check enforcing this across the codebase lands with the M2 lint wiring.

**`using namespace`.** Never in a header. Tolerated in a `.cpp` translation unit only for literal-operator imports (`using namespace std::chrono_literals;`) with scope kept tight. Prefer `using` type aliases (`using Error = core::Error;`) over `typedef`.

**`auto`.** Use where it aids readability: iterator types, lambda captures, return values from factory functions that name the type in their signature (`auto provider = make_git_provider(...)`). Avoid for primitive scalar locals where the spelled type carries intent.

**`noexcept`.** Mark destructors, move constructors, move assignment, and `swap()` as `noexcept`. Do not spray `noexcept` across ordinary functions — the application leans on `std::expected`, not on noexcept-as-contract.

### Error handling

Domain-level operations use `std::expected<T, Error>`. This includes every method on `SourceControlProvider`, the schema engine, record manager, query engine, storage layer, and command execution. The `Error` type is defined in `src/core/error.hpp` — see spec §8.7 for the concrete struct, the `Error::Category` enumerations, and the category-to-UI-behaviour boundaries. Construct errors through the helper functions in `error.hpp` (`Error::io(...)`, `Error::from_libgit2(...)`, `Error::validation(...)`, etc.) rather than raw brace-initialisation; the helpers capture the `underlying` diagnostic consistently.

Exceptions are reserved for unrecoverable failures where the application cannot sensibly continue: `std::bad_alloc`, filesystem catastrophes that invalidate the working copy (e.g., the project directory is deleted out from under the running app), and programming errors caught by `assert` / contract violations. These propagate to a top-level handler in `main.cpp` that logs and terminates cleanly.

The UI layer consumes `std::expected` from the core and renders failures as inline error states — it does not catch core exceptions except at the top-level handler. Qt signals carrying `Error` values are fine; Qt signals do not throw.

libgit2 returns C-style error codes. The `GitProvider` implementation wraps every libgit2 call and translates non-success codes into the project `Error` type, attaching the libgit2 class/code for diagnostic logging.

### Dependency management

All third-party C++ dependencies are managed via vcpkg in manifest mode. The repo ships a `vcpkg.json` at the root listing pinned dependencies (`qtbase`, `libgit2`, `yyjson`, `catch2`) plus a `builtin-baseline` field whose value is a specific vcpkg registry commit SHA — this is the authoritative pin for the entire dependency graph. CI and contributors point CMake at vcpkg's toolchain file (`-DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake`), which installs the manifest on the first configure.

Only the dynamic vcpkg triplets are supported: `x64-windows`, `x64-osx`, `arm64-osx`, `x64-linux`, `arm64-linux`. The `-static` variants (e.g. `x64-windows-static`) are prohibited because they would statically link Qt in violation of rule 3. The Qt-linking assertion in the root `CMakeLists.txt` (see spec §15.1) is the executable enforcement of this — it is a `FATAL_ERROR` at configure time if `Qt6::Core` is not a `SHARED_LIBRARY`.

Do not add `FetchContent`, `ExternalProject`, or vendored `add_subdirectory` dependencies without a documented reason. Version bumps — including `builtin-baseline` SHA bumps — are their own commits so that any resulting behaviour change is bisectable.

### Testing

The testing emphasis is unit + integration, not UI or smoke tests. Unit tests cover individual classes and free functions; integration tests cover the seams between major components (e.g., `CommandStack` driving the storage layer, `GitProvider` against a scratch on-disk repo, schema migration against a fixture project). Both are written in Catch2 and live under `tests/`.

UI testing is deferred. The UI layer lands in M3, and no automated UI tests are written before then. When UI testing is eventually added, the framework is Qt Test (`QtTest`) — it handles the Qt event loop, signal/slot verification, and input synthesis idiomatically. Catch2 is not used for UI tests.

Tests are linked as one or more CTest targets per module and run via `ctest --output-on-failure`. Each test binary is self-contained — no shared test fixtures across binaries — so failures are localised. Integration tests that need a filesystem write to a temporary directory use `std::filesystem::temp_directory_path()` with a unique per-test subdirectory, cleaned up on test completion.

Tests that drive code consuming `SourceControlProvider` use `TestProvider` from `tests/support/` — a fake with programmable state, call recording, and a FIFO per-operation fault-injection queue for error-path coverage. `TestProvider` is compiled into a `test_support` static library and linked only by test binaries; it is explicitly not a shipped provider and is excluded from the rule-6 exhaustiveness test (decision #27). See spec §8.4a for the interface and usage examples.

### Threading and UI handoff

Background work (project-open parallel parse, incremental full-text indexing, background commit pipeline status) runs on `std::async` workers with a thread pool sized to `std::thread::hardware_concurrency()` (capped ~8). Workers never touch any Qt object. Each worker posts completion back to the UI thread via `QMetaObject::invokeMethod(target, ..., Qt::QueuedConnection)`, where `target` is a long-lived `QObject` on the UI thread; the queued handler is the only code that mutates the `QAbstractItemModel` and emits `rowsInserted` / `dataChanged`. Batch at worker-chunk granularity, not per-file, so UI-thread wakeups stay bounded. `QFutureWatcher` and `QtConcurrent` are deliberately not used — keeping the pool inside the application core (rather than Qt) keeps core and storage layers free of any Qt dependency, preserving rule 1's core/UI separation. See spec §8.5 for the full rationale.

---

## Tech stack

| Layer | Technology | Notes |
|-------|-----------|-------|
| Language | C++23 | Default for all implementation |
| JSON | yyjson | Single library for all read/write; C99, fast both directions |
| UI | Qt 6 (LGPL 3.0) | Dynamic linking required; `QTableView` for record grids, `QListView` for undo history rendering |
| Source control v1 | libgit2 | Implements `SourceControlProvider`; no dependency on a git binary |
| Build | CMake + Ninja | Cross-platform; CPack for installers |
| Testing | Catch2 | Unit and integration tests |
| CI | GitHub Actions | Build + test matrix across macOS, Windows, Linux |

---

## Directory structure

The tree below is the target layout for v1. As of M0a, only `src/main.cpp` and `tests/smoke/` exist; the remaining modules materialise across M0b–M3 per spec §15.

```
philotechnia/
  src/
    core/            # Schema engine, record manager, query engine, auth,
                     #   CommandStack, SchemaValidator, Error type
    storage/         # Hash-prefix dir walk, JSON file I/O, atomic writes, mtime cache
    vcs/             # SourceControlProvider interface + all concrete providers
      provider.hpp   # Abstract base class (the interface)
      git/           # GitProvider (libgit2)
      svn/           # SvnProvider (v2 — not yet implemented)
      perforce/      # PerforceProvider (v3 — not yet implemented)
    ui/              # Qt UI layer — widgets, views, panels
    commands/        # Command pattern — all concrete Command subclasses
    main.cpp
  tests/
    core/
    storage/
    vcs/
    smoke/           # M0a pipeline smoke test — stays long-term as a minimal
                     #   end-to-end build-and-run guard that survives even if
                     #   richer test binaries are temporarily disabled.
    support/         # TestProvider and other shared test doubles — compiled
                     #   into a `test_support` static library linked only by
                     #   test binaries. Never linked into the shipping app.
                     #   See spec §8.4a for the TestProvider surface.
  docs/
    philotechnia_spec.md
    decisions.md
  CMakeLists.txt
  CLAUDE.md
  README.md
```

---

## Key interfaces (from spec §7.2 and §8.4)

### SourceControlProvider (abstract base)

```cpp
struct RemoteStatus {
    bool available;
    int  local_only;   // local commits not yet on remote
    int  remote_only;  // remote changes not yet local
};

// Fallible operations return std::expected<T, Error>; see Conventions §Error handling.
// Pure accessors (capability flags, labels, workflow metadata) return plain values.
class SourceControlProvider {
public:
    virtual ~SourceControlProvider() = default;

    // Capability flags and labels — pure accessors
    virtual bool supports_staging() const = 0;
    virtual bool supports_offline_commits() const = 0;
    virtual bool supports_branching() const = 0;
    virtual bool supports_stashing() const = 0;
    virtual std::string commit_action_label()  const = 0;
    virtual std::string sync_action_label()    const = 0;
    virtual std::string receive_action_label() const = 0;

    // Lifecycle
    virtual std::expected<void, Error> initialise(const std::filesystem::path& dir) = 0;
    virtual std::expected<void, Error> clone(const std::string& url,
                                             const std::filesystem::path& dest) = 0;

    // Working state
    virtual std::expected<std::vector<PendingChange>, Error> get_pending_changes() = 0;
    virtual std::expected<void, Error> stage(const std::vector<std::filesystem::path>& files) = 0;
    virtual std::expected<void, Error> commit(const std::string& message, const std::string& author) = 0;

    // Remote sync
    virtual std::expected<void, Error>         push() = 0;  // no-op (success) for centralized providers
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

    // History and conflicts
    virtual std::expected<std::vector<HistoryEntry>, Error> get_history(
        const std::filesystem::path& file, int limit = 100) = 0;
    virtual std::expected<std::vector<Conflict>, Error> get_conflicts() = 0;
    // resolved_content: present = full post-merge UTF-8 file contents;
    // nullopt = accept delete (for ModifiedVsDeleted / DeletedVsModified).
    // One call per file; merge commit produced by the normal commit pipeline.
    virtual std::expected<void, Error> resolve(const std::filesystem::path& file,
                                               std::optional<std::string> resolved_content) = 0;

    // Workflow guidance — pure accessors
    virtual WorkflowMetadata get_workflow_metadata() const = 0;
    virtual std::string      suggested_primary_branch_name() const = 0;
};
```

### CommandStack (in-memory, application core)

```
CommandStack
  done: Command[]              // most recent last
  undone: Command[]
  commit_boundary_index: int   // done[] index of the last commit; undo stops here
  can_undo() → bool            // done.size() > commit_boundary_index
  can_redo() → bool
  undo() → void                // no-op past the commit boundary
  redo() → void
  history() → string[]         // full done[] descriptions; bound to QListView
                               //   entries at or before commit_boundary_index
                               //   are rendered dimmed / marked "committed"
  commit_boundary() → int      // exposes commit_boundary_index for the UI
  mark_clean() → void           // called after successful commit:
                               //   sets commit_boundary_index = done.size()
                               //   clears undone[] (no redo across commits)
  is_clean() → bool
```

### Commit pipeline

On commit: iterate all `RecordWorkingCopy` instances → call `is_dirty()` → invoke `SchemaValidator::validate()` against the proposed post-commit state → if validation returns an error with any `Blocking` issues, surface them in the commit panel and abort; otherwise for each dirty record, write working state to `records/{entity-type-id}/{xx}/{record-uuid}.json` → collect file paths → call `stage()` → call `commit()` → call `mark_clean()`.

---

## Storage format (from spec §7.1)

```
my-project/
  [.git/ | .svn/ | ...]
  manifest.json                 # includes primary_branch field
  schema/
    entity_types/{entity-type-id}.json
    enums/{enum-id}.json
  records/
    {entity-type-id}/
      {xx}/                 # first 2 hex chars of record UUID
        {record-uuid}.json
```

Hash-prefix directories keep any single directory under ~200 files at 50K record scale.

---

## Performance targets (from spec §11)

| Metric | Target |
|--------|--------|
| Cold start | < 500 ms |
| Project open, 10K records (cold) | < 3 seconds |
| Project open (subsequent) | < 1 second (mtime cache) |
| Record list render, 10K records | < 16 ms per frame |
| Full-text search, 100K values | < 500 ms |
| Write throughput | ≥ 500 records/second |

Project open uses a thread pool (`std::thread::hardware_concurrency()`, capped ~8) to parallelise the directory walk and per-file JSON parse. Platform-native async I/O (io_uring / IOCP / kqueue) is a deferred optimisation — use `std::async` first and profile before adding it.

---

## Build commands

```bash
# Configure (Debug)
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake

# Build
cmake --build build

# Test
cd build && ctest --output-on-failure

# Package (Release)
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
cmake --build build
cd build && cpack
```

---

## Open questions (as of spec v0.8)

All major design questions are currently resolved. The spec has no outstanding open questions. Refer to [`docs/philotechnia_spec.md`](docs/philotechnia_spec.md) §14 for the full list (all struck through) and §13 for the corresponding resolved decisions.

---

## Known gaps and deferred decisions

- **File attachments:** `attachment` field type stores a URI reference (URL or local file path) to an externally hosted resource — no binary data in the repo. Known limitation: URIs can become stale if the external resource moves or is deleted. A more integrated strategy (e.g., Git LFS) may be revisited in a future milestone.
- **Hard deletion:** v1 uses soft deletion only (`deleted_at` field; file retained). A "compact project" action for hard deletion is deferred to a future milestone.
- **Schema migration:** Stateless diff-based — the schema files are the migration target and reconciliation runs in-memory at record load time. Missing fields get declared defaults; removed fields go to `_deprecated_fields`. A "purge deprecated fields" action (surfaced as a destructive confirmation dialog) is planned but not yet designed in detail.
- **Field type conversion flow:** v1 blocks field type changes at commit time (SchemaValidator, §6.10). Users needing to change a field's type must delete the field (values go to `_deprecated_fields`) and add a new field with the desired type. A proper type-conversion flow with value mapping and preview is deferred to a future milestone.
- **Rebase-on-pull:** v1 implements merge-on-pull only. A rebase-on-pull option (per project or per user) is deferred.
- **Preflight disk-space check on commit:** The commit pipeline aborts cleanly on write failure and supports retry. A preflight check that estimates batch size against available disk space before starting writes is a deferred UX improvement (candidate for M4).
- **Username rename flow:** v1 treats usernames as stable — `LocalProfile.username` is the sole user identity key across the system and is stamped into `Record.created_by` at record creation time. There is no project-level user registry, so renaming a user is a bulk rewrite of `created_by` across every record the user authored in every project that references the old name. No rename flow ships in v1; if it lands later, it pairs naturally with the "purge deprecated fields" action.
- **Licensing instrument:** Source-available. BSL, FSL, or custom — to be selected before first public release.
- **SVN and Perforce providers:** Interface is defined; implementations are not started.

---

## Where to find more

- **Full specification:** [`docs/philotechnia_spec.md`](docs/philotechnia_spec.md)
- **Resolved decisions quick-reference:** [`docs/decisions.md`](docs/decisions.md)
