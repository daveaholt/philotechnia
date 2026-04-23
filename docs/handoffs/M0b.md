# Handoff — M0b: Storage Foundation

*Derived from `docs/philotechnia_spec.md` §15.2. If this document and the spec disagree, the spec wins.*

---

## Mission

Add the storage primitives (`src/storage/`) and the project lifecycle surface (`src/core/`) that let the application create and open a project directory containing only a `manifest.json`. Persist a machine-local `LocalProfile` (username + email) on first launch under OS app data. No source control is instantiated yet — M1a adds `GitProvider`. Nothing in M0b consumes the profile; shipping its persistence now guarantees the file exists before any feature needs it.

**Predecessor:** M0a — green build matrix, CMake/vcpkg/Catch2/CPack scaffolding. **Successor:** M1a — `SourceControlProvider` interface + `GitProvider`.

---

## Foundation

### Project in one paragraph

Philotechnia is a cross-platform C++ desktop application for structured record management. Projects are version-controlled directories of plain JSON files (one file per record, hash-prefix directories). The application includes a first-party source control UI built on an abstract `SourceControlProvider` interface — Git via libgit2 ships in v1; SVN and Perforce are later milestones. The UI is Qt. JSON is yyjson. All undo/redo and dirty-state tracking flows through a custom `CommandStack`, not Qt's built-in undo framework.

### Non-negotiable architectural rules (from CLAUDE.md)

Rules active in M0b:

- **Rule 3 — Qt must be dynamically linked.** Already enforced in `CMakeLists.txt` at M0a; do not regress.
- **Rule 4 — All record writes use atomic rename.** Temp file + rename-over. POSIX: `rename(2)`. Windows: `SetFileInformationByHandle` with `FileRenameInfoEx` and `FILE_RENAME_FLAG_POSIX_SEMANTICS | FILE_RENAME_FLAG_REPLACE_IF_EXISTS`. Never `MoveFileExW`. Never write directly to the final path. On startup, scan for orphaned `.tmp` files from interrupted writes and log/recover them.
- **Rule 5 — Hand-written ASM only with a profiled bottleneck.** C++23 is the default.
- **Rule 9 — Commands mutate only in-memory state; disk I/O happens exclusively in the commit pipeline.** M0b predates the command / commit pipeline, but the atomic-write primitive you ship here is the *only* one that future milestones may call from the pipeline — do not leak it into UI, core, or command code paths.

Forward-looking rules (1, 2, 6, 7, 8) apply from later milestones. Ensure no M0b code closes off a future path they require.

### Tech stack slice needed in this milestone

| Layer | Technology | Where it lands |
|-------|-----------|----------------|
| Language | C++23, `std::expected<T, Error>` return convention | all `src/` headers |
| JSON | yyjson (fast-parse flags) | `src/storage/json_io.hpp` |
| Build | CMake 3.25+ | module CMakeLists under `src/storage/`, `src/core/` |
| Testing | Catch2 v3 | `tests/storage/`, `tests/core/` |

### Storage format (§7.1, verbatim)

A project is a version-controlled directory with the following structure. The directory layout is VCS-agnostic — any source control provider tracks the same files. Provider-specific internals (e.g., `.git/`, `.svn/`) are managed by the provider and are not part of the Philotechnia format.

```
my-project/
  [.git/ | .svn/ | ...]                  # Provider-managed internals (not part of the format)
  manifest.json                          # Project identity, format version, provider, primary branch
  schema/
    entity_types/
      {entity-type-id}.json              # One file per entity type and its field definitions
    enums/
      {enum-id}.json                     # One file per user-defined enum type
  records/
    {entity-type-id}/
      {xx}/                              # First 2 hex chars of record UUID (hash prefix)
        {record-uuid}.json               # One file per record
```

**Design rationale** (excerpted; full rationale in spec §7.1):

- Every record is its own JSON file, making concurrent edits to different records structurally non-conflicting.
- Hash-prefix directories (first 2 characters of the record UUID) prevent any single directory from holding more than ~200 files at the 50K record scale.
- All filenames — records, entity types, enums — use UUIDs rather than human-readable slugs. This preserves uniformity, removes the need for rename-on-display-name-change handling, avoids slug-collision checks, and sidesteps case-sensitivity quirks across macOS / Windows / Linux filesystems.
- A record's `entity_type_id` is **path-derived, not file-contained.** It is encoded in the directory segment of the record's path and is populated on the in-memory `Record` during hydration from that path — the on-disk JSON does not duplicate it.
- The storage format carries no VCS-specific assumptions.

**Crash safety:** Writes use a write-to-temp-then-rename pattern to prevent partial writes from corrupting existing data before it is staged and committed. On POSIX, `rename(2)` provides atomic replacement. On Windows, `SetFileInformationByHandle` with `FileRenameInfoEx` and `FILE_RENAME_FLAG_POSIX_SEMANTICS | FILE_RENAME_FLAG_REPLACE_IF_EXISTS` provides POSIX-equivalent atomic rename semantics (available since Windows 10 build 14393; the platform minimum of 19041 guarantees availability). Atomicity is at the filesystem level, not across volumes. On startup the application scans for orphaned `.tmp` files from interrupted writes and recovers them.

### Relevant §7.2 entities (verbatim, scoped to M0b)

```
// manifest.json
Project
  id: uuid (string)
  name: string
  created_at: ISO 8601 timestamp
  format_version: integer          // application-level file-format marker; governs
                                   //   open-time compatibility — see "Format version
                                   //   compatibility" below. There is deliberately
                                   //   no schema_version field.
  source_control_provider: string  // "git" | "svn" | "perforce" | ...
  primary_branch: string           // e.g. "main", "trunk"; confirmed by user at project creation

// Local application data (NOT in the project repo — machine-local, persisted in OS app data dir)
LocalProfile
  username: string       // sole identity key across the system; stamped into Record.created_by
                         //   at record creation time; used as VCS commit author name
  email: string          // used as VCS commit author email across all projects
  created_at: ISO 8601 timestamp
```

**Format version compatibility (spec §7.2):**

- If `format_version` on disk equals the running build's supported version, open normally.
- If `format_version` on disk is lower, the running build knows how to upgrade. Records are reconciled in memory at load time; the first commit after opening writes the newer `format_version` back to `manifest.json`.
- If `format_version` on disk is higher than the running build supports, the application refuses to open the project and surfaces a dialog indicating that a newer version of Philotechnia is required. A read-only fallback is deliberately not offered.

### Sample `manifest.json` (§7.3, verbatim)

```json
{
  "id": "3f4e2a9c-7b1d-4d8e-9f2a-0c5b8a6d1234",
  "name": "Engineering Roadmap",
  "created_at": "2026-03-12T14:22:08Z",
  "format_version": 1,
  "source_control_provider": "git",
  "primary_branch": "main"
}
```

---

## Spec cross-reference: §6.5 Project Management (verbatim)

- A *project* is a version-controlled directory on disk with a defined folder structure and JSON files (see §7.1)
- The source control provider is selected per-project at creation time; git is the default
- The directory is self-contained and portable — clone it, move it, push it to any compatible host
- Users can open multiple projects simultaneously in separate windows
- Opening an existing directory auto-detects the provider from its contents (e.g., presence of `.git/`, `.svn/`) silently — no confirmation dialog is shown; the active provider is displayed persistently in the application title bar (see §13.8)

*(Provider auto-detect is the M1a deliverable, not M0b.)*

---

## Relevant resolved decisions (from `docs/decisions.md`)

| # | Decision | Bearing on M0b |
|---|----------|----------------|
| 2 | **Storage format:** One JSON file per record in hash-prefix directories. UUID-based filenames; display-name-to-UUID surfaced in UI. | Shape of every path helper in `src/storage/paths.hpp`. |
| 3 | **JSON library:** yyjson. | Single dependency for all read/write in `src/storage/json_io.hpp`. |
| 14 | **Schema migration strategy:** Stateless diff-based; no `schema_version` field in `manifest.json`; `format_version` retained as the application-level file-format marker. | Manifest has exactly the fields above — no `schema_version`. |
| 19 | **Authentication and identity model:** No application-level passwords. Local profile (username + email) in OS app data set up on first launch; sole source of commit author identity across all projects. | Drives `ensure_local_profile()` + first-launch prompt. |
| 25 | **Format version compatibility:** Equal → open. Lower → upgrade on first commit. Higher → refuse with dialog. | `read_manifest()` must return `Error::from_format_version(...)` when the on-disk version exceeds the running build. |
| 28 | **User identity key:** `LocalProfile.username` is the sole identity key. | Profile shape has exactly `username`, `email`, `created_at` — no user UUID. |

---

## Deliverable detail (§15.2, verbatim)

M0b adds the storage layer under `src/storage/` and the project lifecycle surface under `src/core/` sufficient to create and open a project that contains only `manifest.json` (no records, no VCS). Concrete deliverables:

**`src/storage/paths.hpp` / `.cpp`** — pure helpers:

- `std::filesystem::path record_path(const ProjectRoot&, const EntityTypeId&, const RecordId&)` returning `records/{entity-type-id}/{xx}/{record-uuid}.json`, where `{xx}` is the first two hex characters of the record UUID (lowercase).
- `std::filesystem::path schema_entity_type_path(...)`, `schema_enum_path(...)`, `manifest_path(...)` as companion helpers.
- A `hash_prefix(const RecordId&) -> std::array<char, 2>` primitive used by both the writer and the directory walker.
- Every helper is pure; no I/O. Unit tests verify path shape against the §7.1 layout for a table of representative UUIDs.

**`src/storage/atomic_write.hpp` / `.cpp`** — atomic file replacement, one function per platform behind a single interface:

- `std::expected<void, Error> atomic_write(const std::filesystem::path& target, std::span<const std::byte> bytes)`.
- POSIX implementation: `open(O_WRONLY | O_CREAT | O_EXCL, 0644)` on `{target}.tmp-{random}`, full write, `fsync`, `rename(2)` to target, `fsync` on the parent directory. Errors from any step produce `Error::io(...)`.
- Windows implementation: `CreateFileW` on the temp path, full write, `FlushFileBuffers`, then `SetFileInformationByHandle` with `FileRenameInfoEx` and the flag combination `FILE_RENAME_FLAG_POSIX_SEMANTICS | FILE_RENAME_FLAG_REPLACE_IF_EXISTS` (CLAUDE.md rule 4). `MoveFileExW` is deliberately not used.
- Unit tests cover: happy path; failure between open and rename (temp file cleaned up); rename-over-existing-file (atomic replacement); rename across volumes (expected error — documented constraint). Windows-only tests additionally cover the two scenarios that motivate CLAUDE.md rule 4's `FileRenameInfoEx` choice: (a) rename-over-open-target with `FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE` on the concurrent handle succeeds — this is the case `FILE_RENAME_FLAG_POSIX_SEMANTICS` exists to enable and the specific reason `MoveFileExW` is rejected; (b) rename-over-open-target without `FILE_SHARE_DELETE` on the concurrent handle fails with a documented error — the residual limitation that POSIX semantics does not remove, surfaced so the commit pipeline's error handling can account for it. These two tests are the verification of rule 4's load-bearing claim and must pass on every Windows CI run.

**`src/storage/tmp_recovery.hpp` / `.cpp`** — orphan-tempfile sweep run by `open_project()`:

- Walks the project tree, identifies `*.tmp-*` files left behind by an interrupted write, logs each, and deletes them. Returns the count recovered as part of the project-open diagnostics.
- Unit test: seed a fixture directory with a few dummy orphan files, call the sweep, assert they are gone and the count is right.

**`src/storage/json_io.hpp` / `.cpp`** — yyjson wrapper for reading and writing record / schema / manifest files:

- `std::expected<yyjson_doc_ptr, Error> read_json(const std::filesystem::path&)` using `yyjson_read_file` with the fast-parse flags Philotechnia trusts; parse failures produce `Error::parse(...)` with the yyjson error code and byte offset.
- `std::expected<void, Error> write_json(const std::filesystem::path&, yyjson_doc* doc)` serialises to a string, calls `atomic_write`, and logs write duration if the file exceeds a threshold (observability hook for the M2 performance targets).
- Unit tests parse and round-trip the §7.3 sample records and the §7.1 `manifest.json` template.

**`src/core/manifest.hpp` / `.cpp`** — typed accessor over `manifest.json`:

- `struct Manifest { ProjectId id; std::string name; int format_version; std::string provider; std::string primary_branch; }`.
- `std::expected<Manifest, Error> read_manifest(const std::filesystem::path& project_root)` — parse, validate required fields present, map to struct.
- `std::expected<void, Error> write_manifest(const std::filesystem::path& project_root, const Manifest&)` — serialise via `json_io`, atomic rename.
- Tests cover: valid manifest, missing required field, wrong `format_version` type, and the §13 format-version compatibility rule (refuse to load a higher `format_version` with `Error::from_format_version(...)`).

**`src/core/project.hpp` / `.cpp`** — project create / open surface:

- `std::expected<Project, Error> create_project(const std::filesystem::path& dir, const ProjectCreateOptions&)` — writes `manifest.json` (with `source_control_provider` set to `"git"`, matching the v1 default), creates the empty `schema/` and `records/`, and returns a `Project` handle. **No provider is instantiated at M0b.** The `Project` struct deliberately has no `SourceControlProvider` field at this milestone; M1a adds `std::unique_ptr<SourceControlProvider> provider` and wires `GitProvider` construction into both entry points, operating on the already-written `"git"` manifest with no migration needed. `TestProvider` (§8.4a) ships in parallel for test-side consumption, but nothing in `src/core/project.cpp` references it.
- `std::expected<Project, Error> open_project(const std::filesystem::path& dir)` — reads `manifest.json`, runs the orphan-tempfile sweep, returns a `Project` handle. Record loading, the mtime cache, and the async directory walk all land in M2.
- Integration tests: create a project in `temp_directory_path() / unique-name`, close, re-open, assert the manifest round-trips and the directory layout matches §7.1. The provider field is absent from `Project` at M0b, so these tests do not touch source control.

**`src/core/local_profile.hpp` / `.cpp`** — machine-local user identity (see §6.7):

- `struct LocalProfile { std::string username; std::string email; std::chrono::system_clock::time_point created_at; }`.
- `std::expected<LocalProfile, Error> read_local_profile()` and `std::expected<void, Error> write_local_profile(const LocalProfile&)` — persistence under the OS app data root (`~/Library/Application Support/Philotechnia/local_profile.json` on macOS, `%APPDATA%/Philotechnia/local_profile.json` on Windows, `$XDG_CONFIG_HOME/philotechnia/local_profile.json` on Linux with the usual fallback to `~/.config`). Written via the same `atomic_write` path used for records so a crashed first-launch setup cannot leave a half-written profile.
- `std::expected<LocalProfile, Error> ensure_local_profile(const FirstLaunchPrompt&)` — reads the existing profile, or drives the first-launch prompt (an injectable interface so the M0b CLI can stub it for tests) and writes the result. Called early in application startup, before any project-open flow.
- Nothing in M0b consumes the profile — `create_project` and `open_project` do not stamp it anywhere yet. M1a is the first milestone that reads it (as the `GitProvider` commit author) and M2 is the first that stamps it into `Record.created_by`. Shipping the persistence surface at M0b keeps M1a's scope to VCS wiring and ensures the profile file exists before any feature needs it.
- Tests: round-trip a profile through read/write; assert the first-launch prompt is skipped when a profile already exists; assert the prompt is invoked exactly once when it does not; assert a partially written profile file is rejected with `Error::parse(...)` rather than silently returning empty strings.

**Exit criteria.** `cmake --build build` green; `ctest --output-on-failure` green, including the storage and manifest integration tests on all three platforms. An operator can run `philotechnia --create-project ./example` from the M0b stub CLI and `philotechnia --open-project ./example` against the resulting directory; both succeed, both produce sensible log output, neither touches the network. First launch of the CLI prompts for the local profile and persists it under the OS app data directory; subsequent launches find the existing profile and do not prompt. VCS is still absent — `M1a` wires it in (and is the first milestone that reads the profile).

---

## What M1a adds next

M1a introduces the `SourceControlProvider` abstract interface (`src/vcs/provider.hpp`), the `GitProvider` implementation via libgit2, and the `Error` type in `src/core/error.hpp` that every `std::expected<T, Error>` in M0b already references. `Project` gains a `std::unique_ptr<SourceControlProvider> provider` field; `create_project` starts producing a git-initialised directory with an "Initial project" commit authored by the `LocalProfile`; `open_project` auto-detects `.git/` and attaches a `GitProvider`. The `Error` category enum (`Io`, `Parse`, `NotFound`, `Validation`, `SourceControl`, `Authentication`, `Network`, `Conflict`, `FormatVersion`, `Cancelled`, `Internal`) lands then — you can reference the category names in M0b error sites even though the helper constructors arrive in M1a.

---

## Source docs

- `docs/philotechnia_spec.md` §6.5, §6.7, §7.1, §7.2 (`Project`, `LocalProfile`, format-version compatibility), §7.3, §15.2
- `docs/decisions.md` rows 2, 3, 14, 19, 25, 28
- `CLAUDE.md` — rules 3, 4, 5, 9
