# Philotechnia

A high-performance, cross-platform desktop application for structured record management in team environments. Every project is a version-controlled directory of plain JSON files — diffable, portable, and ownable without a vendor.

> **Status: Pre-implementation (design phase).** The architecture and tech stack are settled; code does not exist yet. See [`docs/philotechnia_spec.md`](docs/philotechnia_spec.md) for the full specification.

---

## What it is

Philotechnia gives teams a schema-driven record system — think internal databases, entity registries, or operational playbooks — backed by source control for collaboration, history, and conflict resolution. Unlike SaaS alternatives, all data lives in open JSON files on disk, in a directory the team fully controls.

Key properties:

- **Structured records.** Users define entity types with typed fields and user-defined enums; records are validated against those schemas.
- **Source-control-native.** The application includes a first-party source control UI built on an abstract `SourceControlProvider` interface. Git (libgit2) ships as the v1 provider; SVN and Perforce are planned.
- **Offline-first (distributed providers).** Full functionality without network access when using git; only push/pull require connectivity.
- **One file per record.** Each record is its own JSON file in a hash-prefix directory structure — concurrent edits to different records never produce merge conflicts.
- **Human-readable storage.** Every file is inspectable and processable without the application.

---

## Tech stack

| Layer | Technology |
|-------|-----------|
| Language | C++23 |
| JSON | yyjson |
| Source control (v1) | libgit2 |
| UI | Qt 6 (LGPL 3.0, dynamically linked) |
| Build | CMake + Ninja |
| Testing | Catch2 |
| CI | GitHub Actions |

---

## Building

**Prerequisites:**
- CMake ≥ 3.25
- Ninja
- A C++23-capable compiler (clang-17+ or MSVC 19.35+ or GCC 13+)
- vcpkg (used in manifest mode; all C++ dependencies — Qt 6, libgit2, yyjson, Catch2 — are installed from the repo's `vcpkg.json`)

### Dependency baseline

The vcpkg registry commit is pinned in two coupled places and must always be kept in sync:

1. `vcpkg.json` → `"builtin-baseline"`
2. `.github/workflows/ci.yml` → `env.VCPKG_COMMIT` (same value)

The current baseline is `c3867e714dd3a51c272826eea77267876517ed99` — vcpkg release tag [`2026.03.18`](https://github.com/microsoft/vcpkg/releases/tag/2026.03.18). When bumping the baseline, pick a commit from [microsoft/vcpkg releases](https://github.com/microsoft/vcpkg/releases) (typically a release tag), update both fields, and land the change as its own commit so that dependency-driven behaviour changes are bisectable (see the Dependency management convention in `CLAUDE.md`).

Only the dynamic vcpkg triplets are supported: `x64-windows`, `x64-osx`, `arm64-osx`, `x64-linux`, `arm64-linux`. The `-static` variants are rejected at configure time because they would statically link Qt in violation of LGPL 3.0 (see `CLAUDE.md` rule 3 and spec §15.1).

### Debug build and tests

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_TARGET_TRIPLET=<your-triplet>
cmake --build build
cd build && ctest --output-on-failure
```

### Platform installers (release builds)

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_TARGET_TRIPLET=<your-triplet>
cmake --build build
cd build && cpack
```

Produces `.dmg` (macOS), `.msi` (Windows), and both `.deb` and `.AppImage` (Linux). The `.AppImage` is built by CPack's `External` generator via `packaging/linux/cpack_appimage.cmake.in`, which downloads and invokes `linuxdeploy` + `linuxdeploy-plugin-qt` against the staged install tree. `APPIMAGE_EXTRACT_AND_RUN=1` is set so the build works without FUSE (e.g., in the GitHub Actions Linux runner).

---

## Repository structure

```
philotechnia/
  .github/workflows/       # CI matrix (build + test + package on mac/win/linux)
  docs/
    philotechnia_spec.md   # Full project specification (start here)
    decisions.md           # Quick-reference of resolved architectural decisions
  src/                     # C++ source (M0a stub; real modules land from M0b)
    main.cpp               # Entry point
  tests/                   # Catch2 unit and integration tests
    smoke/                 # M0a pipeline smoke test
  CMakeLists.txt           # Root build file
  vcpkg.json               # Pinned C++ dependency manifest
  .gitignore
  CLAUDE.md                # AI assistant handoff — architecture rules and invariants
  README.md                # This file
```

---

## License

Source-available. The specific instrument (Business Source License, Functional Source License, or custom) will be selected before the first public release. See [`docs/philotechnia_spec.md`](docs/philotechnia_spec.md) §13 for context.
