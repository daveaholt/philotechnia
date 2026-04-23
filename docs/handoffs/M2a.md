# Handoff — M2a: Schema, Records, Commands, Commit Pipeline

*Derived from `docs/philotechnia_spec.md` §15.5. If this document and the spec disagree, the spec wins.*

---

## Mission

Land the application's **data plane** on top of the storage primitives from M0b and the `GitProvider` from M1a: the schema engine, record hydration with stateless diff-based migration, `RecordWorkingCopy` dirty tracking, the custom `CommandStack`, the three canonical concrete `Command`s, the commit pipeline, and `SchemaValidator` with all four pre-commit validation categories. No UI ships — the M0b stub CLI is extended with a handful of subcommands so integration tests (and operators) can drive create/edit/commit round-trips end-to-end. After M2a, a record can be created, edited, validated, committed, closed, and reopened with identity and migration intact, and each `SchemaValidator` category can independently block an invalid commit.

**Predecessor:** M1b — conflict interception, full `WorkflowMetadata` surface, rule-6 exhaustiveness constituent (1). **Successor:** M2b — query, full-text search, in-memory index, mtime-based project-open cache, parallel async file loading.

---

## Foundation

### Project in one paragraph

Philotechnia is a cross-platform C++ desktop application for structured record management. Projects are version-controlled directories of plain JSON files (one file per record, hash-prefix directories). The application includes a first-party source control UI built on an abstract `SourceControlProvider` interface — Git via libgit2 ships in v1; SVN and Perforce are later milestones. The UI is Qt. JSON is yyjson. All undo/redo and dirty-state tracking flows through a custom `CommandStack`, not Qt's built-in undo framework.

### Non-negotiable architectural rules (from CLAUDE.md)

Rules active in M2a:

2. **`CommandStack` owns all undo/redo logic. Qt does not.** `QUndoStack` is not used. `CommandStack` lands this milestone in `src/core/command_stack.hpp` with `done`, `undone`, and `commit_boundary_index_` tracked directly. `history()` returns `std::vector<std::string>` and is the contract M3's `QListView` binds against — Qt does not own the stack, it only renders it.
4. **All record writes use atomic rename.** The commit pipeline is the only code path that calls `atomic_write` from M0b. Every dirty `RecordWorkingCopy` is serialised and written to its hash-prefix path via the M0b primitive — never via a direct `std::ofstream` open on the final path.
8. **Schema invariants are enforced at commit time, not discovered at pull time.** `SchemaValidator` lands here with all four validation categories; the commit pipeline invokes it against the proposed post-commit state and aborts on any `Blocking` issue before calling `stage()` / `commit()`. UI surfacing of the issue list is deferred to M4 (decision #15) — but the check logic itself is non-negotiable from this milestone forward.
9. **Commands mutate only in-memory state; disk I/O happens exclusively in the commit pipeline.** Every concrete `Command` in `src/commands/` follows the `CreateRecord` worked example at the end of §7.2 — identity supplied, state captured by reference at construction, in-memory mutation only. The commit pipeline in `src/core/commit_pipeline.cpp` is the sole code path that writes record or schema files.

Rules 1, 3, 5, 6, 7 are background: rule 3 (Qt dynamic) was enforced at M0a; rule 5 (no ASM without a profiled bottleneck) is passively observed; rule 6 gains its first CI check at M1b and its second at M3–M4 — M2a neither adds a check nor introduces new provider surface that would need one; rule 1 (UI independence from concrete providers) is not active because no UI work happens in M2a; rule 7 (provider-aware UI guidance) likewise lands with the UI at M3–M4.

### Tech stack slice

| Layer | Technology | Where it lands |
|-------|-----------|----------------|
| Language | C++23 | All new modules |
| JSON I/O | yyjson via `json_io::read_json` / `write_json` from M0b | `schema.cpp`, `record.cpp` |
| Atomic write | `atomic_write` from M0b | `commit_pipeline.cpp` only |
| Source control | `GitProvider` + `SourceControlProvider` from M1a | consumed abstractly by `commit_pipeline.cpp` |
| Testing | Catch2 | `tests/core/`, `tests/commands/`, `tests/storage/` |
| CLI | The `src/main.cpp` stub from M0b, extended with parse branches | `main.cpp` |

### Storage format (§7.1 summary)

```
my-project/
  [.git/ | .svn/ | ...]
  manifest.json
  schema/
    entity_types/{entity-type-id}.json
    enums/{enum-id}.json
  records/
    {entity-type-id}/
      {xx}/                               # first 2 hex chars of record UUID
        {record-uuid}.json
```

A record's `entity_type_id` is **path-derived, not file-contained** — populated on the in-memory `Record` during hydration from the `records/{entity-type-id}/…` directory segment. On load, `read_record` validates that the JSON `id` matches the `{record-uuid}` segment of the filename; mismatch is `Error::parse(...)` with the offending path, never silently accepted.

### Error type (§8.7 summary)

```cpp
struct Error {
    enum class Category {
        Io, NotFound, Parse, Validation, SourceControl,
        Authentication, Network, Conflict, FormatVersion,
        Cancelled, Internal,
    };
    Category    category;
    std::string message;
    struct ProviderError { std::string source; int code; std::string detail; };
    std::optional<ProviderError> underlying;
};
```

M2a primarily produces `Io`, `NotFound`, `Parse`, `Validation`, and `Internal` values. `Validation` is produced by the commit pipeline when `SchemaValidator` reports any `Blocking` issue; the `message` carries the first blocking issue, the full list is retrievable via `commit_pipeline.get_last_validation_issues()` for M4's commit panel. `Parse` from record hydration cites path and byte offset. Use the helper constructors (`Error::io`, `Error::parse`, `Error::validation`, …) rather than brace-initialisation — they capture `underlying` consistently.

---

## Spec cross-references

### §6.1 Schema Builder (verbatim)

- Users define *entity types*, each with a name and a set of typed fields
- Users also define *enum types* — named, reusable sets of labeled values (e.g., a "Status" enum with values Open, In Progress, Closed) that can be referenced by fields across any entity type
- Supported field types: text, integer, float, boolean, date/datetime, enum (references a user-defined enum type), reference (link to another entity type), and file attachment
- The `attachment` field type stores a **URI** — a URL to an externally hosted resource (e.g., S3, Azure Blob, any HTTPS-accessible location) or a local file path. No binary data is stored inside the project directory; the project repo contains only the URI string.
- Enum values carry stable IDs, so labels can be renamed without corrupting existing records
- Schema changes are versioned; migrations are applied automatically on open
- Entity type and enum detail views display the item's UUID alongside its display name, with a one-click copy action.

### §6.2 Record Management (verbatim)

- Create, read, update, and delete records for any entity type
- Inline editing directly in list and grid views
- Bulk import from CSV; bulk export to CSV and JSON
- Full per-record change history is provided by the source control provider's history log — no separate history mechanism is required

(CSV import/export and inline UI editing are M3+ concerns. M2a ships only the programmatic CRUD path that drives the CLI and tests.)

### §6.8 Change Tracking & Undo (verbatim)

- Every user action that mutates state (editing a field, adding a record, renaming a schema element, etc.) is encapsulated as a **Command** with execute and undo operations — nothing in the UI touches state directly. The custom `CommandStack` (§7.2) is the implementation; Qt is used to render the history list as a widget, not to manage the stack itself
- **Commands mutate only in-memory state.** `execute()` and `undo()` update `RecordWorkingCopy` instances, schema working state, and `CommandStack` bookkeeping — they never touch the project files on disk. Disk writes happen exclusively in the commit pipeline (§7.2). This makes `Command::undo()` infallible by construction (no I/O means no recoverable failure modes) and allows `CommandGroup::undo()` to iterate its children in reverse without partial-failure handling. A command that appears to need disk I/O during execute or undo is a design error — it should be redesigned to stage the change in a `RecordWorkingCopy` or an equivalent in-memory buffer, with the write deferred to commit
- A per-session **undo/redo stack** lets users step backward and forward through their changes; each command carries a short human-readable description (e.g., "Changed 'Status' from Open to Closed") so the stack is navigable as a history list, not just a blind undo button
- **Dirty state tracking** per record: every record knows its baseline (last committed state) and its current working state; modified records and fields are visually indicated in the UI
- The undo stack is **session-scoped**. Committing is a checkpoint: after a successful commit the `CommandStack` records a boundary at the current position of the done stack. Subsequent undo operations can step back through post-commit commands but cannot cross into pre-commit history. The redo stack is cleared at the commit boundary — commands undone before a commit cannot be redone after it.
- Bulk operations (e.g., CSV import, schema migrations) are wrapped in a **command group** so they appear as a single undoable unit

### §6.10 Pre-commit Validation (verbatim)

Philotechnia enforces schema and record invariants at commit time rather than discovering violations at pull time. A `SchemaValidator` runs in the commit pipeline (see §7.2) after dirty-record gathering and before the source control provider's `stage()` call. If validation fails, the commit panel surfaces the issues inline and the commit action is disabled until every blocking issue is resolved. This is a non-negotiable architectural guarantee: the application never produces a commit that would leave the project in an internally inconsistent state.

The validator runs four categories of checks:

- **Schema internal consistency.** Structural checks on the schema itself, independent of any record state. No two fields share a name within the same entity type; enum references point to enums that exist; reference fields point to entity types that exist; enum values within an enum have unique IDs; `required` flags are compatible with the declared field type; default values are well-formed for the declared field type.

- **Schema change compatibility.** Comparisons between the proposed new schema and the baseline (last committed schema), gated against existing record state. Adding a required field without a default, when records exist, blocks the commit. Changing a field's declared type blocks the commit — type conversions are an explicit destructive action that must go through a dedicated migration flow, not a schema edit. Removing an enum value that records still reference blocks the commit. Removing an entity type that records of other types reference (via `reference` fields) blocks the commit. Removing a field archives its values to `_deprecated_fields` on each record (non-destructive by default); a separate "purge deprecated fields" action (surfaced as a destructive confirmation dialog) is required for hard removal.

- **Record integrity.** Each dirty record is validated against the proposed schema: required fields must have values; field values must match their declared type; enum-typed fields must reference enum values that currently exist; date/datetime fields must parse; references must point to records that exist and aren't soft-deleted.

- **Referential integrity.** Any record with a `reference` field whose target has been soft-deleted or hard-deleted is surfaced. Soft-deleted targets are a warning (commit proceeds); hard-deleted targets (which v1 does not expose through the UI but can arise from external repo edits) block the commit.

Failing checks produce a `ValidationError` containing a list of `ValidationIssue` entries. Each issue carries a category, a user-facing message, the UUIDs of affected schema elements or records, and — where possible — a suggested fix. The UI renders these in the commit panel; the user resolves them through the normal schema editor, record editor, or a bulk-fill flow, then re-triggers validation.

**Defense in depth.** Pre-commit validation is the primary guard against invalid schema states. Secondary handling exists on pull because invalid state can still arrive from sources outside the application — e.g., a team member who edited JSON files with a text editor, an older Philotechnia version that predates this validation, or external tooling. When a pull brings in invalid state, the application flags affected records in the UI with a banner and offers the same resolution flows, but does not block editing or further commits on other records.

### §7.2 — `Command`, `CommandStack`, `RecordWorkingCopy`, `SchemaValidator`, commit pipeline (verbatim, abridged to what M2a lands)

```
Command (abstract)
  description: string
  execute() → void    // in-memory only (rule 9)
  undo()    → void    // infallible by construction

CommandGroup extends Command
  commands: Command[]

CommandStack
  done:                  Command[]
  undone:                Command[]
  commit_boundary_index: int
  can_undo()          → bool     // done.size() > commit_boundary_index
  can_redo()          → bool
  undo()              → void     // no-op past the boundary
  redo()              → void
  history()           → string[] // entries at or before the boundary are rendered dimmed
  commit_boundary()   → int
  mark_clean()        → void     // commit_boundary_index = done.size(); undone.clear()
  is_clean()          → bool

RecordWorkingCopy
  record_id: uuid
  baseline:  Record (nullable)       // nullopt for newly created records
  working:   Record
  is_dirty()      → bool             // !baseline || *baseline != working
  dirty_fields()  → field_id[]
  diff()          → FieldDiff[]

SchemaValidator
  validate(schema: Schema,
           records: RecordWorkingCopy[]) → std::expected<void, ValidationError>

ValidationIssue
  category:      { SchemaIntegrity | SchemaChangeCompat | RecordIntegrity | ReferentialIntegrity }
  severity:      { Blocking | Warning }
  message:       string
  affected_ids:  string[]
  suggested_fix: string (nullable)
```

**Commit pipeline (verbatim).** When the user initiates a commit, the application core iterates over all `RecordWorkingCopy` instances in the current session and calls `is_dirty()` on each. It then invokes `SchemaValidator::validate()` against the proposed post-commit state (schema as it would be after pending changes, combined with the working state of every dirty record). If validation returns an error, the commit panel displays each `ValidationIssue` and the commit action is disabled until every `Blocking` issue is resolved; `Warning` issues are shown but do not block the commit. Once validation passes, for every dirty record the pipeline resolves the on-disk path using the hash-prefix structure (`records/{entity-type-id}/{xx}/{record-uuid}.json`) and writes the current working state to that file. The resulting set of file paths is then passed to `SourceControlProvider::stage()` (a no-op for providers that do not support explicit staging). After a successful `commit()`, `CommandStack::mark_clean()` is called.

**Commit pipeline failure handling (verbatim summary).** The pipeline is not batch-atomic at the filesystem level — the commit-atomicity boundary is the VCS commit. On a mid-write failure the pipeline aborts before calling `stage()` / `commit()`; in-memory state remains dirty; retry completes the batch (atomic rename makes re-writes idempotent). On `stage()` / `commit()` failure after successful writes, files are valid on disk but `mark_clean()` is not called. On crash mid-batch, baselines re-read from disk on restart; successfully written records surface as uncommitted changes to the VCS via `get_pending_changes()`. `mark_clean()` is called only after `SourceControlProvider::commit()` returns success.

**Schema migration (verbatim summary).** Stateless diff-based — not step-versioned. Runs **in-memory, at record load time**, never as a disk-write pass triggered by project open or pull. Fields present in the schema but missing on the record receive their declared default in `working`; fields present on the record but absent from the schema are moved into `_deprecated_fields` in `working`. `baseline` continues to reflect exactly what is on disk. If the record is subsequently edited and committed, migration rides along in the normal commit pipeline.

**Format version compatibility (verbatim summary).** `format_version` equal to the running build: open normally. Lower: upgrade on first commit after opening. Higher: refuse to open with "newer version required" dialog; no read-only fallback.

**`CreateRecord` worked example (verbatim, from §7.2).**

```cpp
// src/core/command.hpp
class Command {
public:
    virtual ~Command() = default;
    virtual std::string_view description() const = 0;
    virtual void execute() = 0;   // in-memory only; see §6.8 and CLAUDE.md rule 9
    virtual void undo()    = 0;   // infallible by construction (no I/O)
};
```

```cpp
// src/commands/create_record.hpp
class CreateRecord final : public Command {
public:
    CreateRecord(ProjectState& state,
                 EntityTypeId entity_type,
                 RecordId record_id,
                 std::string created_by,
                 std::unordered_map<FieldId, Value> initial_values);

    std::string_view description() const override { return description_; }

    void execute() override {
        Record working{
            .id                 = record_id_,
            .entity_type_id     = entity_type_,
            .created_at         = {},               // filled by commit pipeline
            .created_by         = created_by_,
            .updated_at         = {},               // filled by commit pipeline
            .updated_by         = created_by_,
            .deleted_at         = std::nullopt,
            .fields             = initial_values_,
            ._deprecated_fields = {},
        };
        state_.records.insert_or_assign(
            record_id_,
            RecordWorkingCopy{
                .record_id = record_id_,
                .baseline  = std::nullopt,
                .working   = std::move(working),
            });
    }

    void undo() override {
        state_.records.erase(record_id_);
    }

private:
    ProjectState& state_;
    EntityTypeId  entity_type_;
    RecordId      record_id_;
    std::string   created_by_;
    std::unordered_map<FieldId, Value> initial_values_;
    std::string   description_;
};
```

Three generalising properties (verbatim): (1) **identity is supplied, not generated** — `record_id` is a ctor argument, not minted inside `execute()`; (2) **all mutable state lives on `ProjectState`** — commands only touch references captured at construction time; (3) **the commit pipeline is the only place writes happen** — removing every `stage()` / `commit()` / `atomic_write()` call site would still leave the application functional in-session.

---

## Relevant resolved decisions (from `docs/decisions.md`)

| # | Decision | Bearing on M2a |
|---|----------|----------------|
| 5 | **Undo/redo architecture** — Custom `CommandStack`, not `QUndoStack`. | `command_stack.{hpp,cpp}` lands as the authoritative undo/redo core in M2a. |
| 13 | **Soft deletion** — `deleted_at` field; file retained. Hard deletion deferred to a future "compact project" action. | `DeleteRecord::execute()` sets `working.deleted_at = now` rather than removing the file; file stays on disk. |
| 14 | **Schema migration strategy** — Stateless diff-based. Missing fields get declared defaults; removed fields → `_deprecated_fields`. No `schema_version` field; schema fingerprint is the cache key. | `read_record` performs migration in-memory against the loaded `Schema`. `schema_fingerprint()` over sorted file contents lives in `schema.cpp`. |
| 15 | **Pre-commit validation** — `SchemaValidator` runs in commit pipeline; four categories; blocks commits with `Blocking` issues. Check logic + tests land in M2; UI surfacing lands in M4. | M2a ships `SchemaValidator` with all four categories + tests. UI surfacing intentionally not built. |
| 24 | **Commit pipeline failure handling** — Commit-atomicity boundary is the VCS commit. Abort before `stage()`/`commit()` on write failure; retry completes the batch. `mark_clean()` only after successful `commit()`. | Drives the structure of `commit_pipeline.cpp` and its fault-injection integration test. |
| 25 | **Format version compatibility** — Equal: open normally. Lower: upgrade on first commit. Higher: refuse with dialog. Distinct from schema versioning. | Open-time check wired in M2a's CLI load path, ahead of any record hydration. |
| 28 | **User identity key** — `LocalProfile.username` is the sole identity key; stamped into `Record.created_by` at creation; supplied as provider commit author. | `CreateRecord` ctor takes a `LocalProfile`; `commit_pipeline` passes `LocalProfile.username` + `.email` as author to `provider.commit()`. |

---

## Deliverable detail (§15.5, verbatim)

M2a adds the application's data plane — schema, records, commands, the commit pipeline, and `SchemaValidator` — layered over the storage primitives from M0b and the `GitProvider` from M1a. No UI; the M0b stub CLI is extended with a handful of subcommands so a test binary or an operator can drive record creation, edits, and commits end-to-end. Exit criterion is that a record round-trips cleanly through the full pipeline (create → edit → validate → commit → reopen → observe) and that each of the four `SchemaValidator` categories blocks an invalid commit.

**`src/core/schema.hpp` / `.cpp`** — schema types and loading:

- `struct FieldDefinition { FieldId id; std::string display_name; FieldType type; bool required; std::optional<Value> default_value; std::optional<EnumId> enum_ref; /* ... */ };` matching the §7.2 in-memory shape (required-not-optional polarity).
- `struct EntityType { EntityTypeId id; std::string display_name; std::vector<FieldDefinition> fields; /* ... */ };`, `struct Enum { EnumId id; std::string display_name; std::vector<EnumValue> values; };`, `struct Schema { std::vector<EntityType> entity_types; std::vector<Enum> enums; };`.
- `std::expected<Schema, Error> load_schema(const std::filesystem::path& project_root)` — walks `schema/entity_types/` and `schema/enums/`, parses each file with `json_io::read_json` from M0b, aggregates into a `Schema`. Parse failures surface as `Error::parse(...)` with path and byte offset.
- `std::string schema_fingerprint(const Schema&)` — deterministic fingerprint over all schema files. Decision #14: no `schema_version` field; the fingerprint is the cache key and the schema-change detector.
- Unit tests: parse a minimal schema fixture; every field type from §5 round-trips; fingerprint stability (same schema → same fingerprint, added field → different fingerprint).

**`src/core/record.hpp` / `.cpp`** — record in-memory representation and JSON I/O:

- `struct Record` with the fields enumerated in §7.2 (`id`, `entity_type_id`, `created_at`, `created_by`, `updated_at`, `updated_by`, `deleted_at`, `fields`, `_deprecated_fields`). `entity_type_id` is populated from the directory path at hydration (§7.1), not from the on-disk JSON.
- `std::expected<Record, Error> read_record(const std::filesystem::path&, const Schema&)` — hydrates a `Record`; validates that the JSON `id` matches the filename `{record-uuid}` segment per §7.1; applies the stateless diff-based migration (decision #14) — missing fields take declared defaults; removed fields move to `_deprecated_fields` with `archived_at` set.
- `std::expected<std::string, Error> serialize_record(const Record&)` — produces canonical JSON (stable key order matching §7.1 template, RFC 3339 UTC timestamps) for atomic write.
- Unit tests: round-trip the §7.3 sample records; file-id / JSON-id mismatch rejected with `Error::validation(...)`; missing-field migration applies default; removed-field migration populates `_deprecated_fields`.

**`src/core/record_manager.hpp` / `.cpp`** — the in-memory record store:

- `class RecordManager` owning loaded records keyed by `(EntityTypeId, RecordId)`.
- `load_records_for(EntityTypeId)` — serial directory walk at M2a (M2b replaces with the parallel walker); parses each `records/{entity-type-id}/**/*.json` via `read_record`.
- Read access: `get(RecordId) -> const Record*`, `list(EntityTypeId) -> std::span<const Record>`.
- Mutation access: `get_or_open_working_copy(RecordId) -> RecordWorkingCopy&` — returns a shared mutable wrapper so multiple commands within a session see the same pending state.
- Integration tests: seed a fixture project on disk, load, assert count + round-trip equality for every record.

**`src/core/record_working_copy.hpp` / `.cpp`** — the dirty-tracking wrapper:

- `class RecordWorkingCopy` wrapping a `Record` plus a dirty flag and the original pre-mutation snapshot (captured at first mutation for `undo()` support).
- `is_dirty() const`, `snapshot() const`, `revert()` (restores the snapshot, clears dirty).
- Only `Command::execute()` / `Command::undo()` mutate the wrapped `Record` (rule 9); the commit pipeline reads from the wrapper but never writes back through it.

**`src/commands/command.hpp`** — the abstract `Command` base:

- `class Command` with pure-virtual `execute() -> std::expected<void, Error>` and `undo() -> void` — `undo()` is infallible by construction because it touches only in-memory state (rule 9). Header comment re-states rule 9 for implementers.
- `description() const -> std::string` — the label rendered by `CommandStack::history()`, bound to `QListView` at M3.

**`src/commands/create_record.hpp` / `.cpp`, `set_field_value.hpp` / `.cpp`, `delete_record.hpp` / `.cpp`** — the three concrete commands called out at the end of §7.2:

- `CreateRecord` — ctor takes `(RecordManager&, const Schema&, EntityTypeId, RecordId, LocalProfile, ChronoTime)`; the `RecordId` is supplied by the caller (identity-supplied-not-generated, §7.2). `execute()` constructs an in-memory `Record` with declared defaults, stamps `created_at` / `created_by` / `updated_at` / `updated_by`, installs a `RecordWorkingCopy` marked dirty. `undo()` removes the working copy.
- `SetFieldValue` — `execute()` writes `working.fields[field_id] = new_value`, updates `updated_at` / `updated_by`, captures the old value. `undo()` restores old value and prior `updated_at` / `updated_by`.
- `DeleteRecord` — `execute()` sets `working.deleted_at = now` (soft delete, decision #13), captures prior value. `undo()` restores.
- Each 40–80 lines per §7.2. Unit tests exercise execute → undo → redo for each.

**`src/core/command_stack.hpp` / `.cpp`** — the custom undo/redo core (CLAUDE.md rule 2, decision #5):

- `class CommandStack` with `done: std::vector<std::unique_ptr<Command>>`, `undone: std::vector<std::unique_ptr<Command>>`, `commit_boundary_index: int` (suffix underscore per the Conventions section).
- `push(std::unique_ptr<Command>)` calls `execute()`; on success appends to `done` and clears `undone`; on failure drops the command without state change.
- `undo()` / `redo()` / `can_undo()` / `can_redo()` per CLAUDE.md "Key interfaces". `undo()` is a no-op past `commit_boundary_index`.
- `history() -> std::vector<std::string>` — descriptions for `QListView` binding (M3 consumes this contract).
- `mark_clean()` — sets `commit_boundary_index = done.size()`, clears `undone`. Called only after `provider.commit()` returns success (decision #24).
- Unit tests: push → undo → redo sequence; commit boundary cleanly divides committed from in-progress; `undone` cleared after commit; redo rejected across the boundary.

**`src/core/schema_validator.hpp` / `.cpp`** — the four-category pre-commit validator (§6.10, decision #15):

- `class SchemaValidator` with `validate(const Schema& proposed, const RecordStore& records) -> std::vector<ValidationIssue>`.
- `struct ValidationIssue { Severity severity /* Blocking | Warning */; std::string message; std::optional<FieldId> field; std::optional<RecordId> record; };`.
- Category 1 — **Schema internal consistency.** Every `FieldDefinition.enum_ref` resolves to a defined `Enum`; field ids unique within an `EntityType`; required fields without a default cannot be added to an `EntityType` that already has records.
- Category 2 — **Schema change compatibility.** Field-type changes blocked (§13 known-gaps — type-conversion flow deferred); removed fields that still have values in at least one record surface a Warning, not a Blocker (the field moves to `_deprecated_fields` on next load per decision #14).
- Category 3 — **Record integrity.** Every record's `fields` map contains only keys present in its `EntityType` schema; required fields have values; `Value` variant tag matches `FieldDefinition.type`.
- Category 4 — **Referential integrity.** Every `reference`-typed field points at an existing, non-soft-deleted record of the declared target `EntityType`.
- Unit tests per category with fixtures that trigger each condition. Property-based test: a valid-by-construction random schema + record set always validates clean.
- M2a ships check logic + tests; commit-panel UI surfacing is M4 (decision #15).

**`src/core/commit_pipeline.hpp` / `.cpp`** — the pipeline (§7.2, CLAUDE.md "Commit pipeline", rule 9):

- `commit(RecordManager&, CommandStack&, const Schema&, SourceControlProvider&, std::string message, LocalProfile, ChronoTime) -> std::expected<void, Error>`.
- Sequence: enumerate dirty `RecordWorkingCopy` instances → `SchemaValidator::validate(proposed_state)` → if any issue is `Blocking`, return `Error::validation(first_blocking.message)` and stop (full list retrievable via `get_last_validation_issues()` for M4's commit panel) → for each dirty record, `serialize_record` then `atomic_write` to `records/{entity-type-id}/{xx}/{record-uuid}.json` → collect file paths → `provider.stage(paths)` → `provider.commit(message, author)` → on success, `command_stack.mark_clean()`.
- Failure handling per decision #24: on any write failure, abort before `provider.stage()`; in-memory state stays dirty; retry succeeds because atomic rename makes re-writes idempotent. Partial on-disk writes after a crash surface through `provider.get_pending_changes()` on reopen.
- Integration tests: happy-path commit (fixture → create record → set field → commit → observe on disk + in `git log` with the `LocalProfile` author); validation-blocked commit (dirty state preserved, zero file writes to `records/`, zero calls to `provider.stage`); mid-write failure via injected `atomic_write` fault (dirty state preserved, tmp files cleaned up, retry succeeds); `CommandStack` boundary advances only on successful commit.

**M0b CLI extensions** — one-off subcommands added at M2a to drive the integration tests and give operators a testing surface before the M3 UI:

- `philotechnia --create-record ./x --type <entity-type-id> --field key=value ...`
- `philotechnia --set-field ./x --record <id> --field key=value`
- `philotechnia --delete-record ./x --record <id>`
- `philotechnia --commit ./x -m "<msg>"`
- `philotechnia --list-records ./x --type <entity-type-id>`

These are deliberately not promoted as a supported user surface; the real UI lands in M3.

**Exit criteria.** `ctest --output-on-failure` green on all three platforms. End-to-end round-trip passes: create a project, seed a minimal schema and a handful of records via the CLI, close, reopen, observe records intact with path-derived `entity_type_id` and any applicable `_deprecated_fields` migration. `SchemaValidator` blocks a commit in each of the four categories (four distinct integration tests). Commit pipeline writes + stages + commits atomically against `GitProvider`; `git log` shows commits authored by the `LocalProfile`. `CommandStack` undo/redo works within a session, and the commit boundary cleanly separates committed from in-progress work. No performance requirement at this milestone — fixtures are ≤1K records per type.

---

## What M2b adds next

M2b replaces M2a's serial directory walker with a parallel `std::async`-based loader capped at `std::thread::hardware_concurrency()` (~8), adds the mtime-based project-open cache (decision #26) keyed by `project.id` under OS app data, builds the in-memory query and full-text index over text fields, and hits the §11 performance targets (cold open 10K records < 3 s; subsequent < 1 s via the cache; full-text search 100K values < 500 ms). The M2a serial load path stays callable as a correctness baseline for the parallel implementation's tests.

---

## Source docs

- `docs/philotechnia_spec.md` §6.1, §6.2, §6.8, §6.10, §7.1, §7.2, §7.3, §8.7, §15.5
- `docs/decisions.md` rows 5, 13, 14, 15, 24, 25, 28
- `CLAUDE.md` — rules 2, 4, 8, 9
