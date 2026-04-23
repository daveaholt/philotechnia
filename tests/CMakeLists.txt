# -----------------------------------------------------------------------------
# tests/ — Catch2 unit and integration tests
# -----------------------------------------------------------------------------
# M0a: a single smoke test that proves the configure → build → ctest path
# on every platform. Real unit tests (tests/core/, tests/storage/,
# tests/vcs/) land from M0b onward per the planned structure in CLAUDE.md.

add_subdirectory(smoke)
