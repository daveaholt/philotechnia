// Philotechnia — M0a smoke test.
//
// A single Catch2 test case used to prove the toolchain + build + ctest
// pipeline works end-to-end on every target platform. This file is a
// scaffolding placeholder and will be removed or replaced once real unit
// tests arrive under tests/core/, tests/storage/, etc. (M0b onward).

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Catch2 is wired up", "[smoke]") {
    REQUIRE(1 + 1 == 2);
}
