# Handoff — M0a: Infrastructure

*Derived from `docs/philotechnia_spec.md` §15.1. If this document and the spec disagree, the spec wins.*

---

## Mission

Produce a repository that builds, tests, and packages an empty stub binary on macOS, Windows, and Linux. No application behaviour ships in this milestone — the deliverable is entirely build-system, dependency-pinning, CI-matrix, and installer scaffolding. A successful M0a means any subsequent milestone can add code against a known-good build.

**Predecessor:** none (bootstrap milestone). **Successor:** M0b — Storage Foundation.

---

## Foundation

### Project in one paragraph

Philotechnia is a cross-platform C++ desktop application for structured record management. Projects are version-controlled directories of plain JSON files (one file per record, hash-prefix directories). The application includes a first-party source control UI built on an abstract `SourceControlProvider` interface — Git via libgit2 ships in v1; SVN and Perforce are later milestones. The UI is Qt. JSON is yyjson. All undo/redo and dirty-state tracking flows through a custom `CommandStack`, not Qt's built-in undo framework.

### Non-negotiable architectural rules (from CLAUDE.md)

These are hard constraints. Do not work around them — raise a question if one creates a conflict.

1. **The UI layer must never depend on a concrete `SourceControlProvider` implementation.** All source control calls from UI or application core go through the abstract `SourceControlProvider` interface only. Never `#include` a provider header from UI code.

2. **`CommandStack` owns all undo/redo logic. Qt does not.** `QUndoStack` is not used. Qt's `QListView` renders the history list by binding to `CommandStack::history()`, but the stack itself is a custom C++ class in the application core. Dirty state, clean marking, and commit lifecycle live in `CommandStack` and `RecordWorkingCopy` — not in any Qt class.

3. **Qt must be dynamically linked.** Static linking of Qt is prohibited under LGPL 3.0. All three platform builds ship Qt as shared libraries (`.dylib` / `.dll` / `.so`). This must be enforced in `CMakeLists.txt` from M0a onward.

4. **All record writes use atomic rename.** Write to a temp file, then rename over the target. On POSIX use `rename(2)`. On Windows use `SetFileInformationByHandle` with `FileRenameInfoEx` and the flags `FILE_RENAME_FLAG_POSIX_SEMANTICS | FILE_RENAME_FLAG_REPLACE_IF_EXISTS` — this gives POSIX-equivalent atomic replacement (available since Windows 10 build 14393; the platform minimum of 19041 guarantees availability). Do **not** use the older `MoveFileExW`. Never write directly to the final path.

5. **Hand-written ASM only with a profiled bottleneck.** C++23 is the default for everything. Hand-written assembly or inline `asm` blocks require a measured, reproducible performance regression to justify them. SIMD via intrinsics or `std::simd` is regular C++ and does not require this justification.

6. **The application must be fully self-sufficient for all source control states it creates.** If Philotechnia can put a repository into a state — conflict, detached HEAD, lock, authentication challenge, incomplete merge — it must be able to guide the user out of that state. No user should ever need an external git, SVN, or Perforce client to resolve a condition the application created.

7. **The source control UI is provider-aware and guides users toward correct workflows.** UI widgets emit `ActionId` values to the central dispatcher (`src/core/workflow.cpp`) and never `switch` on `ActionId` themselves.

8. **Schema invariants are enforced at commit time, not discovered at pull time.** The `SchemaValidator` runs in the commit pipeline and must reject any commit that would produce schema, record, or referential inconsistencies.

9. **Commands mutate only in-memory state; disk I/O happens exclusively in the commit pipeline.** `Command::execute()` and `Command::undo()` update `RecordWorkingCopy`, schema working state, or `CommandStack` bookkeeping — never the project files on disk.

*(Rules 4, 6, 7, 8, 9 are forward-looking at M0a — they constrain later milestones but must not be contradicted by any build-system or CI decision taken here. Rules 3 and 5 are immediately active in M0a.)*

### Tech stack (from §9)

| Layer | Technology | Notes |
|-------|-----------|-------|
| Language | C++23 | `CMAKE_CXX_STANDARD 23`, `CMAKE_CXX_EXTENSIONS OFF` |
| JSON | yyjson | Single library for all JSON read/write |
| UI | Qt 6 (LGPL 3.0) | **Must be dynamically linked — see rule 3** |
| Source control v1 | libgit2 | vcpkg port name `libgit2`; CMake config module `unofficial-git2` |
| Build | CMake + Ninja | Minimum CMake 3.25 |
| Testing | Catch2 (v3) | Header pair; registers via `catch_discover_tests` |
| CI | GitHub Actions | Matrix across macOS, Windows, Linux |

### Platform targets (§8.2)

| Platform | Minimum version | Notes |
|----------|----------------|-------|
| macOS    | 13 (Ventura)   | ARM + Intel universal binary |
| Windows  | 10 build 19041 | x86-64 |
| Linux    | Ubuntu 22.04 LTS | x86-64; other distros best-effort |

---

## Spec cross-references

### §8.3 UI Framework (verbatim)

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

---

## Relevant resolved decisions (from `docs/decisions.md`)

| # | Decision | Bearing on M0a |
|---|----------|----------------|
| 4 | **UI framework:** Qt 6 (LGPL 3.0). Must be dynamically linked. | Drives the Qt assertion in root `CMakeLists.txt` and the dynamic-only triplet policy. |
| 7 | **Assembly / low-level code:** C++23 is the default. Hand-written ASM requires a profiled, reproducible bottleneck. | Constrains toolchain / compiler-flag choices to stay in portable C++. |
| 8 | **Installer strategy:** CMake + CPack. Artifacts: `.dmg` (macOS), `.msi` (Windows), `.deb` / `.AppImage` (Linux). | Defines the CPack generator matrix (`DragNDrop`, `WIX`, `DEB` + `External`). |
| 10 | **Licensing:** Source-available. Specific instrument (BSL, FSL, or custom) TBD before first public release. | No license file to commit at M0a beyond a placeholder; revisit for M6. |

---

## Deliverable detail (§15.1, verbatim)

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

---

## What M0b adds next

M0b builds on the green matrix by adding `src/storage/` (atomic-write, hash-prefix path helpers, orphan-tempfile sweep, yyjson wrapper) and `src/core/` surface for `manifest.json` + project create/open + the machine-local `LocalProfile`. No source control yet — M1a wires `GitProvider`. The `tests/smoke/` binary you ship in M0a stays long-term as the minimal end-to-end build-and-run guard.

---

## Source docs

- `docs/philotechnia_spec.md` §8.2, §8.3, §9, §15.1, §13 (decisions 4, 7, 8, 10)
- `docs/decisions.md` rows 4, 7, 8, 10
- `CLAUDE.md` — rules 3 and 5 active in this milestone
