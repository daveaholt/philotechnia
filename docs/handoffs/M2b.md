# Handoff — M2b: Query, Search, Index, Parallel Open, Open Cache

*Derived from `docs/philotechnia_spec.md` §15.6. If this document and the spec disagree, the spec wins.*

---

## Mission

Turn M2a's correct-but-linear data plane into one that hits the §11 scale targets at 10K records per entity type and 100K indexed text values. Land the query surface, the equality `RecordIndex`, the in-memory full-text index (`FtsIndex`), the parallel `std::async`-based project-open pipeline, and the mtime-based project-open cache — writing the cache only as a commit-pipeline side effect so CLAUDE.md rule 9 extends cleanly over it. A performance harness runs on the Linux CI runner as a regression guard. No UI work happens in M2b; every deliverable is exercised through unit, integration, and perf tests against the M2a engine.

**Predecessor:** M2a — schema engine, record CRUD, `CommandStack`, three concrete commands, commit pipeline, `SchemaValidator`. **Successor:** M3 — UI alpha (table views, schema editor, `CommandStack` history binding).

---

## Foundation

### Project in one paragraph

Philotechnia is a cross-platform C++ desktop application for structured record management. Projects are version-controlled directories of plain JSON files (one file per record, hash-prefix directories). The application includes a first-party source control UI built on an abstract `SourceControlProvider` interface — Git via libgit2 ships in v1; SVN and Perforce are later milestones. The UI is Qt. JSON is yyjson. All undo/redo and dirty-state tracking flows through a custom `CommandStack`, not Qt's built-in undo framework.

### Non-negotiable architectural rules (from CLAUDE.md)

Rules active in M2b:

1. **The UI layer must never depend on a concrete `SourceControlProvider` implementation.** Here the rule manifests as a *core / UI boundary* rather than a provider boundary: the parallel-open worker pool, `RecordIndex`, and `FtsIndex` all live in `src/core/` or `src/storage/` and must compile without any `Qt*` header. The `QMetaObject::invokeMethod` handoff described in §8.5 is deliberately deferred to M3 when the UI thread exists — M2b ships a headless orchestrator.
4. **All record writes use atomic rename.** The cache write lands as a new call site inside `src/core/commit_pipeline.cpp` and uses the M0b `atomic_write`. The cache file is JSON/binary per the `OpenCache` format; it is not a record file, but it is on-disk state and therefore follows the same write discipline.
9. **Commands mutate only in-memory state; disk I/O happens exclusively in the commit pipeline.** The cache write is the M2b extension of rule 9. It is **only** performed after a successful `provider.commit()` — never at project open, never at project close, never as a side effect of `RecordIndex` or `FtsIndex` mutation. This is the single-source-of-truth property that makes rule 9 a bisectable discipline rather than a sentiment.

Rules 2, 3, 5, 6, 7, 8 are background: rule 2 (CommandStack ownership) was enforced at M2a; rule 3 (Qt dynamic) at M0a; rule 5 is passively observed; rule 6's second CI check lands with the UI at M3–M4; rule 7 also rides the UI milestone; rule 8 (SchemaValidator at commit) is already active and is not touched by M2b.

### Tech stack slice

| Layer | Technology | Where it lands |
|-------|-----------|----------------|
| Async | `std::async` + manual thread pool, cap 8 | `src/storage/project_open_parallel.cpp` |
| JSON I/O | yyjson via `json_io` from M0b | `src/storage/open_cache.cpp` (cache format), `record.cpp` (record parse) |
| Atomic write | `atomic_write` from M0b | cache write call site in `commit_pipeline.cpp` |
| Indexing | Hand-rolled in-memory hash index; inverted-index FTS | `src/core/record_index.{hpp,cpp}`, `src/core/fts_index.{hpp,cpp}` |
| Testing | Catch2 unit + integration; custom perf harness | `tests/core/`, `tests/storage/`, `tests/perf/` |

### Performance targets (§11, verbatim)

| Metric | Target | Notes |
|--------|--------|-------|
| Cold start time | < 500 ms on reference hardware | Excludes project open time |
| Project open (10K records, local SSD) | < 3 seconds | Directory walk + per-file JSON parse; parallel async I/O should be used to mitigate syscall overhead |
| Project open (subsequent opens) | < 1 second | mtime-based cache skips unchanged files; only modified files are re-parsed |
| Record list render (10,000 records) | < 16 ms per frame (60 fps) | Operates on in-memory index, not disk |
| Full-text search response | < 500 ms on 100K text field values | Operates on in-memory index |
| Write throughput | ≥ 500 record writes/second (single writer) | One JSON file per record + atomic rename per write |

**Network filesystem warning (verbatim).** Projects opened from a network-mounted path (NFS, SMB/CIFS, network drives) will have significantly higher open times due to per-file I/O latency. 10K files over a typical LAN could take 30–60+ seconds on first open. This is a known limitation of the one-file-per-record design and should be communicated clearly in the documentation. The recommended practice is to keep the working copy on local storage and use the source control provider for sharing.

### Storage format (§7.1 summary)

Project = directory with `manifest.json`, `schema/...`, `records/{entity-type-id}/{xx}/{record-uuid}.json` (hash-prefix dirs). The hash-prefix structure keeps any single directory under ~200 files at the 50K-record scale (decision #9) and is what makes the serial directory walk already fast enough to stay single-threaded in §8.5 — only the per-file parse is parallelised.

---

## Spec cross-references

### §6.3 Query & Filter (verbatim)

- Filter records by any field value, with multi-condition AND/OR logic
- Sort by any field, ascending or descending
- Save named filter presets per entity type
- Full-text search across all text fields within an entity type

(Saved presets are a UI concern and land at M3+ against this milestone's programmatic `Query` API. M2b ships the engine behind the presets, not the presets themselves.)

### §8.5 Async I/O and Threading Model (verbatim)

The project-open performance target (< 3 seconds for 10K records on local SSD — see §11) requires parallelising the directory walk and per-file JSON parse. The threading approach is as follows:

**Directory walk:** A single thread performs the hash-prefix directory walk and enqueues file paths onto a work queue. Because `records/` is organised into at most ~200-file subdirectories by the hash-prefix structure, the walk itself is fast and does not need parallelising.

**Parallel parse:** A thread pool (sized to `std::thread::hardware_concurrency()`, capped at a reasonable maximum such as 8) drains the work queue. Each worker reads a file, calls yyjson to parse it, and inserts the resulting in-memory record into a thread-safe portion of the index. Insertion uses per-entity-type locks or a concurrent data structure to avoid a single global lock becoming a bottleneck.

**UI-thread bridge:** The in-memory index populated by the parse workers is not the same object as the `QAbstractItemModel` rendered by the table view — the model is a UI-thread view over the index and must be mutated only from the UI thread. Workers therefore never touch any Qt object. As each worker completes its chunk, it posts a completion batch (the record IDs it inserted, or the first parse error it encountered) back to the UI thread via `QMetaObject::invokeMethod(target, ..., Qt::QueuedConnection)`, where `target` is a long-lived `QObject` that lives on the UI thread and holds a reference to the model. The queued invocation runs inside the Qt event loop; its handler asks the model to emit the appropriate `rowsInserted` (or `dataChanged`) signal for the newly-visible range, and surfaces any parse failure as an inline error state in the project-open UI. Batching at worker-chunk granularity — not per-file — keeps UI-thread wakeups bounded regardless of record count. The same pattern applies to any later background work that needs to update the UI (e.g., incremental full-text index population, background commit pipeline status reporting): `std::async` worker plus `QMetaObject::invokeMethod(target, ..., Qt::QueuedConnection)` is the project's canonical UI-handoff idiom. `QFutureWatcher` and `QtConcurrent` are deliberately not used — staying on `std::async` keeps the thread-pool ownership inside the application core rather than inside Qt, and keeps the `SourceControlProvider` / core / storage layers free of any Qt dependency (preserving CLAUDE.md rule 1's separation of concerns between core and UI).

**Platform async I/O:** For the v1 implementation, `std::async` / `std::future` with a manually managed thread pool is sufficient and avoids introducing a platform-specific I/O framework dependency. If profiling reveals that syscall overhead (rather than parse time) is the bottleneck at scale, platform-native async I/O (Linux `io_uring`, Windows IOCP, macOS `kqueue`) can be added as a future optimisation behind an abstraction layer. This is deferred until profiled evidence justifies the complexity.

**Write path:** Record writes do not use the thread pool — they are synchronous on the calling thread, using the atomic write-to-temp-then-rename pattern described in §7.1. Concurrent writes from different UI actions are serialised by the `CommandStack`, so parallel writes from application code are not a concern in v1.

### §8.6 Project-open cache (verbatim)

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

---

## Relevant resolved decisions (from `docs/decisions.md`)

| # | Decision | Bearing on M2b |
|---|----------|----------------|
| 9 | **Large dataset ceiling** — 50,000 records per entity type for v1. Hash-prefix structure keeps filesystem and VCS index healthy at this scale. Sparse checkout / partial clone to be evaluated per-provider beyond this scale. | 50K is the outer envelope for the perf harness; 10K is the §11-stated gate. 50K fixtures live in the `tests/perf/` top-end runs. |
| 16 | **Async I/O approach** — `std::async` thread pool for v1 project open (parallelise directory walk + JSON parse). Platform-native I/O deferred pending profiling evidence. | Dictates `project_open_parallel.cpp`'s pool type. No `io_uring`/IOCP/`kqueue` in v1. |
| 26 | **Project-open cache** — mtime-based, per-project UUID, under OS app data. Per-entry invalidation on mtime/size mismatch; whole-cache invalidation on `format_version` change, schema fingerprint change, or cache-format version change. Written only after successful `commit()` — extends rule 9. Orphan caches older than 90 days removed on app startup. | The central decision behind `OpenCache`. Matches the invalidation matrix in §8.6 and the write-on-commit-only discipline. |

---

## Deliverable detail (§15.6, verbatim)

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

---

## What M3 adds next

M3 lands the UI alpha: Qt 6 main window, project chrome (open/close/active-provider indicator), a `QTableView`-backed record grid wired to a `QAbstractItemModel` that reads from the M2a `RecordManager` + M2b `RecordIndex`, the schema editor, and the `QListView` binding against `CommandStack::history()` (decision #4, CLAUDE.md rule 2). The §8.5 `QMetaObject::invokeMethod(..., Qt::QueuedConnection)` UI-thread bridge finally has a UI thread to hand off to — the M2b parallel-open coordinator is extended with a per-chunk callback that M3 routes through that bridge. Rule 6's second CI check (renderer coverage for every `ErrorState`) also lands in the M3–M4 window as the source control UI takes shape.

---

## Source docs

- `docs/philotechnia_spec.md` §6.3, §8.5, §8.6, §11, §15.6
- `docs/decisions.md` rows 9, 16, 26
- `CLAUDE.md` — rules 1, 4, 9
