# Philotechnia — Project Specification

**Version:** 0.8 (Draft)
**Date:** 2026-04-22
**Status:** In Progress

---

## 1. Overview

Philotechnia is a high-performance, cross-platform desktop application for structured record management in team environments. The name is derived from the Greek *philotechnia* — "love of craft" — reflecting the project's commitment to quality, precision, and longevity at the software level.

Rather than layering abstraction on top of abstraction, Philotechnia is built as close to the metal as possible, targeting C++ as the primary implementation language throughout. All data is stored as individual human-readable JSON files in a version-controlled directory, making every project natively diffable at the record level and shareable through any supported source control host. Source control integration is a first-party design concern, not an afterthought — the application includes a full source control UI built on an abstract `SourceControlProvider` interface, making the underlying VCS implementation replaceable. Git (via libgit2) ships as the v1 provider. The result is a tool that is fast, predictable, and built to outlast the frameworks of the moment.

---

## 2. Problem Statement

Modern knowledge management tools are either too shallow (simple note-taking apps) or too heavy (SaaS platforms with opaque data formats, network dependencies, and framework bloat). Teams that need to manage structured, relational records — think internal databases, operational playbooks, entity registries, or domain projects — are forced to choose between:

- Spreadsheets (powerful but unstructured and error-prone at scale)
- SaaS databases (hosted, format-locked, and dependent on vendor uptime)
- General-purpose databases (no GUI, requires engineering to operate)

Philotechnia fills the gap: a native, offline-first desktop application that gives teams a structured, schema-driven record system they fully own and control, backed by version control for collaboration, history, and auditability.

---

## 3. Goals

- **Structured records first.** Users define schemas (entity types with typed fields) and create, edit, and query records against those schemas.
- **Source-control-native collaboration.** Every project is a version-controlled directory. Collaboration, history, and change tracking are provided by the configured source control provider — not by a custom sync server or proprietary protocol. Teams work through standard VCS workflows (commit, push, pull, branch, merge) via a first-party UI built into the application. The source control layer is designed as an abstract provider interface so that multiple VCS backends can be supported without changes to the UI or application core.
- **Offline-first (distributed providers).** When using a distributed provider such as git, the application is fully functional without a network connection — only pushing and pulling require connectivity. Centralized providers (e.g., SVN, Perforce) require network connectivity for commits; this is a known architectural constraint of those systems and is communicated clearly in the UI.
- **Cross-platform.** Must run natively on macOS, Windows, and Linux with consistent behavior across all three.
- **Human-readable, record-level storage.** Every record is its own JSON file, organized in hash-prefix directories. The format is inspectable without the application, diffable at the individual record level, and processable by any standard JSON tooling. The storage format is VCS-agnostic.
- **Responsive performance.** Startup time, query response, and rendering should feel fast for typical team-sized data sets (tens of thousands of records). The in-memory index mitigates query cost after the initial directory walk on open.
- **Data ownership.** All data is stored locally in open, documented file formats under a standard version-controlled directory. No vendor lock-in. The project can be hosted on any server the team controls.

## 4. Non-Goals (v1)

- Real-time collaborative editing (the source control provider handles asynchronous collaboration; simultaneous editing of the same record requires a commit and merge)
- A hosted/cloud-only deployment model
- A general-purpose document editor or rich text authoring tool
- Mobile clients
- Plugin or scripting system (considered for v2)

---

## 5. Target Users

**Primary:** Teams and organizations that manage operational knowledge — operations teams, research groups, internal tooling departments, and technical organizations that need a structured, queryable, team-wide record system without depending on SaaS infrastructure.

**Secondary:** Power users and developers who want a personal structured project they fully own, version-control, and can extend via the file format.

**Identity model:** Each installation of Philotechnia maintains a **local profile** (username + email) stored in local application data — not in any project repository. This profile is created on first launch via a one-time setup prompt. It is the sole source of commit author identity across all projects on that machine. Philotechnia has no project-level user registry, no role system, and no application-layer access control; every team member who can clone the repository is treated identically by the application. Access control, where required, is enforced at the VCS hosting level (branch protection rules, repository permissions on GitHub/GitLab/Gitea, etc.) — not by Philotechnia.

---

## 6. Core Features

### 6.1 Schema Builder
- Users define *entity types*, each with a name and a set of typed fields
- Users also define *enum types* — named, reusable sets of labeled values (e.g., a "Status" enum with values Open, In Progress, Closed) that can be referenced by fields across any entity type
- Supported field types: text, integer, float, boolean, date/datetime, enum (references a user-defined enum type), reference (link to another entity type), and file attachment
- The `attachment` field type stores a **URI** — a URL to an externally hosted resource (e.g., S3, Azure Blob, any HTTPS-accessible location) or a local file path. No binary data is stored inside the project directory; the project repo contains only the URI string. This keeps VCS history clean and repo size manageable regardless of attachment size. **Known limitation:** URIs can become stale if the external resource is moved or deleted — the project is not fully self-contained when attachments are in use. This is an accepted tradeoff for v1. A more integrated strategy (e.g., Git LFS, content-addressed sidecar store) may be evaluated in a future milestone if URI proves insufficient.
- Enum values carry stable IDs, so labels can be renamed without corrupting existing records
- Schema changes are versioned; migrations are applied automatically on open
- Entity type and enum detail views display the item's UUID alongside its display name, with a one-click copy action. Because schema and record filenames are UUID-based (see §7.1), this is the affordance users rely on to locate a specific file in source control — e.g., to search git log or to open the file directly in a text editor

### 6.2 Record Management
- Create, read, update, and delete records for any entity type
- Inline editing directly in list and grid views
- Bulk import from CSV; bulk export to CSV and JSON
- Full per-record change history is provided by the source control provider's history log — no separate history mechanism is required

### 6.3 Query & Filter
- Filter records by any field value, with multi-condition AND/OR logic
- Sort by any field, ascending or descending
- Save named filter presets per entity type
- Full-text search across all text fields within an entity type

### 6.4 Views
- **Table view** — spreadsheet-style grid, configurable column visibility and order
- **Detail view** — single-record pane showing all fields and source control history for that record
- **Reference graph** — visual exploration of relationships between linked records (deferred to v1.1)

### 6.5 Project Management
- A *project* is a version-controlled directory on disk with a defined folder structure and JSON files (see §7.1)
- The source control provider is selected per-project at creation time; git is the default
- The directory is self-contained and portable — clone it, move it, push it to any compatible host
- Users can open multiple projects simultaneously in separate windows
- Opening an existing directory auto-detects the provider from its contents (e.g., presence of `.git/`, `.svn/`) silently — no confirmation dialog is shown; the active provider is displayed persistently in the application title bar (see §13.8)

### 6.6 Collaboration & Sync
The configured source control provider is the collaboration mechanism. There is no custom sync protocol or server.

- **Sharing:** The project directory is pushed to a shared remote. Team members clone or check out the project locally and open it in Philotechnia.
- **Committing:** Changes accumulate in the working state (dirty state tracking, §6.8) until the user explicitly commits via the source control panel. The logged-in Philotechnia user is mapped to the commit author identity. For centralized providers, committing requires network connectivity.
- **Pushing and pulling:** Initiated from the source control panel. Pull fetches and merges remote changes; conflicts are surfaced in the conflict resolution UI (§6.9) rather than as raw file-level conflict markers.
- **Branching:** Where the provider supports it, users can create and switch branches from the source control panel.

### 6.7 User Identity
- Each machine running Philotechnia has a **local profile** (username + email) set up on first launch and stored in local application data (never in any project repository). This is the user's identity for commit authorship across all projects.
- The commit author for each change is drawn from the local profile (name + email), providing a durable audit trail through the source control provider's history without a separate audit log mechanism.
- Philotechnia has **no project-level user registry and no role system.** There is no `users.json`, no Owner/Editor/Viewer distinction, and no application-layer permission enforcement. Every team member who can open the project is treated identically by the application.
- **Onboarding a new team member** is simply: clone the repository, open in Philotechnia, set up the local profile on first launch (if not already set). No Owner action is required and no out-of-band credential exchange is needed. The commit author for their subsequent work is their local profile name and email, visible in the source control history.
- **Access control**, where required, is enforced at the VCS hosting level — branch protection rules, repository permissions, signed commits — not by Philotechnia. This is a deliberate architectural choice: the project is a plain directory of JSON files under version control, and any application-layer role check can be trivially bypassed by editing files outside the app. Pushing the security boundary to the VCS host is the only model that produces a real guarantee.

### 6.8 Change Tracking & Undo
- Every user action that mutates state (editing a field, adding a record, renaming a schema element, etc.) is encapsulated as a **Command** with execute and undo operations — nothing in the UI touches state directly. The custom `CommandStack` (§7.2) is the implementation; Qt is used to render the history list as a widget, not to manage the stack itself
- **Commands mutate only in-memory state.** `execute()` and `undo()` update `RecordWorkingCopy` instances, schema working state, and `CommandStack` bookkeeping — they never touch the project files on disk. Disk writes happen exclusively in the commit pipeline (§7.2). This makes `Command::undo()` infallible by construction (no I/O means no recoverable failure modes) and allows `CommandGroup::undo()` to iterate its children in reverse without partial-failure handling. A command that appears to need disk I/O during execute or undo is a design error — it should be redesigned to stage the change in a `RecordWorkingCopy` or an equivalent in-memory buffer, with the write deferred to commit
- A per-session **undo/redo stack** lets users step backward and forward through their changes; each command carries a short human-readable description (e.g., "Changed 'Status' from Open to Closed") so the stack is navigable as a history list, not just a blind undo button
- **Dirty state tracking** per record: every record knows its baseline (last committed state) and its current working state; modified records and fields are visually indicated in the UI
- The undo stack is **session-scoped**. Committing is a checkpoint: after a successful commit the `CommandStack` records a boundary at the current position of the done stack. Subsequent undo operations can step back through post-commit commands but cannot cross into pre-commit history. Pre-commit command descriptions remain visible in the history list (rendered as dimmed / marked "committed") so the user keeps visual context for earlier session work, but the undo action is disabled at the boundary. The redo stack is cleared at the commit boundary — commands undone before a commit cannot be redone after it. The source control history provides visibility into changes across commits at a coarser (commit-level) granularity
- Bulk operations (e.g., CSV import, schema migrations) are wrapped in a **command group** so they appear as a single undoable unit

### 6.9 Source Control Integration
Source control is surfaced as a first-party UI feature through the abstract `SourceControlProvider` interface. The UI presents Philotechnia's vocabulary — pending changes, history, conflicts — regardless of which provider is active. Action labels (e.g., "Push" vs "Submit") are supplied by the provider via its label methods so the UI layer never needs to know the concrete provider type. Features unsupported by the active provider (e.g., branching, offline commits) are hidden or disabled with a brief explanation.

The source control UI **guides users toward correct workflows** — it does not merely expose VCS operations. Where the active provider supports branching, the application enforces a **branch-first default**: if the user is on the primary branch and makes any change, the app prompts them to create or select a working branch before proceeding. The user can dismiss the prompt and continue on the primary branch, but branch-first is the strongly encouraged default path. Multiple branches can be open simultaneously; the user can switch between them from the source control panel.

**Stashing** is supported for providers where `supports_stashing()` is true. If a user has uncommitted changes on a branch and needs to switch context, they can stash their work, switch branches, and return to it later. The stash list is visible in the source control panel.

**Primary branch** is configured at project creation time — the user confirms the name (git default: "main"; SVN default: "trunk"; provider suggests via `suggested_primary_branch_name()`). The primary branch name is stored in `manifest.json`. For projects opened from an existing repository that predates Philotechnia, the app detects the current default branch and prompts the user to confirm it before the first session begins.

Workflow language and guidance text are provider-appropriate: git's branch model is described differently from SVN's copy-based branches or Perforce's streams. Each provider contributes a `WorkflowMetadata` struct (via `get_workflow_metadata()`) that supplies a branching model description and a set of error recovery flows — so the UI can render provider-aware guidance without knowing the concrete provider type (see §13).

**Hard quality bar:** Philotechnia must be fully self-sufficient for every source control state it can create. If the application can put a repository into a state — conflict, detached HEAD, incomplete merge, lock, authentication challenge — it must be able to guide the user out of that state within the application. No user should ever need to open a terminal or an external VCS client to resolve a condition Philotechnia created.

The source control panel provides:

- **Pending changes list** — all modified, added, and deleted record and schema files since the last commit, described in human-readable terms ("Record 'Acme Corp' modified", "Entity type 'Invoice' added") rather than raw file paths. Pending changes are surfaced independent of the in-memory `CommandStack` state — so on-disk changes from an aborted commit batch, an external JSON edit, or a prior session are always visible and never silently invisible to the user
- **Pre-commit diff panel** — field-by-field breakdown of pending changes per record; ability to revert individual field changes before committing; feeds directly from `CommandStack` dirty state
- **Commit panel** — write a commit message and commit pending changes; Philotechnia user identity is mapped to the provider's author concept; disabled without connectivity for centralized providers; disabled while any `Blocking` pre-commit validation issue is unresolved (see §6.10)
- **Push / pull controls** — labelled using the provider's `sync_action_label()` and `receive_action_label()` methods; remote status indicator drawn from `RemoteStatus`
- **Branch management** — create, switch, and delete branches; hidden for providers where `supports_branching()` returns false
- **History view** — commit log rendered as a timeline of human-readable change summaries; selecting a commit shows a record-level diff
- **Conflict resolution UI** — when a sync produces conflicts, affected records are shown side-by-side (local vs. incoming), field by field, with options to accept either version or manually set a value; raw file-level conflict markers are never exposed to the user. Interception works as follows: the provider performs a programmatic three-way merge (for `GitProvider`, `git_remote_fetch` followed by `git_merge_trees` with manual index handling — never the default checkout path that writes conflict markers to disk) and surfaces each conflict as three byte blobs (ancestor, local, incoming) via `get_conflicts()`. The JSON-aware field-level merge logic lives in the application core, not in the provider — the provider is deliberately unaware of JSON structure so that future SVN/Perforce providers surface the same three-blob shape with no provider-side JSON handling. This is a significant implementation requirement (see §15 M1b)

### 6.10 Pre-commit Validation

Philotechnia enforces schema and record invariants at commit time rather than discovering violations at pull time. A `SchemaValidator` runs in the commit pipeline (see §7.2) after dirty-record gathering and before the source control provider's `stage()` call. If validation fails, the commit panel surfaces the issues inline and the commit action is disabled until every blocking issue is resolved. This is a non-negotiable architectural guarantee: the application never produces a commit that would leave the project in an internally inconsistent state.

The validator runs four categories of checks:

- **Schema internal consistency.** Structural checks on the schema itself, independent of any record state. No two fields share a name within the same entity type; enum references point to enums that exist; reference fields point to entity types that exist; enum values within an enum have unique IDs; `required` flags are compatible with the declared field type; default values are well-formed for the declared field type.

- **Schema change compatibility.** Comparisons between the proposed new schema and the baseline (last committed schema), gated against existing record state. Adding a required field without a default, when records exist, blocks the commit. Changing a field's declared type blocks the commit — type conversions are an explicit destructive action that must go through a dedicated migration flow, not a schema edit. Removing an enum value that records still reference blocks the commit. Removing an entity type that records of other types reference (via `reference` fields) blocks the commit. Removing a field archives its values to `_deprecated_fields` on each record (non-destructive by default); a separate "purge deprecated fields" action (surfaced as a destructive confirmation dialog) is required for hard removal.

- **Record integrity.** Each dirty record is validated against the proposed schema: required fields must have values; field values must match their declared type; enum-typed fields must reference enum values that currently exist; date/datetime fields must parse; references must point to records that exist and aren't soft-deleted.

- **Referential integrity.** Any record with a `reference` field whose target has been soft-deleted or hard-deleted is surfaced. Soft-deleted targets are a warning (commit proceeds); hard-deleted targets (which v1 does not expose through the UI but can arise from external repo edits) block the commit.

Failing checks produce a `ValidationError` containing a list of `ValidationIssue` entries (see §7.2). Each issue carries a category, a user-facing message, the UUIDs of affected schema elements or records, and — where possible — a suggested fix ("Provide a default value for 'due_date', or run Bulk Fill to set values on the 47 existing records that would be affected"). The UI renders these in the commit panel; the user resolves them through the normal schema editor, record editor, or a bulk-fill flow, then re-triggers validation.

**Defense in depth.** Pre-commit validation is the primary guard against invalid schema states. Secondary handling exists on pull because invalid state can still arrive from sources outside the application — e.g., a team member who edited JSON files with a text editor, an older Philotechnia version that predates this validation, or external tooling. When a pull brings in invalid state, the application flags affected records in the UI with a banner and offers the same resolution flows, but does not block editing or further commits on other records. See "Schema migration on pull" in §7.2 for the full pull-side behavior.

---

## 7. Data Model

### 7.1 Storage Format

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

**Design rationale:**
- Every record is its own JSON file, making concurrent edits to different records structurally non-conflicting — two team members editing different records in the same entity type will never produce a merge conflict regardless of the VCS in use
- Hash-prefix directories (first 2 characters of the record UUID) prevent any single directory from holding more than ~200 files at the 50K record scale, keeping filesystem and VCS index performance healthy — the same pattern git uses for its own object store
- Schema files (entity types and enums) are one file each, so schema changes are naturally isolated
- All filenames — records, entity types, enums — use UUIDs rather than human-readable slugs. This preserves uniformity across the repo, removes the need for rename-on-display-name-change handling, avoids slug-collision checks, and sidesteps case-sensitivity quirks across macOS / Windows / Linux filesystems. The tradeoff is that raw git log output and filesystem browsing are less self-documenting; this is mitigated by the application UI surfacing each entity type's and enum's UUID alongside its display name so a user who needs to locate a file in git can look it up directly (see §6.1)
- A record's `entity_type_id` is **path-derived, not file-contained.** It is encoded in the directory segment of the record's path (`records/{entity-type-id}/{xx}/{record-uuid}.json`) and is populated on the in-memory `Record` during hydration from that path — the on-disk JSON does not duplicate it. This keeps record files compact and avoids the drift risk of two sources of truth for the same fact. Hydration is required to validate that the `id` field inside the JSON matches the `{record-uuid}` segment of the filename; a mismatch indicates filesystem corruption (a file moved to the wrong directory, a bad external merge, a manual rename) and is surfaced as an `Error::parse(...)` with the offending path, never silently accepted
- The storage format carries no VCS-specific assumptions — it works identically whether tracked by git, SVN, Perforce, or any other file-based version control system
- A separate `history/` directory is not needed — the source control provider's history log, queryable per file, provides this with greater power and no additional maintenance
- `manifest.json` is deliberately kept as a single file for simplicity; it does not benefit from the per-record conflict isolation of the `records/` structure, but it changes rarely enough that this is a non-issue in practice

**Crash safety:** Writes use a write-to-temp-then-rename pattern to prevent partial writes from corrupting existing data before it is staged and committed. On POSIX, `rename(2)` provides atomic replacement. On Windows, `SetFileInformationByHandle` with `FileRenameInfoEx` and `FILE_RENAME_FLAG_POSIX_SEMANTICS | FILE_RENAME_FLAG_REPLACE_IF_EXISTS` provides POSIX-equivalent atomic rename semantics (available since Windows 10 build 14393; the platform minimum of 19041 guarantees availability). Atomicity is at the filesystem level, not across volumes. On startup the application scans for orphaned `.tmp` files from interrupted writes and recovers them.

### 7.2 Core Entities (Internal)

```
// manifest.json
Project
  id: uuid (string)
  name: string
  created_at: ISO 8601 timestamp
  format_version: integer          // application-level file-format marker; governs
                                   //   open-time compatibility — see "Format version
                                   //   compatibility" below. There is deliberately
                                   //   no schema_version field: the schema files
                                   //   themselves are the authoritative state, and a
                                   //   fingerprint over them is computed when a cache
                                   //   key is needed.
  source_control_provider: string  // "git" | "svn" | "perforce" | ...
  primary_branch: string           // e.g. "main", "trunk"; confirmed by user at project creation

// schema/entity_types/{entity-type-id}.json
EntityType
  id: uuid (string)
  name: string
  fields: FieldDefinition[]

FieldDefinition
  id: uuid (string)
  name: string
  type: string  // "text" | "integer" | "float" | "boolean" | "date" | "datetime"
                //   | "enum" | "reference" | "attachment"
  required: boolean
  default_value: any (nullable)
  enum_id: uuid (only when type == "enum"; references a user-defined EnumType)
  reference_entity_type_id: uuid (only when type == "reference")

// schema/enums/{enum-id}.json
EnumType
  id: uuid (string)
  name: string              // e.g. "Status", "Priority", "Region"
  values: EnumValue[]
  created_at: ISO 8601 timestamp

EnumValue
  id: uuid (string)         // stable ID; display label can be renamed without breaking records
  label: string             // the human-readable display value
  sort_order: integer       // controls display ordering in UI dropdowns

// records/{entity-type-id}/{xx}/{record-uuid}.json
// entity_type_id is NOT stored here — it is encoded in the directory path
// and populated on the in-memory Record during hydration (see §7.1).
Record
  id: uuid (string)
  created_at: ISO 8601 timestamp
  created_by: string (username)   // taken from LocalProfile.username at creation time;
                                  //   a durable attribution of authorship that rides with
                                  //   the record even if the author later changes their
                                  //   local profile username. Any future username rename
                                  //   flow must update created_by in bulk across records.
  updated_at: ISO 8601 timestamp  // set by the commit pipeline on every write (including
                                  //   the insert write). Denormalized onto the record so
                                  //   that "sort by last modified" / "filter changed in
                                  //   the last N days" are O(records-in-memory) rather
                                  //   than requiring a per-file walk of VCS history.
  updated_by: string (username)   // username of the committer on that write; same source
                                  //   as the VCS commit author — LocalProfile.username.
  deleted_at: ISO 8601 timestamp | null
  fields: { [field_id: uuid]: any }
  _deprecated_fields:              // optional key; populated when a field is removed
    { [field_id: uuid]:            //   from the schema while this record still held a
        { value: any,              //   value for it. Values are preserved in case the
          archived_at: ISO 8601    //   schema change is reverted before a hard prune.
          timestamp } }            //   Removed only by the "purge deprecated fields"
                                   //   action (a destructive confirmation dialog;
                                   //   deferred feature). Omitted
                                   //   entirely on records that have never held an
                                   //   archived field.

// Soft deletion note:
// Records are soft-deleted by setting deleted_at to an ISO 8601 timestamp. The file is
// retained on disk. This preserves VCS history continuity — hard deleting a file removes
// its per-file history from providers that track at file granularity. The tradeoff is that
// deleted record files accumulate indefinitely. A future "compact project" action (surfaced
// as a destructive confirmation dialog) can hard-delete the files and commit the removal.
// Until that action exists, Philotechnia filters out deleted records in all UI views and
// queries. Hard deletion (file removal) is not exposed in v1.

// Local application data (NOT in the project repo — machine-local, persisted in OS app data dir)
LocalProfile
  username: string       // sole identity key across the system; stamped into Record.created_by
                         //   at record creation time; used as VCS commit author name
  email: string          // used as VCS commit author email across all projects
  created_at: ISO 8601 timestamp

// In-memory only (not persisted) — command pattern
Command (abstract)
  description: string         // human-readable label for the undo history UI
  execute() → void            // contract: mutates only in-memory state
                              //   (RecordWorkingCopy, schema working state,
                              //   CommandStack bookkeeping). MUST NOT perform
                              //   disk I/O. See §6.8.
  undo() → void               // contract: reverses execute() using only
                              //   in-memory state. Infallible by construction:
                              //   no I/O means no recoverable failure modes.

CommandGroup extends Command  // wraps multiple commands as one undoable unit
  commands: Command[]         // undo() iterates in reverse with no
                              //   partial-failure handling — relies on the
                              //   in-memory-only contract above

CommandStack                  // one per open project, lives in application core
  done: Command[]             // executed commands, most recent last
  undone: Command[]           // commands available to redo
  commit_boundary_index: int  // position in done[] where the last commit occurred;
                              //   undo cannot step past this point. Initial value 0.
  can_undo() → bool           // true iff done.size() > commit_boundary_index
  can_redo() → bool           // true iff undone is non-empty
  undo() → void               // no-op when can_undo() is false
  redo() → void               // no-op when can_redo() is false
  history() → string[]        // descriptions of the full done stack (including
                              //   entries at or before commit_boundary_index);
                              //   rendered by a Qt list widget, with entries at
                              //   or before the boundary visually distinguished
  commit_boundary() → int     // exposes commit_boundary_index so the UI can
                              //   render a separator / "committed" styling
  mark_clean() → void         // called after a successful commit:
                              //   sets commit_boundary_index = done.size()
                              //   and clears undone[] (no redo across commits)
  is_clean() → bool           // true when no uncommitted changes exist

// In-memory schema types — the parsed counterparts of the files under
// schema/entity_types/ and schema/enums/. This is the minimum surface
// ProjectState, Record, RecordWorkingCopy, and the §6.10 validator call
// sites need to resolve to real types; the full shape lands in M2.

Schema
  entity_types: { [entity_type_id: uuid] : EntityType }
  enums:        { [enum_id: uuid]        : EnumDefinition }
  // schema_fingerprint (string) is computed over sorted file contents at
  // load time and cached here for §8.6's project-open cache invalidation.
  // An inbound-reference map { target_entity_type_id → [(source_entity_type_id, field_id)] }
  // is also derived at load time and cached for §6.10's referential-integrity check.

EntityType
  id:     uuid
  name:   string
  fields: FieldDefinition[]                        // ordered; drives UI column order

FieldDefinition                                    // matches the on-disk FieldDefinition name in §7.2
  id:               uuid
  name:             string
  type:             enum { Text, Integer, Float, Boolean, Date, DateTime, Enum, Reference, Attachment }
  required:         bool                           // same polarity as on-disk FieldDefinition.required; default false = field is optional
  default_value:    Value (nullable)               // see Value below; must match type. Combines with required: a required field with a default auto-fills when the user omits a value
  enum_id:          uuid (nullable)                // must be set iff type == Enum
  reference_target: entity_type_id (nullable)      // must be set iff type == Reference

EnumDefinition
  id:     uuid
  name:   string
  values: EnumValue[]                              // insertion-order stable
  // §6.2 governs value addition / rename / hide semantics. Hidden values
  // remain referenceable by existing records but are not selectable for
  // new input — enforcement is in the UI layer, not the schema itself.

EnumValue
  id:           uuid                               // stable across renames; stored in Record field values
  display_name: string
  hidden:       bool

// In-memory only — the full record shape parsed from a file under
// records/{entity-type-id}/{xx}/{uuid}.json. System fields are set by the
// commit pipeline (created_* on insert; updated_* on every write).
// _deprecated_fields is the migration carry-over described below §6.10 —
// fields present on disk but no longer in the current schema.
Record
  id:                 uuid
  entity_type_id:     uuid                         // populated from directory path at load time (see §7.1); NOT in on-disk JSON
  created_at:         datetime
  created_by:         username                     // inline per §7.2 / decision #28; taken from LocalProfile.username at creation
  updated_at:         datetime                     // set by commit pipeline on every write; mirrors on-disk updated_at
  updated_by:         username
  deleted_at:         datetime (nullable)          // soft delete; see §13 known gaps
  fields:             { [field_id: uuid] : Value } // user-defined field contents; matches on-disk "fields" key
  _deprecated_fields:                              // fields on record but not in current schema — same shape as on disk
    { [field_id: uuid] : { value: Value, archived_at: datetime } }

// In-memory only — dirty state tracking per record. Wraps a Record plus
// an optional pre-edit baseline: baseline is nullopt for a record created
// in-session (nothing on disk to revert to), and Some(Record) once the
// commit pipeline has written the record at least once. Commands mutate
// `working` per rule 9. is_dirty() is structural inequality — any
// diverging field, including system fields like deleted_at, counts.
RecordWorkingCopy
  record_id: uuid                                  // mirrors working.id for fast lookup
  baseline:  Record (nullable)                     // state at last commit; nullopt for newly created records
  working:   Record                                // current in-session state
  is_dirty() → bool                                // !baseline.has_value() || *baseline != working
  dirty_fields() → field_id[]                      // field_ids where baseline.fields differs from working.fields
  diff() → FieldDiff[]                             // feeds the pre-commit diff panel

FieldDiff
  field_id: uuid
  old_value: Value  // in-memory; null when the field was absent in baseline
  new_value: Value  // in-memory; null when the field was removed in working

// In-memory only — the aggregate in-session project state that Commands
// mutate. One instance per open project, owned by the application core
// (src/core/project_state.hpp). Commands hold a reference captured at
// construction time (see the CreateRecord worked example below); the
// commit pipeline iterates state.records to discover dirty working copies.
// The exact field set is expected to grow as M2 lands — this is the
// minimum surface the worked example and §6.10 validator need.
ProjectState
  records: { [record_id: uuid] : RecordWorkingCopy }
  schema:  Schema  // in-memory representation of the schema/ tree
                   //   (declared above); drives SchemaValidator and
                   //   record hydration. Record lookups that need
                   //   entity_type_id / created_by go through
                   //   records[id].working — no redundant index maps.

// Value — the tagged union over the field types declared in §6.1
// (text / integer / float / boolean / date / datetime / enum / reference /
// attachment). Referenced by RecordWorkingCopy.baseline / .working and by
// Command constructors (e.g., CreateRecord.initial_values). The expected
// C++ shape is std::variant<std::string, int64_t, double, bool,
// std::chrono::year_month_day, std::chrono::system_clock::time_point,
// EnumValueId, RecordId, Uri>; the concrete definition lands with the
// in-memory schema types in M2. "any" elsewhere in §7.2 denotes the
// on-disk JSON value; Value is its in-memory counterpart.

// In-memory only — pre-commit validation (see §6.10)
SchemaValidator
  validate(schema: Schema, records: RecordWorkingCopy[]) → std::expected<void, ValidationError>
  // Runs all four validation categories: schema internal consistency, schema
  // change compatibility, record integrity, referential integrity.

ValidationError
  issues: ValidationIssue[]

ValidationIssue
  category: enum {
      SchemaIntegrity,        // structural schema problems
      SchemaChangeCompat,     // proposed change conflicts with existing records
      RecordIntegrity,        // record violates current schema
      ReferentialIntegrity    // reference field target missing/deleted
  }
  severity: enum { Blocking, Warning }
  message: string                    // user-facing description
  affected_ids: string[]             // UUIDs of schema elements or records involved
  suggested_fix: string (nullable)   // e.g. "Provide a default for 'due_date' or run Bulk Fill"
```

**Schema migration model.** Migration is **stateless and diff-based**, not step-versioned. There are no numbered migration scripts, no `migrations/0001_add_field.*` equivalents, and no per-version upgrade code. The schema files on disk are the migration target; migration is the diff between any given record's field set and the current schema's `FieldDefinition` set. This makes schema changes merge cleanly under VCS without coordinating migration numbers across branches.

Migration runs **in-memory, at record load time** — never as a disk-write pass triggered by project open or pull. When a record is hydrated into a `RecordWorkingCopy`, the schema engine reconciles it against the current schema: fields present in the schema but missing on the record receive their declared default in the `working` state; fields present on the record but absent from the schema are moved into `_deprecated_fields` in the working state. The `baseline` continues to reflect exactly what is on disk. If the record is subsequently edited and committed, the migration rides along in the normal commit pipeline — disk I/O happens only there, preserving the invariant in CLAUDE.md rule 9. If the record is never edited, the file on disk stays in its older shape until some other change triggers a write.

One consequence: after a pull that adds a required field with a default, every record appears as a pending change in the source control panel even though the user did nothing directly. This is the correct behavior — the records' in-memory state differs from what is on disk — and it gives the user a clean entry point to review the migration and commit it. An explicit "purge deprecated fields" action (surfaced as a destructive confirmation dialog) is required for hard removal.

**Schema migration on pull.** A pull that brings in schema changes (`schema/entity_types/` or `schema/enums/`) is not a special case — it simply updates the schema files on disk, and subsequent record loads reconcile against the new schema as described above. Schema file conflicts (e.g., both sides renamed the same entity type) are resolved through the conflict resolution UI treating the schema file as the unit of conflict, per §6.9.

Because pre-commit validation (§6.10) runs on every commit produced by the application, invalid schema states should rarely arrive via pull. When they do — typically from external editing of JSON files, older Philotechnia versions, or tooling outside the app — affected records are flagged with a banner in the UI and offered the standard resolution flows (set a default, bulk fill, fix the reference). Unlike pre-commit failures, post-pull invalid state does not block editing or further commits on other records; it is treated as a defect that the team resolves incrementally.

**Format version compatibility.** `format_version` in `manifest.json` is the application-level file-format marker, incremented when the on-disk shape of any Philotechnia-managed file changes (record JSON structure, manifest fields, schema file layout). It is distinct from any schema-level versioning — per the migration model above, the schema files are their own authoritative state. On project open:

- If `format_version` on disk equals the running build's supported version, open normally.
- If `format_version` on disk is lower, the running build knows how to upgrade. Records are reconciled in memory at load time; the first commit after opening writes the newer `format_version` back to `manifest.json`.
- If `format_version` on disk is higher than the running build supports, the application refuses to open the project and surfaces a dialog indicating that a newer version of Philotechnia is required. A read-only fallback is deliberately not offered — a partial-format reader risks silently misinterpreting fields and eventually writing back bytes that corrupt the newer format.

**CommandStack → stage() pipeline:** When the user initiates a commit, the application core iterates over all `RecordWorkingCopy` instances in the current session and calls `is_dirty()` on each. It then invokes `SchemaValidator::validate()` against the proposed post-commit state (schema as it would be after pending changes, combined with the working state of every dirty record). If validation returns an error, the commit panel displays each `ValidationIssue` and the commit action is disabled until every `Blocking` issue is resolved; `Warning` issues are shown but do not block the commit. Once validation passes, for every dirty record the pipeline resolves the on-disk path using the hash-prefix structure (`records/{entity-type-id}/{xx}/{record-uuid}.json`) and writes the current working state to that file. The resulting set of file paths is then passed to `SourceControlProvider::stage()` (a no-op for providers that do not support explicit staging). After a successful `commit()`, `CommandStack::mark_clean()` is called, which clears the done stack boundary and resets all `RecordWorkingCopy` baselines to the newly committed state.

**Commit pipeline failure handling.** The pipeline is not batch-atomic at the filesystem level — the commit-atomicity boundary is the VCS commit, not the multi-file write. Failure modes are handled as follows:

1. **Write failure during the record-write phase.** On the first write that fails (out of disk, permission denied, filesystem error), the pipeline aborts immediately and does not call `stage()` or `commit()`. Records written before the failure remain on disk; the failing record and all subsequent records stay in their pre-write state. `RecordWorkingCopy` in-memory state is not mutated by the write phase — baselines are updated only by `mark_clean()` after a successful `commit()` — so all records remain dirty in memory. The user can retry the commit: atomic rename makes re-writes idempotent, so records that already succeeded are re-written to the same bytes, and the retry completes the batch.

2. **`stage()` or `commit()` failure after successful writes.** The written files are valid on disk but the commit did not happen. `mark_clean()` is not called; in-memory state is still dirty. A retry completes the batch.

3. **Application crash mid-batch.** Same on-disk result as an abort: some records written, some not. On restart, in-memory state is lost and baselines are re-read from disk. Records that were successfully written appear "clean" to Philotechnia's dirty-state tracker but show up as uncommitted changes to the VCS provider — `get_pending_changes()` reflects them, and the source control panel surfaces them per §6.9. The user commits them through the normal flow. The orphaned `.tmp` recovery on startup (see §7.1 crash safety) cleans up the temp file from the specific write that was in flight when the crash occurred.

4. **Invariants across partial writes.** `SchemaValidator` runs against the in-memory post-commit state before any write begins, so the validator's guarantees hold for every committed state. A partial on-disk state between writes may not satisfy cross-record invariants, but that state is never observable as a committed state — it is `pending changes` from the VCS perspective and is resolved by the retry completing the batch.

5. **`mark_clean()` discipline.** `CommandStack::mark_clean()` is called only after `SourceControlProvider::commit()` returns success. Any failure earlier in the pipeline leaves the `CommandStack` state and `RecordWorkingCopy` baselines unchanged.

A preflight check that estimates batch size against available disk space and fails fast on `ENOSPC` before starting any writes is a UX improvement, not a correctness requirement. It is a candidate feature for the M4 commit panel.

**Worked example: `CreateRecord`.** The following concrete subclass is the canonical starting point for every other `Command` in the codebase — editing a field, deleting a record, renaming a schema element, and so on all follow the same shape. The example illustrates the in-memory-only contract (CLAUDE.md rule 9), the stable-UUID pattern that keeps undo / redo round-trippable, and the handoff point to the commit pipeline.

```cpp
// src/core/command.hpp — the abstract base referenced above and by CLAUDE.md rule 9.
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
//
// Adds a newly-authored record to the in-memory project state. The disk
// write does not live here — it is executed by the commit pipeline above
// when the user commits.
class CreateRecord final : public Command {
public:
    // record_id is generated once by the caller (the "New record" UI handler)
    // and passed in — not generated inside execute(). This keeps the record's
    // UUID stable across execute / undo / redo cycles so the pre-commit diff
    // panel and the undo history render consistently.
    CreateRecord(ProjectState& state,
                 EntityTypeId entity_type,
                 RecordId record_id,
                 std::string created_by,
                 std::unordered_map<FieldId, Value> initial_values);

    std::string_view description() const override { return description_; }

    void execute() override {
        // Build the in-session Record. created_at / updated_at stay at the
        // default-constructed sentinel — the commit pipeline stamps them
        // (along with updated_by) when it writes the dirty working copy.
        // baseline is nullopt because the record has no on-disk state yet;
        // is_dirty() is therefore trivially true until the first commit
        // rotates working into baseline.
        Record working{
            .id                 = record_id_,
            .entity_type_id     = entity_type_,
            .created_at         = {},               // filled by commit pipeline
            .created_by         = created_by_,
            .updated_at         = {},               // filled by commit pipeline
            .updated_by         = created_by_,
            .deleted_at         = std::nullopt,
            .fields             = initial_values_,  // copy; undo() must leave state reusable
            ._deprecated_fields = {},
        };
        state_.records.insert_or_assign(
            record_id_,
            RecordWorkingCopy{
                .record_id = record_id_,
                .baseline  = std::nullopt,
                .working   = std::move(working),
            });
        // NOTE: no disk I/O. The commit pipeline will later call
        //   SchemaValidator::validate(state.schema, state.records) → if ok,
        //   write each dirty working copy to records/{entity-type}/{xx}/{uuid}.json
        //   via atomic_write(), then stage() + commit() through the active
        //   SourceControlProvider, then CommandStack::mark_clean().
    }

    void undo() override {
        // Remove the entry execute() inserted. Infallible — pure in-memory.
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

Three properties of this example generalise to every other `Command`:

1. **Identity is supplied, not generated.** `record_id` is a constructor argument, not something `execute()` mints. This is how undo → redo re-exposes the same record (same UUID in the history, same entry in the pre-commit diff panel). The pattern holds for `CreateEntityType`, `CreateEnum`, and any other command that introduces new identifiers.
2. **All mutable state lives on `ProjectState`** (or its close cousins — schema working state, `CommandStack` bookkeeping). `execute()` and `undo()` only touch references the command captured at construction time; no globals, no singletons, no filesystem.
3. **The commit pipeline is the only place writes happen.** A natural smoke test for rule 9 compliance: if every `stage()` / `commit()` / `atomic_write()` call site were removed from the codebase, the application would still run end-to-end with every in-session feature working — the user would simply be unable to persist. That invariant is what makes `Command::undo()` infallible and `CommandGroup::undo()` safe to iterate in reverse without partial-failure handling.

Commands that look like they need I/O — say, `ImportCsv` that reads a file or `AttachFile` that copies bytes into the repo — are resolved by moving the I/O out of `execute()`: read the CSV before the command is enqueued and pass its parsed rows as constructor arguments; store attachment URIs (per decision #22) rather than embedding bytes. The command itself always stays pure in-memory.

`DeleteRecord` is the mirror image of the example above — `execute()` sets `working.deleted_at` to now in the working copy, `undo()` restores the previous value (captured at construction). `SetFieldValue` is smaller still: `execute()` writes `working.fields[field_id] = new_value`, `undo()` restores the captured old value. Each lands under `src/commands/` as its own translation unit; each fits in 40–80 lines including header and body.

### 7.3 Sample Files

The two examples below show how the entity types in §7.2 serialise to disk. They are intentionally minimal — enough to make the file layout concrete for an implementer or a reviewer reading the repo.

**`manifest.json`** — project root

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

**A record** — path: `records/6a2c1f88-0d3e-4a77-b4c2-1e9d0f8b2a55/a7/a7d4c82e-3f91-4b60-9c8e-b2f5d1a3c7e8.json`

The directory segment `6a2c1f88-0d3e-4a77-b4c2-1e9d0f8b2a55` is the entity type UUID (for example, an "Initiative" entity type defined under `schema/entity_types/`). `a7` is the first two hex characters of the record UUID — the hash-prefix bucket. The filename is the full record UUID.

```json
{
  "id": "a7d4c82e-3f91-4b60-9c8e-b2f5d1a3c7e8",
  "created_at": "2026-03-15T09:47:33Z",
  "created_by": "dholt",
  "updated_at": "2026-03-15T09:47:33Z",
  "updated_by": "dholt",
  "deleted_at": null,
  "fields": {
    "2c8d4a1e-5f6b-4c7d-9e8f-0a1b2c3d4e5f": "Migrate auth service to OAuth 2.1",
    "8b7a6c5d-4e3f-4210-9a8b-7c6d5e4f3a2b": 2,
    "4f3e2d1c-0b9a-4876-8765-2198a7b6c5d4": "2026-06-30",
    "9d8c7b6a-5f4e-4321-8765-6d5c4b3a2109": false
  }
}
```

On the insert commit, `updated_at` equals `created_at` and `updated_by` equals `created_by` — the record has been written exactly once. Subsequent commits that touch this record update the `updated_*` pair (and `created_*` stays fixed) so the table view can sort and filter by last-modified without walking VCS history.

Each key under `fields` is the UUID of a `FieldDefinition` from the relevant `EntityType` file — not a human-readable name. This keeps field renames a pure display-layer operation with no file rewrites. The UI resolves UUIDs to display names whenever it renders the record.

**A post-migration record.** When a field is removed from the schema while a record still carries a value for it, the value is moved to `_deprecated_fields` on the record's next write. A migrated record looks like the original sample with the removed field relocated:

```json
{
  "id": "a7d4c82e-3f91-4b60-9c8e-b2f5d1a3c7e8",
  "created_at": "2026-03-15T09:47:33Z",
  "created_by": "dholt",
  "updated_at": "2026-04-22T14:00:00Z",
  "updated_by": "dholt",
  "deleted_at": null,
  "fields": {
    "2c8d4a1e-5f6b-4c7d-9e8f-0a1b2c3d4e5f": "Migrate auth service to OAuth 2.1",
    "8b7a6c5d-4e3f-4210-9a8b-7c6d5e4f3a2b": 2,
    "4f3e2d1c-0b9a-4876-8765-2198a7b6c5d4": "2026-06-30"
  },
  "_deprecated_fields": {
    "9d8c7b6a-5f4e-4321-8765-6d5c4b3a2109": {
      "value": false,
      "archived_at": "2026-04-22T14:00:00Z"
    }
  }
}
```

The `_deprecated_fields` key is omitted entirely on records that have never held an archived field. The key is removed only by the "purge deprecated fields" action (a destructive confirmation dialog; deferred feature).

---

## 8. Architecture

### 8.1 High-Level Components

```
┌──────────────────────────────────────────────────────────────┐
│                        UI Layer (Qt)                         │
│  - Record table, detail view, schema builder                 │
│  - Undo history list (Qt widget over CommandStack)           │
│  - Source control panel: pending changes, pre-commit diff,   │
│    commit, push/pull, branches, history, conflict resolution │
└──────────────────────────┬───────────────────────────────────┘
                           │
┌──────────────────────────▼───────────────────────────────────┐
│                   Application Core (C++)                     │
│  - Schema engine                                             │
│  - Query engine                                              │
│  - Record manager                                            │
│  - CommandStack (undo/redo + dirty state)                    │
│  - SchemaValidator (pre-commit validation — §6.10)           │
│  - Local profile (commit author identity — §6.7)             │
└────────────┬─────────────────────────────┬───────────────────┘
             │                             │
┌────────────▼────────────┐  ┌─────────────▼─────────────────┐
│   Storage Engine (C++)  │  │  SourceControlProvider (C++)   │
│  - Hash-prefix dir walk │  │  Abstract interface — see §8.4 │
│  - One JSON file/record │  └──────────────┬────────────────┘
│  - In-memory index      │                 │
│  - Atomic writes        │  ┌──────────────┼──────────────────┐
└─────────────────────────┘  │              │                  │
                    ┌────────▼──────┐ ┌─────▼─────────┐ ┌─────▼──────────┐
                    │  GitProvider  │ │  SvnProvider  │ │PerforceProvider│
                    │ (libgit2) v1  │ │  (v2 TBD)     │ │   (v3 TBD)     │
                    └───────────────┘ └───────────────┘ └────────────────┘
```

### 8.2 Platform Targets
| Platform | Minimum Version | Notes |
|----------|----------------|-------|
| macOS    | 13 (Ventura)   | ARM + Intel universal binary |
| Windows  | 10 (build 19041)| x86-64 |
| Linux    | Ubuntu 22.04 LTS| x86-64; other distros best-effort |

### 8.3 UI Framework

**Decision: Qt (LGPL 3.0)**

Qt is the chosen UI framework. Key reasons in the context of this application:

- `QTableView` with a custom model is a natural fit for the record table view
- Qt's widget toolkit provides the list view, form layouts, diff panels, and source control UI surfaces without needing custom rendering
- The undo history list is rendered as a Qt `QListView` bound to `CommandStack::history()` — Qt renders the list, but the underlying stack is the custom `CommandStack` defined in §7.2, not `QUndoStack`. This design is required because `CommandStack` is tightly integrated with the dirty state tracker and the `SourceControlProvider` commit lifecycle, which `QUndoStack` has no awareness of
- Consistent, professional appearance across macOS, Windows, and Linux without significant per-platform work

**Licensing: LGPL 3.0 — Dynamic Linking Required**

Qt is used under the LGPL 3.0 license. This permits commercial use provided the following condition is met: Qt must be dynamically linked so that end users can replace the Qt libraries with a modified version. This requirement must be accounted for in the build system and packaging pipeline:

- All three platform builds (macOS, Windows, Linux) must link Qt as shared libraries, not statically
- Installers must ship the Qt `.dylib` / `.dll` / `.so` files alongside the application binary
- The end-user license agreement must notify users of their right to relink against a modified Qt
- Qt source code (or a pointer to it) must be made available to users on request

This is a build and packaging constraint that must be established at M0a before any other work proceeds.

### 8.4 SourceControlProvider Interface

The source control layer is implemented as a C++ abstract base class. The application core and UI layer depend only on this interface — never on a concrete provider. This is a strict architectural boundary.

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
// The three byte blobs are surfaced unparsed. JSON-aware field-level merge logic
// lives in the application core (see §6.9) — the provider is deliberately unaware
// of JSON structure so that future providers (SVN, Perforce) surface the same
// three-blob shape with no provider-side JSON handling.

struct RemoteStatus {
    bool available;       // false if provider cannot determine (e.g., no network)
    int  local_only;      // local commits / changes not yet on remote
    int  remote_only;     // remote changes not yet local
    // For centralized providers, local_only is always 0 (no local commits exist);
    // remote_only reflects whether the working copy is behind the server.
};

struct StashEntry {
    std::string id;        // provider-specific reference (e.g. "stash@{0}")
    std::string message;   // user-provided or auto-generated description
    std::chrono::system_clock::time_point timestamp;
};

// Application-owned enums — defined in src/core/workflow.hpp. Both the UI and
// every provider depend on these symbols; providers never invent new values.
// Adding a new state or action requires a matching UI renderer / dispatch
// entry in the same change (enforced by the exhaustiveness test described
// below the interface declaration).

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
    std::string branching_model_name;         // e.g. "Feature branches", "Trunk-based development"
    std::string branching_model_description;  // 1–2 sentences; shown in orientation UI for new users

    struct RecoveryFlow {
        ErrorState  error_state;   // which state this flow recovers from
        std::string title;         // short heading shown in the recovery UI
        std::string description;   // explanation of what the state means and how it arose
        struct Step {
            std::string label;       // button or action label, provider-authored copy
            std::string description; // what this step does
            ActionId    action_id;   // resolved by the central dispatcher in src/core/workflow.cpp;
                                     //   never interpreted directly by UI widgets
        };
        std::vector<Step> steps;
    };
    std::vector<RecoveryFlow> recovery_flows;  // provider supplies flows for states it can create
};

// All fallible operations return std::expected<T, Error> per the error-handling
// convention (see CLAUDE.md Conventions). Pure accessors (capability flags, labels,
// workflow metadata) return plain values because they cannot fail. Error is the
// project-owned error type defined in src/core/error.hpp.

class SourceControlProvider {
public:
    virtual ~SourceControlProvider() = default;

    // Capability flags — query before calling optional operations.
    // Pure accessors; cannot fail.
    virtual bool supports_staging() const = 0;           // git: true;  SVN/Perforce: false
    virtual bool supports_offline_commits() const = 0;   // git: true;  SVN/Perforce: false
    virtual bool supports_branching() const = 0;         // git: true;  SVN: limited; Perforce: limited
    virtual bool supports_stashing() const = 0;

    // Provider-supplied UI labels — keeps the UI layer provider-agnostic.
    // Pure accessors; cannot fail.
    virtual std::string commit_action_label()  const = 0; // "Commit"
    virtual std::string sync_action_label()    const = 0; // "Push" / "Submit" / "Sync"
    virtual std::string receive_action_label() const = 0; // "Pull" / "Update" / "Sync"

    // Lifecycle
    virtual std::expected<void, Error> initialise(const std::filesystem::path& dir) = 0;
    virtual std::expected<void, Error> clone(const std::string& url,
                                             const std::filesystem::path& dest) = 0;

    // Working state
    virtual std::expected<std::vector<PendingChange>, Error> get_pending_changes() = 0;
    virtual std::expected<void, Error> stage(
        const std::vector<std::filesystem::path>& files) = 0;
    // stage() is a no-op (returns success) when supports_staging() == false
    virtual std::expected<void, Error> commit(const std::string& message,
                                              const std::string& author) = 0;
    // For centralized providers, commit() requires connectivity and implies push

    // Remote sync
    virtual std::expected<void, Error> push() = 0;
    // For centralized providers where commit() implies push, push() is a no-op (returns success).
    virtual std::expected<void, Error>         pull() = 0;
    virtual std::expected<RemoteStatus, Error> remote_status() = 0;
    // RemoteStatus.available == false is a normal return indicating the remote could not
    // be queried (e.g., offline). An Error is returned only for hard failures such as
    // repository corruption or misconfiguration.

    // Branching — return empty / no-op (success) when supports_branching() == false
    virtual std::expected<std::vector<std::string>, Error> get_branches() = 0;
    virtual std::expected<std::string, Error>              current_branch() = 0;
    virtual std::expected<void, Error>                     create_branch(const std::string& name) = 0;
    virtual std::expected<void, Error>                     switch_branch(const std::string& name) = 0;

    // Stashing — no-op / empty (success) when supports_stashing() == false
    virtual std::expected<void, Error>                    stash(const std::string& message) = 0;
    virtual std::expected<std::vector<StashEntry>, Error> get_stashes() = 0;
    virtual std::expected<void, Error>                    apply_stash(const std::string& stash_id) = 0;
    virtual std::expected<void, Error>                    drop_stash(const std::string& stash_id) = 0;

    // History
    virtual std::expected<std::vector<HistoryEntry>, Error> get_history(
        const std::filesystem::path& file, int limit = 100) = 0;

    // Conflict resolution
    // Providers perform a programmatic three-way merge and surface each conflict
    // as three byte blobs (ancestor, local, incoming) without ever writing
    // file-level conflict markers to disk. For GitProvider: git_remote_fetch
    // followed by git_merge_trees with manual index handling — never the
    // default checkout path. JSON-aware merge logic lives in the application
    // core (see §6.9), not in the provider.
    virtual std::expected<std::vector<Conflict>, Error> get_conflicts() = 0;
    virtual std::expected<void, Error> resolve(const std::filesystem::path& file,
                                               std::optional<std::string> resolved_content) = 0;

    // Workflow guidance — pure accessors over provider-static metadata; cannot fail.
    virtual WorkflowMetadata get_workflow_metadata() const = 0;
    virtual std::string      suggested_primary_branch_name() const = 0;
    // Returns the provider's suggested default for the primary branch name
    // (e.g. "main" for git, "trunk" for SVN). Shown as pre-filled default
    // during project creation; user confirms or overrides. Stored in manifest.json.
};
```

**Provider roadmap:**

| Provider | Library | Status | Notes |
|----------|---------|--------|-------|
| Git | libgit2 | v1 | Full implementation; SSH + HTTPS auth; branching supported; offline commits supported |
| SVN | libsvn (Apache) | v2 candidate | `supports_staging()` = false; `supports_offline_commits()` = false; `commit()` implies push; branching limited |
| Perforce | Helix Core C++ API | v3 candidate | Significantly different model (changelists, mandatory checkout in many configs); requires interface evaluation before committing |

**Action dispatch.** `ActionId` values resolve to behaviour through a single dispatch table in `src/core/workflow.cpp`. UI widgets never `switch` on `ActionId` themselves — they emit the value to the dispatcher, which invokes the right combination of `SourceControlProvider` method, application command, or UI navigation. This keeps the UI layer free of any knowledge about how recovery actions actually execute (and preserves CLAUDE.md rule 1, which forbids UI from depending on a concrete provider).

**Rule-6 exhaustiveness — staged enforcement.** CLAUDE.md rule 6 requires the application to guide the user out of every state it can create. Decision #27 expresses this as code through three CI-enforced checks; each lands with the code it depends on, so the rule-6 guarantee is enforced at the earliest moment each piece is meaningful.

1. **`ErrorState` → `RecoveryFlow` coverage (lands at M1b).** A unit test walks `enum class ErrorState`; for each value, it passes if *some* shipped provider's `WorkflowMetadata::recovery_flows` contains a `RecoveryFlow` whose `error_state` equals that value, or if the value is listed in the `kUnshippedErrorStates` array in `src/core/workflow.hpp`. Otherwise it fails. `kUnshippedErrorStates` is the narrow, visible escape hatch for states that belong to providers not yet shipped — v1 uses it for `ErrorState::CentralizedCommitBehind`, which is SVN/Perforce-only. When SvnProvider ships, the same commit that adds the provider also removes `CentralizedCommitBehind` from `kUnshippedErrorStates` (and so the test now requires a flow for it). Adding a new `ErrorState` value therefore forces the author to either supply a `RecoveryFlow` from a shipped provider in the same commit or add the value to `kUnshippedErrorStates` — both reviewer-visible.
2. **`ErrorState` → renderer coverage (lands at M3–M4).** Once the recovery UI exists, a second test walks the enum and asserts that the recovery-UI renderer handles every value, with the same `kUnshippedErrorStates` exemption applied.
3. **`ActionId` → dispatch entry coverage (lands at M4 with the dispatch table).** This is a compile-time exhaustiveness check: the dispatch function in `src/core/workflow.cpp` is a `switch` over every `ActionId` enumerator with no `default` case, so adding an `ActionId` without a dispatch entry is a build failure rather than a CI failure.

`TestProvider` (§8.4a) lives under `tests/support/` and is deliberately excluded from check (1) — the test walks shipped providers registered under `src/vcs/`, not test doubles.

**Resolution semantics.** `resolve()` is called once per conflicted file after the application core has reconciled the three blobs surfaced by `get_conflicts()` into a user-approved result. The `resolved_content` parameter is the complete post-merge contents of the file as UTF-8 bytes — for record and schema files this is the fully merged JSON, already shaped as the provider would find it on disk after a clean commit. A present value instructs the provider to write the file (via the same atomic temp-file-then-rename path as §7.1) and clear the conflict from its index; `std::nullopt` instructs the provider to accept a delete (the correct resolution for `ModifiedVsDeleted` and `DeletedVsModified` conflicts where the user chooses the deletion side) — the provider removes the working-copy file if present and marks the path resolved. The provider does not produce a merge commit itself; after every conflict has been resolved, the application invokes the normal commit pipeline (§7.2) which writes any remaining dirty working copies, runs `SchemaValidator`, and calls `stage()` + `commit()` with a merge-commit message. Partial resolution is supported: the UI may call `resolve()` for some files and defer others; `get_conflicts()` continues to report the unresolved set until all are handled, and `commit()` fails with a `Category::Conflict` error if invoked while conflicts remain.

### 8.4a TestProvider

`TestProvider` is the test double for `SourceControlProvider`. It lives under `tests/support/test_provider.hpp` / `.cpp`, is compiled into a `test_support` static library, and is linked only by test binaries — never by the shipping application. M0b ships the scaffolding; tests consume it from M1a onward as real code that takes a `SourceControlProvider&` comes online.

**Role.** Integration tests of the commit pipeline (§7.2), recovery-flow dispatch (§8.4), and conflict resolution (§6.9) all need a provider they can drive deterministically without a real git repo. `TestProvider` provides that surface. It is explicitly **not** a shipped provider and is excluded by construction from the rule-6 exhaustiveness checks (§8.4, decision #27): check (1) walks shipped providers registered under `src/vcs/`, and `TestProvider` lives under `tests/support/`; checks (2) and (3) are UI/dispatch-scoped and do not consume providers at all.

**Programmable state.** Every piece of state a provider surfaces is a public, mutable member. Tests pre-load whatever shape they need — pending changes, branches, stashes, history, conflicts, remote status — before driving the code under test. Defaults are "git-like and healthy": all capability flags true, one branch named `main`, no pending changes, no conflicts, `RemoteStatus{available=true, 0, 0}`.

**Fault injection.** A per-operation FIFO queue of `Error` values. The next call to each `Operation` pops its queued failure (if any) and returns it; an empty queue means the call performs the default action against the programmable state. This covers both "the operation always fails" (enqueue once) and "the nth call fails" (enqueue with preceding no-op successes) patterns. The Operation enum is orthogonal to `ActionId` — it names provider methods, not UI actions.

**Call recording.** Every invocation is captured in a `calls` vector for test assertions ("was `commit()` called with this message?", "was `stage()` called before `commit()`?"). Successful commits additionally populate a `commits` vector with `{message, author, files}` tuples.

```cpp
// tests/support/test_provider.hpp
class TestProvider final : public SourceControlProvider {
public:
    // Capability flags — default to git-like. Tests that exercise the
    // centralized-provider UX flip these before invocation.
    bool supports_staging_         = true;
    bool supports_offline_commits_ = true;
    bool supports_branching_       = true;
    bool supports_stashing_        = true;

    // Provider-supplied UI labels — configurable.
    std::string commit_action_label_  = "Commit";
    std::string sync_action_label_    = "Push";
    std::string receive_action_label_ = "Pull";

    // Programmable state.
    std::vector<PendingChange> pending_changes;
    std::vector<std::string>   branches        = {"main"};
    std::string                current_branch_ = "main";
    std::vector<Conflict>      conflicts;
    std::vector<HistoryEntry>  history;
    std::vector<StashEntry>    stashes;
    RemoteStatus               remote_status_  = {true, 0, 0};
    WorkflowMetadata           workflow_metadata_;
    std::string                suggested_primary_branch_name_ = "main";

    // Fault injection. Enqueue an Error to be returned by the NEXT call
    // to the named Operation. FIFO; empty queue means the operation
    // succeeds against the programmable state.
    enum class Operation {
        Initialise, Clone,
        GetPendingChanges, Stage, Commit,
        Push, Pull, GetRemoteStatus,
        GetBranches, CurrentBranch, CreateBranch, SwitchBranch,
        Stash, GetStashes, ApplyStash, DropStash,
        GetHistory, GetConflicts, Resolve,
    };
    void enqueue_failure(Operation op, Error err);

    // Call log — every invocation recorded for test assertions.
    struct Call {
        Operation                          op;
        std::vector<std::string>           string_args;
        std::vector<std::filesystem::path> path_args;
    };
    std::vector<Call> calls;

    // Commits actually made against this provider, captured at commit time.
    struct RecordedCommit {
        std::string                        message;
        std::string                        author;
        std::vector<std::filesystem::path> files;
    };
    std::vector<RecordedCommit> commits;

    // Conflict-seeding helpers — one per Conflict::Kind variant. These
    // append a fully-shaped Conflict to the conflicts vector so tests
    // exercising the JSON-aware merge logic in the application core
    // (§6.9) never hand-roll Conflict structs; the shape stays consistent
    // as Conflict evolves.
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

    // SourceControlProvider overrides follow. Each pops from the failure
    // queue first; if a failure is queued for this Operation, the call is
    // recorded and the Error is returned. Otherwise the call performs the
    // default action against the programmable state and records itself.
    // ...
};
```

**Example — commit-pipeline failure coverage (§7.2, decision #24).** The commit pipeline's guarantee that `mark_clean()` runs only after `commit()` returns success is testable directly without arranging a disk-full condition:

```cpp
TestProvider tp;
tp.enqueue_failure(TestProvider::Operation::Commit,
                   Error::io("simulated ENOSPC during record write"));

auto result = commit_pipeline.run(tp, dirty_records, "msg", "author");
REQUIRE_FALSE(result.has_value());
REQUIRE(result.error().category == Error::Category::Io);
REQUIRE(tp.commits.empty());            // pipeline aborted before commit
REQUIRE_FALSE(stack.is_clean());        // mark_clean() was never called
```

**Example — recovery-flow dispatch.** Rule-6 recovery flows (decision #27) are exercised by populating `workflow_metadata_` with a specific `RecoveryFlow` and queuing a provider error that should route to it:

```cpp
TestProvider tp;
tp.workflow_metadata_.recovery_flows.push_back(
    make_reauthenticate_flow());  // test helper
tp.enqueue_failure(TestProvider::Operation::Push,
                   Error::authentication("HTTPS credentials rejected"));

auto dispatch = dispatcher.route(tp, ActionId::Push);
REQUIRE(dispatch.recovery_flow_rendered);
REQUIRE(dispatch.recovery_flow.error_state == ErrorState::AuthenticationRequired);
```

**Example — conflict resolution over seeded state.** The four seeding helpers let a test set up any `Conflict::Kind` without fragility:

```cpp
TestProvider tp;
tp.seed_conflict_both_modified(
    "records/6a2c.../a7/a7d4....json",
    /*base=*/    R"({"fields": {"status": "Open"}})",
    /*local=*/   R"({"fields": {"status": "InProgress"}})",
    /*incoming=*/R"({"fields": {"status": "Closed"}})");

auto merged = field_level_merge(tp.get_conflicts().value());
REQUIRE(merged.size() == 1);
REQUIRE(merged[0].decisions.size() == 1);  // one conflicting field
```

### 8.5 Async I/O and Threading Model

The project-open performance target (< 3 seconds for 10K records on local SSD — see §11) requires parallelising the directory walk and per-file JSON parse. The threading approach is as follows:

**Directory walk:** A single thread performs the hash-prefix directory walk and enqueues file paths onto a work queue. Because `records/` is organised into at most ~200-file subdirectories by the hash-prefix structure, the walk itself is fast and does not need parallelising.

**Parallel parse:** A thread pool (sized to `std::thread::hardware_concurrency()`, capped at a reasonable maximum such as 8) drains the work queue. Each worker reads a file, calls yyjson to parse it, and inserts the resulting in-memory record into a thread-safe portion of the index. Insertion uses per-entity-type locks or a concurrent data structure to avoid a single global lock becoming a bottleneck.

**UI-thread bridge:** The in-memory index populated by the parse workers is not the same object as the `QAbstractItemModel` rendered by the table view — the model is a UI-thread view over the index and must be mutated only from the UI thread. Workers therefore never touch any Qt object. As each worker completes its chunk, it posts a completion batch (the record IDs it inserted, or the first parse error it encountered) back to the UI thread via `QMetaObject::invokeMethod(target, ..., Qt::QueuedConnection)`, where `target` is a long-lived `QObject` that lives on the UI thread and holds a reference to the model. The queued invocation runs inside the Qt event loop; its handler asks the model to emit the appropriate `rowsInserted` (or `dataChanged`) signal for the newly-visible range, and surfaces any parse failure as an inline error state in the project-open UI. Batching at worker-chunk granularity — not per-file — keeps UI-thread wakeups bounded regardless of record count. The same pattern applies to any later background work that needs to update the UI (e.g., incremental full-text index population, background commit pipeline status reporting): `std::async` worker plus `QMetaObject::invokeMethod(target, ..., Qt::QueuedConnection)` is the project's canonical UI-handoff idiom. `QFutureWatcher` and `QtConcurrent` are deliberately not used — staying on `std::async` keeps the thread-pool ownership inside the application core rather than inside Qt, and keeps the `SourceControlProvider` / core / storage layers free of any Qt dependency (preserving CLAUDE.md rule 1's separation of concerns between core and UI).

**Platform async I/O:** For the v1 implementation, `std::async` / `std::future` with a manually managed thread pool is sufficient and avoids introducing a platform-specific I/O framework dependency. If profiling reveals that syscall overhead (rather than parse time) is the bottleneck at scale, platform-native async I/O (Linux `io_uring`, Windows IOCP, macOS `kqueue`) can be added as a future optimisation behind an abstraction layer. This is deferred until profiled evidence justifies the complexity.

**Write path:** Record writes do not use the thread pool — they are synchronous on the calling thread, using the atomic write-to-temp-then-rename pattern described in §7.1. Concurrent writes from different UI actions are serialised by the `CommandStack`, so parallel writes from application code are not a concern in v1.

### 8.6 Project-open cache

Subsequent project opens target < 1 second (see §11). This is achieved via an mtime-based cache of parsed record content, stored per-machine and per-project — never in the project directory. The cache is a complement to, not a replacement for, the cold-open pipeline in §8.5: a cold open populates the cache; subsequent opens consult it to skip disk reads and parses for unchanged files.

**Location.** One cache directory per project, keyed by `project.id` from `manifest.json`, under the OS application data root:

- macOS: `~/Library/Application Support/Philotechnia/cache/{project-id}/`
- Windows: `%APPDATA%/Philotechnia/cache/{project-id}/`
- Linux: `$XDG_CACHE_HOME/philotechnia/{project-id}/`, falling back to `~/.cache/philotechnia/{project-id}/`

Because the key is the project UUID — stable across filesystem moves and tracked in the repo — renaming or relocating the project folder does not orphan its cache.

**Structure.** A single cache file per project (`index.json`), yyjson-serialised, containing:

- `cache_format_version` (integer) — governs the cache file's own layout; distinct from project `format_version`
- `project_format_version` (integer) — the project `format_version` at time of write
- `manifest_fingerprint` (string) — hash over `manifest.json`; used as a duplicate-project safety check (see below)
- `schema_fingerprint` (string) — hash over the sorted contents of `schema/entity_types/` and `schema/enums/`
- `entries` — array of `{ relative_path, mtime_ns, size, record_bytes }`, where `record_bytes` is the raw JSON of the record as read from disk

The cache stores raw JSON rather than a pre-parsed internal representation. This trades cache-read-plus-parse for disk-read-plus-parse, saving the disk read on cache hits while avoiding a second serialization format that must be kept in sync with the record structure. If profiling later shows parse is the dominant cost, the cache format can be swapped for a custom binary layout behind the cache interface.

**Invalidation.**

- Per-entry: a record's mtime or size differs from the cached value → the file is re-read and re-parsed; the entry is replaced.
- Per-entry: a record file is on disk but absent from the cache → parse and add.
- Per-entry: a record is in the cache but no longer on disk → drop from the in-memory index and from the cache.
- Whole cache: project `format_version` on disk differs from cached → full invalidation and cold rebuild.
- Whole cache: schema fingerprint differs from cached → full invalidation (records need re-reconciliation against the current schema per §7.2's migration model).
- Whole cache: `cache_format_version` mismatch, or the cache file fails to parse → full invalidation.
- Whole cache: `manifest_fingerprint` mismatch → full invalidation. This catches the duplicate-project case where a user has copied a project folder on the filesystem; two folders would share `project.id` and therefore share the cache directory, but their `manifest.json` contents differ on at least timestamps.

**Lifecycle.** The cache is written only after a successful commit — the natural checkpoint where in-memory state matches on-disk state. Open does not write the cache; project close does not write the cache. This extends CLAUDE.md rule 9: the commit pipeline is the only code path that writes record, schema, or cache files. If the app crashes without a successful commit, the cache reflects the last successful commit — still internally consistent, just stale by whatever was in memory but uncommitted, which the commit pipeline would have recomputed anyway.

**Orphan cleanup.** Cache directories for projects not touched in 90 days are deleted on app startup, after the first project's open flow completes (so cleanup never adds latency to cold start). No LRU or size-cap policy in v1 — age-based deletion is sufficient for the expected scale.

### 8.7 Error type

The concrete project-owned error type carried by every `std::expected<T, Error>` return across the application core, storage layer, provider implementations, and command execution (see CLAUDE.md "Conventions — Error handling" for the scope). Defined in `src/core/error.hpp`.

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

    // Optional upstream diagnostic — populated when a provider or OS call's native error
    // is worth preserving. Never rendered raw to the user; surfaced only in a "show
    // details" affordance and in log output.
    struct ProviderError {
        std::string source;   // "libgit2", "posix", "win32", "yyjson", ...
        int         code;     // provider-native numeric code
        std::string detail;   // provider-native message, if any
    };
    std::optional<ProviderError> underlying;
};
```

**Category boundaries.** Categories are distinguished by what the UI does with them, not by where the error was raised:

- `Io`, `Parse`, `Internal` — surface as an inline error state in the affected view; log the `underlying` detail. Retryable for `Io` and transient `Parse` (e.g., a partial write observed between atomic rename windows).
- `NotFound` — usually not an error at the call site (callers use `std::expected` flow control), but may bubble if a cascade expected the referent.
- `Validation` — produced by the commit pipeline when `SchemaValidator` reports any `Blocking` issue; the pipeline aborts before calling `stage()` / `commit()` (see §6.10 and §7.2 commit pipeline). The `message` is the first blocking issue; the full list is surfaced in the commit panel UI, not via `Error`.
- `SourceControl`, `Authentication`, `Network` — route into the recovery UI via the appropriate `ErrorState` (see §8.4). The `Category` answers "what kind of failure"; `ErrorState` answers "which recovery flow to render."
- `Conflict` — signals that an operation (e.g., push, branch switch) is blocked pending conflict resolution. Not itself an error to the user — the UI routes to the conflict resolution panel.
- `FormatVersion` — produced only by project open; triggers the "newer version required" dialog (see §7.2 "Format version compatibility"). Never caught and continued.
- `Cancelled` — not shown as an error; used to unwind a cancelled operation cleanly. Callers typically filter this out of user-facing surfaces.

`ProviderError::source` strings are a small open set. Providers that need a new source string add it in the same commit that raises it, by convention. Log formatting is `"[{source} {code}] {detail}"`.

Helper constructors (`Error::io(msg)`, `Error::from_libgit2(int code)`, `Error::validation(msg)`, etc.) live alongside the type in `error.hpp` and are the preferred call sites — raw brace-initialisation is discouraged because it skips the underlying-diagnostic capture logic.

---

## 9. Tech Stack

| Layer | Technology | Rationale |
|-------|-----------|-----------|
| Primary language | C++23 | Performance, portability, ecosystem maturity. Default for all implementation; ASM deferred unless profiling identifies a specific bottleneck |
| JSON | yyjson | Fast in both read and write directions; C99, integrates cleanly into C++; single library for all JSON operations |
| Source control (v1) | libgit2 | C library implementing `SourceControlProvider` for git; no dependency on a git binary; cross-platform; well-maintained |
| Build system | CMake + Ninja | Industry standard for cross-platform C++ |
| UI | Qt (LGPL 3.0) | Retained-mode widgets; `QTableView` for record grids; `QListView` for undo history rendering; consistent cross-platform appearance. Must be dynamically linked — see §8.3 |
| Testing | Catch2 | Header-only, well-suited for C++ unit and integration tests |
| CI | GitHub Actions | Build and test matrix across all three platforms |

---

## 10. User Flows

### 10.1 Creating a New Project
1. User launches Philotechnia → sees "Open or Create Project" prompt
2. User selects "New" → chooses a name and save location; git is pre-selected as the source control provider with an option to change
3. User confirms the primary branch name (pre-filled with the provider's `suggested_primary_branch_name()`; e.g. "main" for git)
4. Philotechnia initialises the directory with the selected provider and creates the initial directory structure; primary branch name is written to `manifest.json`
5. User is taken to the Schema Builder to define their first entity type
6. User adds fields → saves schema → an initial commit is created automatically → user is taken to the record list (empty)

### 10.2 Adding a Record
1. User selects an entity type from the sidebar
2. Clicks "New Record" → inline form appears with all fields for that entity type
3. User fills in fields → clicks "Save" → a `CreateRecord` command executes, staging a new `RecordWorkingCopy` in memory (no baseline, working state = the form values). The record appears in the table view marked as uncommitted. No file is written to disk at this point; the write occurs later, when the user commits, via the pipeline described in §7.2 (this follows the in-memory-only command contract in §6.8)

### 10.3 Sharing with a Team
1. One team member pushes the project to a shared remote compatible with the configured provider
2. Other team members clone or check out the project locally and open the directory in Philotechnia; the source control provider is auto-detected, and each member's local profile supplies their commit author identity. If a member has not yet created a local profile, the first-launch prompt does so
3. No roster edits, role assignments, or out-of-band credential exchange are required — Philotechnia has no project-level user registry. Access control, where needed, is configured at the VCS host (branch protection, repo permissions)

### 10.4 Querying Records
1. User opens an entity type → clicks the Filter icon
2. Adds one or more conditions (field / operator / value)
3. Results update in real time against the in-memory index; user can save the filter as a named preset
4. User can sort by any column header

### 10.5 Committing and Syncing Changes
1. User makes edits across one or more records; modified records are indicated in the table view and the source control panel shows a pending changes count (driven by in-memory `RecordWorkingCopy` dirty state — nothing has been written to disk yet)
2. User opens the source control panel → reviews the pre-commit diff (field-by-field changes per record) → optionally reverts individual changes
3. User writes a commit message and clicks the commit action (label from `commit_action_label()`). The pipeline (§7.2) runs: `SchemaValidator::validate()` against the proposed post-commit state → if any `Blocking` issues are returned, they are rendered inline in the commit panel and the action is disabled until resolved; otherwise each dirty record is written to its hash-prefix path, passed to `SourceControlProvider::stage()` (if staging is supported), then committed with the local profile as author. On success, `CommandStack::mark_clean()` is called
4. User initiates a push via the sync action (label from `sync_action_label()`) → changes are sent to the configured remote

### 10.6 Resolving a Conflict
1. User initiates a pull → the provider performs a programmatic three-way merge before writing anything to disk
2. If conflicts exist, the conflict resolution UI opens with affected records shown side-by-side, field by field
3. User accepts one side per field, or types a manual value → clicks "Resolve"
4. Once all conflicts are resolved, the user completes the merge commit

---

## 11. Performance Requirements

| Metric | Target | Notes |
|--------|--------|-------|
| Cold start time | < 500 ms on reference hardware | Excludes project open time |
| Project open (10K records, local SSD) | < 3 seconds | Directory walk + per-file JSON parse; parallel async I/O should be used to mitigate syscall overhead |
| Project open (subsequent opens) | < 1 second | mtime-based cache skips unchanged files; only modified files are re-parsed |
| Record list render (10,000 records) | < 16 ms per frame (60 fps) | Operates on in-memory index, not disk |
| Full-text search response | < 500 ms on 100K text field values | Operates on in-memory index |
| Write throughput | ≥ 500 record writes/second (single writer) | One JSON file per record + atomic rename per write |

**Network filesystem warning:** Projects opened from a network-mounted path (NFS, SMB/CIFS, network drives) will have significantly higher open times due to per-file I/O latency. 10K files over a typical LAN could take 30–60+ seconds on first open. This is a known limitation of the one-file-per-record design and should be communicated clearly in the documentation. The recommended practice is to keep the working copy on local storage and use the source control provider for sharing.

---

## 12. Security Considerations

- No application-level passwords. Identity is based on the local profile (username + email stored in OS app data), which supplies commit author information to the source control provider. No credentials, user registry, or role data are stored in the project repo. Access control, where required, is enforced at the VCS hosting level (branch protection rules, repository permissions, signed commits) — Philotechnia does not implement application-layer permissions and does not attempt to.
- No at-rest encryption in v1. Projects are plain JSON on disk. Users needing confidentiality against filesystem-level access should rely on OS-level full-disk encryption (FileVault, BitLocker, LUKS), which does not interfere with the source control provider's ability to diff and merge files. Application-level encryption of record files was considered and rejected because it is structurally incompatible with multi-user collaboration — the source control provider cannot diff or merge encrypted blobs, so any conflict on an encrypted record is unresolvable
- No telemetry, no network calls unless a push or pull is explicitly initiated by the user
- Commit authorship is tied to the logged-in Philotechnia user via the source control provider, providing a tamper-evident audit trail through the provider's history
- Remote credentials are managed by the host OS credential store where supported (git credential helpers on all three platforms); credential handling for SVN and Perforce is provider-specific and will be specified per-implementation
- The project-open cache (§8.6) stores raw record JSON under the OS app data directory. This mirrors the content of the project files on disk — same trust model — and does not introduce a new confidentiality risk, but is worth noting when describing the on-disk footprint to users on a multi-user machine

---

## 13. Resolved Decisions

The following were formerly open questions and have been settled.

1. **Sync architecture:** The `SourceControlProvider` interface is the sync and collaboration mechanism. Git ships in v1. No custom CRDT or sync server. Real-time sync remains a non-goal.
2. **Licensing:** Source-available — publicly readable and auditable but not freely redistributable or modifiable for commercial use. Specific instrument (Business Source License, Functional Source License, or custom) to be selected before first public release.
3. **Assembly scope:** C++ is the default. Hand-written ASM requires a profiled bottleneck to justify it. ASM/SIMD characteristics of third-party libraries should be considered when evaluating new dependencies.
4. **Installer strategy:** Per-platform native installers via CMake + CPack. Artifacts: `.dmg` (macOS), `.msi` (Windows), `.deb` / `.AppImage` (Linux).
5. **Large dataset strategy:** v1 ceiling of 50,000 records per entity type. Hash-prefix structure keeps filesystem and VCS index healthy at this scale. Mitigation strategies (e.g., sparse checkout, partial clone) to be evaluated per-provider beyond this scale.
6. **JSON library:** yyjson — fast for both read and write, C99, single dependency.
7. **UI framework:** Qt (LGPL 3.0) with mandatory dynamic linking. Custom `CommandStack` used for undo logic; Qt widgets used for rendering.
8. **Provider selection UX:** Auto-detect provider silently on open (no confirmation dialog). Active provider displayed persistently in the application title bar.
9. **Branch strategy UX:** Branch-first strong default. When the user is on the primary branch and makes any change, the application prompts them to create or select a working branch before proceeding. The prompt can be dismissed to stay on primary, but branching is the default path. Multiple branches can be open simultaneously. Stashing is supported for providers where `supports_stashing()` == true. The primary branch name is configured at project creation (provider suggests a default via `suggested_primary_branch_name()`) and stored in `manifest.json`. The `SourceControlProvider` interface exposes `get_workflow_metadata()` returning a `WorkflowMetadata` struct (branching model description + error recovery flows) and `suggested_primary_branch_name()`. The trigger rule for branch prompts is universal — any change on the primary branch — so no operation-specific trigger list is needed in the metadata.
10. **Authentication and identity model:** No application-level passwords. Each machine maintains a local profile (username + email) in OS app data, set up on first launch. This is the sole source of commit author identity across all projects on that machine. Philotechnia has no project-level user registry, no role system, and no application-layer access control — access control, where required, is enforced at the VCS hosting level (branch protection, repo permissions, signed commits). A team member's username is stamped into `Record.created_by` at record creation time and into the provider's commit author on every commit, giving a durable audit trail through the provider's history without any separate registry file in the repo.
11. **Centralized provider offline UX:** Editing is always permitted regardless of connectivity. For centralized providers (SVN, Perforce), the commit action is disabled when the network is unavailable, with a clear inline explanation. A persistent connectivity status indicator is shown in the source control panel whenever the active provider requires connectivity for commits. This pattern must be accommodated in the v1 UI groundwork even though centralized providers ship later.
12. **Git hosting compatibility (v1):** Explicit support and test targets: GitHub, GitLab, Gitea (self-hosted). Both SSH and HTTPS authentication required. Credentials delegated to the OS credential store (macOS Keychain, Windows Credential Manager, libsecret on Linux) via libgit2 credential callbacks — Philotechnia does not manage credentials directly. SSH key configuration (path to key, passphrase) available in application settings to handle cases where `ssh-agent` is not available.
13. **File attachment storage strategy:** The `attachment` field type stores a URI (URL or local file path) to an externally hosted resource. No binary data enters the project repo. Known limitation: URI references can become stale if the external resource moves or is deleted. A more integrated strategy may be revisited in a future milestone. The `attachment` field type is available in v1.
14. **Pull strategy (distributed providers):** Merge-on-pull is the v1 default. A pull is implemented as `git_remote_fetch` followed by a programmatic merge with manual index handling — the default checkout path that would write conflict markers to disk is never invoked. Each conflict is surfaced as three byte blobs (ancestor, local, incoming); JSON-aware field-level merge logic lives in the application core, not in the provider. Rebase-on-pull is deferred to a future milestone as a per-project or per-user setting. Centralized providers (SVN, Perforce) have no analogous choice — their pull semantics are determined by the provider's native update mechanism.
15. **Schema migration strategy:** Stateless and diff-based — no numbered migration scripts and no per-version upgrade code. The schema files on disk are the migration target; migration is the diff between a record's field set and the current schema. Reconciliation runs in-memory at record load time; disk is not touched until the normal commit pipeline. Missing fields receive declared defaults; removed fields move to `_deprecated_fields` on the record. `schema_version` in `manifest.json` is not used — the schema files are the authoritative state, and a fingerprint over them is computed when a cache key is needed. `format_version` is retained as the application-level file-format marker (see §7.2 "Format version compatibility"). A "purge deprecated fields" action (surfaced as a destructive confirmation dialog) is required for hard removal.
16. **Commit pipeline failure handling:** The commit-atomicity boundary is the VCS commit, not the filesystem batch. On any record-write failure the pipeline aborts before calling `stage()` or `commit()`, and in-memory `RecordWorkingCopy` state is not mutated by the write phase — so a retry re-runs the pipeline from unchanged in-memory state. Atomic rename makes re-writes idempotent. Partial on-disk writes after a crash are surfaced as pending changes via `get_pending_changes()` and cleaned up through the normal commit flow. `CommandStack::mark_clean()` is called only after `commit()` returns success. A preflight disk-space check is deferred as a UX improvement (candidate for M4).
17. **Project-open cache location and invalidation:** An mtime-based cache of parsed record content is stored per-machine and per-project under the OS app data directory (keyed by `project.id`) — never inside the project directory, so it is never committed to source control. Per-entry invalidation on mtime or size mismatch; whole-cache invalidation on `format_version` change, schema fingerprint change (computed over all files under `schema/`), or cache-format version change. The cache is written only after a successful commit, extending CLAUDE.md rule 9 beyond record and schema writes. Orphan caches for projects not opened in 90 days are removed on application startup. See §8.6 for structure and lifecycle details.
18. **Typed workflow-metadata handshake:** `RecoveryFlow.error_state` and `Step.action_id` in §8.4 are typed `enum class ErrorState` and `enum class ActionId` respectively, not `std::string`. Both enums are owned by the application core (`src/core/workflow.hpp`); providers depend on them. `ActionId` values resolve through a single dispatch table in `src/core/workflow.cpp` — UI widgets never `switch` on the value directly, which preserves CLAUDE.md rule 1 by keeping UI code free of provider-method knowledge. Rule-6 exhaustiveness is enforced by three CI-checked constituents, each landing with its dependencies: (1) at M1b, a unit test asserts every `ErrorState` value has a `RecoveryFlow` from some shipped provider's `WorkflowMetadata` or is listed in the narrow `kUnshippedErrorStates` escape hatch in `src/core/workflow.hpp` (v1 uses that escape hatch only for `CentralizedCommitBehind`, which is SVN/Perforce-only and is removed from the list when SvnProvider ships); (2) at M3–M4, a unit test asserts every `ErrorState` value has a renderer in the recovery UI, with the same exemption applied; (3) at M4, the dispatch function in `src/core/workflow.cpp` is a `switch` over every `ActionId` enumerator with no `default` case, making a missing dispatch entry a compile error. Together they express CLAUDE.md rule 6 — the application must guide the user out of every state it can create — as an enforceable invariant rather than a prose hope. Adding a new state or action therefore requires the corresponding dispatch, renderer, and provider-side `RecoveryFlow` (or an explicit `kUnshippedErrorStates` entry) in the same change.
19. **User identity key:** `LocalProfile.username` is the sole identity key across the system — stamped into `Record.created_by` at record creation time and supplied to the source control provider as the commit author name. There is no UUID-based user identity, no project-level user entity, and no second identity space. A consequence is that a future username-rename flow cannot be a local-only edit to the profile: it must also rewrite `created_by` across all records the user authored in every project that references the old name. A rename flow is not in v1 — usernames are treated as stable — and is deferred to a future milestone alongside the "purge deprecated fields" action.

---

## 14. Open Questions

1. ~~**Provider selection UX**~~ — resolved. See §13.
2. ~~**Branch strategy UX**~~ — resolved. See §13.
3. ~~**Commit identity fallback**~~ — resolved. The local profile (§5, §6.7, §13.10) is created on first launch and always provides the commit author identity. No fallback is needed.
4. ~~**Centralized provider offline UX**~~ — resolved. See §13.
5. ~~**Git hosting compatibility**~~ — resolved. See §13.

6. ~~**File attachment storage strategy**~~ — resolved. See §13.

7. ~~**SourceControlProvider workflow metadata extension**~~ — resolved. See §13.9 and §8.4. `get_workflow_metadata()` returns a `WorkflowMetadata` struct (branching model description + error recovery flows). `suggested_primary_branch_name()` supplies the provider's default primary branch name. Stashing is handled via `supports_stashing()` and associated methods. Branch prompt trigger is universal (any change on the primary branch) — no operation-specific trigger list required.

---

## 15. Milestones (Provisional)

| Milestone | Scope |
|-----------|-------|
| M0a — Infrastructure | Repo setup; CMake + Ninja build on macOS, Windows, Linux; vcpkg manifest mode with pinned dependencies; Qt 6 dynamic linking enforced in `CMakeLists.txt`; GitHub Actions build + test matrix; Catch2 wired to CTest; CPack scaffolding for `.dmg` / `.msi` / `.deb` / `.AppImage`. Detail: §15.1 |
| M0b — Storage Foundation | Hash-prefix directory structure (`records/{entity-type-id}/{xx}/`); atomic JSON read/write via yyjson (temp-file + atomic rename per CLAUDE.md rule 4, including Windows `FileRenameInfoEx` path); `manifest.json` schema and parser; orphaned `.tmp` recovery on project open; project creation and open flows (no VCS yet); local profile (username + email) first-launch setup and persistence under the OS app data directory — nothing reads it until M1a wires it into `GitProvider` as the commit author identity. Detail: §15.2 |
| M1a — Basic VCS | `SourceControlProvider` interface defined with capability flags, workflow metadata, and `std::expected<T, Error>` surface; `GitProvider` (libgit2) implementing initialise / clone / stage / commit / push / pull / branch management / history / stash; SSH + HTTPS auth via OS credential store (the remote-credential handling surface); local profile from M0b is wired into `GitProvider` as commit author name + email; provider auto-detect on project open |
| M1b — Conflict Handling | Throwaway prototype validating the libgit2 merge-without-checkout path against a scratch repo (pre-milestone de-risking); `get_conflicts()` / `resolve()` wired through `GitProvider`; programmatic three-way merge for conflict interception via `git_merge_trees` with manual index handling — never the default checkout path that would write conflict markers to disk; three-blob `Conflict` surface (ancestor / local / incoming) with all `Kind` variants (BothModified, BothAdded, ModifiedVsDeleted, DeletedVsModified); `GitProvider::get_workflow_metadata()` returns a `RecoveryFlow` for every git-applicable `ErrorState` — `DetachedHead`, `IncompleteMerge`, `RepositoryLocked`, `AuthenticationRequired`, `RemoteUnreachable`, `PushRejected`, `DirtyWorkingCopyOnBranchSwitch`, `DivergentBranches` (per CLAUDE.md rule 6); `CentralizedCommitBehind` is listed in `kUnshippedErrorStates` (removed from that list when SvnProvider ships); rule-6 exhaustiveness check (1) — `ErrorState` → `RecoveryFlow` coverage, see §8.4 — runs in CI from M1b onward; no UI yet, all flows exercised through integration tests |
| M2a — Core Engine | Schema engine (load, parse, fingerprint); record CRUD with stateless diff-based migration at load time (§7.2, decision #14); `RecordWorkingCopy`; `CommandStack` (CLAUDE.md rule 2, decision #5); concrete `Command`s — `CreateRecord`, `SetFieldValue`, `DeleteRecord` (§7.2 worked example shape); commit pipeline (§7.2) wiring atomic writes, `SourceControlProvider::stage` / `commit`, and `CommandStack::mark_clean`; `SchemaValidator` with all four pre-commit validation categories (§6.10, decision #15) — check logic + tests at M2a, commit-panel UI surfacing at M4. No query engine, no FTS, no cache; tests run against ≤1K records per type. Detail: §15.5 |
| M2b — Query, Search & Performance | Query engine (filter + sort, operator set in §15.6); `RecordIndex` for equality lookups on indexed fields; full-text search index targeting the §11 500 ms / 100K-value budget; mtime-based project-open cache (§8.6, decision #26) with per-entry and whole-cache invalidation; parallel async file loading using `std::async` thread pool sized `hardware_concurrency()` capped at 8 (decision #16); performance harness validating every §11 target on 10K-record fixtures. Cache write hook added to the M2a commit pipeline extends CLAUDE.md rule 9 beyond record + schema writes. Detail: §15.6 |
| M3 — UI Alpha | Table view, detail view, schema builder, undo/redo (CommandStack + Qt list widget) |
| M4 — Source Control UI | Pending changes list, pre-commit diff panel, commit panel (including inline rendering of `ValidationIssue` list and commit-disabled behavior per §6.10), push/pull controls, branch management, history view, conflict resolution UI, post-pull invalid-state banner and resolution flows — all via `SourceControlProvider` interface |
| M5 — Polish & Perf | Additional performance tuning beyond the §11 targets already met in M2b (profiling hotspots surfaced during M3/M4 UI work; FTS index persistence if rebuild cost becomes the bottleneck); network-filesystem documentation and any required accommodations; final packaging and installer polish; end-to-end testing across all three platforms |
| M6 — v1.0 Release | Stable release across macOS, Windows, Linux |

### 15.1 M0a deliverable detail

M0a produces no application behaviour — its deliverable is a repo that builds, tests, and packages an empty stub binary on all three target platforms. The concrete artefacts are:

**`vcpkg.json`** at the repo root, listing:

- `"name": "philotechnia"`, `"version-string": "0.0.0"`
- `"builtin-baseline"`: a specific vcpkg registry commit SHA pinning the entire dependency graph. Chosen once at M0a kickoff and bumped only in dedicated commits so that dependency-driven behaviour changes are bisectable (per the Dependency management convention in CLAUDE.md).
- `"dependencies"`: `qtbase`, `libgit2`, `yyjson`, `catch2`. Each entry may carry a `"version>="` constraint, but the baseline SHA is the authoritative pin.
- `"overrides"`: used only when a specific dependency must be held back or forward relative to the baseline; kept empty at M0a.

**Triplet policy.** CI and contributor builds use the dynamic vcpkg triplets only — `x64-windows`, `x64-osx`, `arm64-osx`, `x64-linux`, `arm64-linux`. The `-static` variants are prohibited because they would statically link Qt and violate CLAUDE.md rule 3 (Qt LGPL 3.0 dynamic-linking requirement). The root `CMakeLists.txt` asserts this both at the toolchain level (rejects any `VCPKG_TARGET_TRIPLET` ending in `-static`) and at the target level (see the Qt assertion below).

**Root `CMakeLists.txt`** with, at minimum:

- `cmake_minimum_required(VERSION 3.25)` — needed for `std::expected` toolchain support and modern Qt 6 integration.
- `project(philotechnia LANGUAGES CXX)`.
- `set(CMAKE_CXX_STANDARD 23)`; `set(CMAKE_CXX_STANDARD_REQUIRED ON)`; `set(CMAKE_CXX_EXTENSIONS OFF)`.
- `find_package(Qt6 COMPONENTS Core Widgets REQUIRED)` followed by a **Qt dynamic-linking assertion**:

  ```cmake
  get_target_property(_qt_core_type Qt6::Core TYPE)
  if(NOT _qt_core_type STREQUAL "SHARED_LIBRARY")
      message(FATAL_ERROR
          "Qt6::Core is linked as ${_qt_core_type}; Philotechnia requires "
          "dynamic Qt linkage under LGPL 3.0 (see CLAUDE.md rule 3). "
          "Use a dynamic vcpkg triplet (e.g. x64-windows, not x64-windows-static).")
  endif()
  ```

  This is non-negotiable and must be present from the first M0a commit. It is the executable form of CLAUDE.md rule 3.
- `find_package(unofficial-git2 CONFIG REQUIRED)`, `find_package(yyjson CONFIG REQUIRED)`, `find_package(Catch2 3 CONFIG REQUIRED)` — the exact package names follow vcpkg's canonical CMake config module names (note: vcpkg's `libgit2` port exports its CMake config as `unofficial-git2`, not `unofficial-libgit2`).
- `add_subdirectory(src)` and `add_subdirectory(tests)` — M0a ships empty stubs for both so the build wiring is exercised.
- `enable_testing()` + `include(CTest)` + `include(Catch)` (from Catch2); each test binary under `tests/` registers via `catch_discover_tests(...)`.
- `include(CPack)` with per-platform generator configuration: `DragNDrop` (macOS `.dmg`), `WIX` (Windows `.msi`), `DEB` + `External` (Linux `.deb` and `.AppImage`). Package metadata (name, vendor, description, version) centralised in one block near the top of the file so version bumps touch one place.

**GitHub Actions matrix** at `.github/workflows/ci.yml`:

- `jobs.build` with a matrix over `{ os: [macos-latest, windows-latest, ubuntu-latest] }`.
- Each job: checkout, set up vcpkg (`run-vcpkg` action or equivalent) pinned to the same `builtin-baseline` SHA, configure with `-DCMAKE_TOOLCHAIN_FILE` and the platform's dynamic triplet, build `Debug`, run `ctest --output-on-failure`.
- A separate `jobs.package` that builds `Release` and runs `cpack` to verify the installer generators succeed. The produced artefacts are uploaded for inspection but not published until M6.
- Workflow fails on any CMake warning in the Qt assertion block — the intent is that a static Qt build never passes CI.

**`tests/` scaffolding**: one `CMakeLists.txt` that adds a single placeholder test binary (`tests/smoke/smoke_test.cpp`) with one passing Catch2 `TEST_CASE` — just enough to prove the full path (configure → build → `ctest`) works on every matrix entry. Real unit tests land in M0b and later.

**Exit criteria.** Green CI on all three matrix entries; `cpack` produces a non-empty installer artefact per platform — `.dmg` on macOS, `.msi` on Windows, and both `.deb` and `.AppImage` on Linux; the Qt-linking assertion demonstrably fails the configure when a static triplet is forced in a local repro. No production code exists yet.

### 15.2 M0b deliverable detail

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

### 15.3 M1a deliverable detail

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

### 15.4 M1b deliverable detail

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

### 15.5 M2a deliverable detail

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

### 15.6 M2b deliverable detail

M2b adds query, full-text search, in-memory indexing, the mtime-based project-open cache, and parallel async file loading — the scale and performance work that takes M2a's correct-but-linear core to the §11 targets. Exit is a fully verifiable performance story at 10K records per type and 100K indexed field values.

**`src/core/query.hpp` / `.cpp`** — structured query surface:

- `struct QueryBuilder` with a fluent API: `from(EntityTypeId)`, `where(FieldId, Op, Value)`, `order_by(FieldId, Direction)`, `include_deleted(bool)`, `limit(size_t)`.
- `struct Query` — the built, immutable query.
- `std::vector<RecordId> execute(const Query&, const RecordManager&, const RecordIndex&)` — linear scan over the relevant `EntityTypeId`, with the `RecordIndex` providing an O(1) fast path for equality filters on indexed fields.
- Operator set: `Eq`, `Ne`, `Lt`, `Le`, `Gt`, `Ge`, `Contains` (strings), `Between` (numbers, datetimes), `IsNull`, `IsNotNull`.
- Unit tests per operator; combined-predicate tests; deleted-record filtering honours `include_deleted`.

**`src/core/record_index.hpp` / `.cpp`** — in-memory equality index:

- `class RecordIndex` — an equality index over fields marked `indexed: true` in the schema. The `indexed` field on `FieldDefinition` is an additive schema change landing at M2b; the default is unindexed, so M2a-era records and schemas remain valid.
- `std::span<const RecordId> lookup(EntityTypeId, FieldId, const Value&) const` — O(1) equality lookup.
- Maintenance: every `Command` that mutates an indexed field updates the index in `execute()` / `undo()` — still in-memory only per rule 9; no disk effect. The `RecordIndex` reference is injected into the `Command` ctor alongside the `RecordManager`.
- Unit tests: index correctness after CRUD sequences; index rebuild on project open; index invariant preserved across undo/redo/redo.

**`src/core/fts_index.hpp` / `.cpp`** — full-text search:

- `class FtsIndex` — inverted index over text-typed field values, in memory. Unicode-aware tokenisation, case-folding, a small English stop-word list (configurability deferred).
- `std::vector<RecordId> search(std::string_view query, EntityTypeId) const` — returns matches ranked by term frequency.
- Incremental update: every `SetFieldValue` on a text field posts an update — in-memory, rule 9 still holds.
- **Persistence is deliberately not part of M2b.** The FTS index is rebuilt from records on project open. Persisting the index is a future perf milestone if profiling shows the rebuild is a bottleneck at 100K values; at M2b, the §11 target of <500 ms for a full-text query over 100K values is met with an in-memory rebuild that runs as part of the parallel open walk.
- Performance test: FTS query over a synthetic 100K-value fixture completes in <500 ms on Linux CI hardware.

**`src/storage/project_open_parallel.hpp` / `.cpp`** — parallel async directory walk (decision #16, §8.5):

- `std::expected<ProjectOpenResult, Error> open_project_parallel(const std::filesystem::path&, const Schema&)` replaces M2a's serial record walk.
- Implementation: `std::async` thread pool sized `std::clamp(std::thread::hardware_concurrency(), 1u, 8u)` (handles the `hardware_concurrency() == 0` fallback). Coordinator thread walks `records/{entity-type-id}/{xx}/` directories; worker threads parse batches of files and return parsed `Record`s. Results accumulate on the coordinator thread — no Qt at this layer; the UI-thread handoff (`QMetaObject::invokeMethod` with `Qt::QueuedConnection`, §8.5) lands in M3 when there is a UI thread to hand off to.
- Per-file parse failures become `ValidationIssue`s in the `ProjectOpenResult`, not fatal errors — a project with one corrupt record still opens, preserving the backstop behaviour described in decision #15.
- Integration tests: parallel vs serial result equivalence over a 10K-record fixture; partial-parse-failure tolerance (one corrupt file does not fail the open); open time under §11 budget on Linux CI hardware (3 s cold on 10K records).

**`src/storage/open_cache.hpp` / `.cpp`** — mtime-based project-open cache (§8.6, decision #26):

- `class OpenCache` — per-machine, per-project cache of parsed record content, keyed by `project.id`, stored under OS app data (`~/Library/Application Support/Philotechnia/cache/{project-id}/` on macOS, `%LOCALAPPDATA%/Philotechnia/cache/{project-id}/` on Windows, `$XDG_CACHE_HOME/philotechnia/{project-id}/` on Linux with the usual fallback). Never inside the project directory, so never committed.
- Layout: one binary file per `EntityTypeId` holding parsed-record snapshots plus the `(mtime, size)` tuple captured at write time. A small manifest at cache root carries `last_opened_at`, `format_version`, and `schema_fingerprint`.
- Invalidation, mirroring decision #26: per-entry on mtime/size mismatch; whole-cache on `format_version` change, `schema_fingerprint` change, or cache-format version change.
- Write path: called from the commit pipeline after `provider.commit()` returns success — adds a single call site to `src/core/commit_pipeline.cpp` and is the M2b extension of CLAUDE.md rule 9 beyond record + schema writes. The write uses `atomic_write` from M0b.
- Orphan sweep on app startup removes caches whose `last_opened_at` is >90 days old.
- Unit tests: happy-path read hit; per-entry invalidation on mtime change; whole-cache invalidation on schema-fingerprint change; orphan sweep at the 90-day boundary; corrupted-cache files are treated as a miss, not a failure.
- Integration test: cold open of a 10K-record project with no cache → close → reopen (warm, cache hit) → warm open is <1 s on Linux CI hardware (§11 target).

**`src/storage/project_open.hpp` / `.cpp` (updated)** — the open orchestrator:

- `std::expected<ProjectOpenResult, Error> open_project(const std::filesystem::path&)` — keeps the M0b signature but routes internally: check the `OpenCache`; on hit, load parsed records from cache; on miss, run `open_project_parallel` and arrange for the cache to be written after the first successful commit (staying behind rule 9's "writes happen only in the commit pipeline" property).
- First-open behaviour (no cache yet, project never committed after opening): parallel parse runs, records load, cache is **not** written until a commit succeeds — matching decision #26. A subsequent commit triggers the cache write as part of the pipeline.

**Performance harness under `tests/perf/`.** A small benchmark driver generates synthetic projects at 1K, 10K, and 50K records per type, measures cold and warm open times, FTS latency, and write throughput, and prints a table. Runs on the Linux CI runner as a regression guard; thresholds are the §11 targets with a 50 % margin so ordinary CI noise does not flake the build.

**Exit criteria.** `ctest --output-on-failure` green on all three platforms including the `tests/perf/` gate on Linux. Cold open of a 10K-record project <3 s; warm open (cache hit) <1 s; FTS over 100K field values <500 ms; write throughput ≥500 records/sec — every §11 target passes on the CI runner hardware. Cache invalidation tests cover every scenario in §8.6 and decision #26. The `RecordIndex` and `FtsIndex` survive the full M2a undo/redo suite (every M2a test is re-run against the M2b stack to confirm no regression). No UI work done in M2b.
