cmake_minimum_required(VERSION 3.25)

# -----------------------------------------------------------------------------
# Project metadata
# -----------------------------------------------------------------------------
project(philotechnia
    VERSION 0.0.0
    DESCRIPTION "Cross-platform desktop application for version-controlled structured record management."
    HOMEPAGE_URL "https://github.com/daveaholt/philotechnia"
    LANGUAGES CXX
)

# -----------------------------------------------------------------------------
# Language standard
# -----------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Respect user-provided CMAKE_BUILD_TYPE; default to Debug for uninitialised
# single-config generators.
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# -----------------------------------------------------------------------------
# vcpkg triplet policy (enforces CLAUDE.md rule 3 at the toolchain level)
# -----------------------------------------------------------------------------
# Only dynamic vcpkg triplets are supported. Static triplets would cause Qt to
# link statically, violating the LGPL 3.0 requirement.
if(DEFINED VCPKG_TARGET_TRIPLET AND VCPKG_TARGET_TRIPLET MATCHES "-static$")
    message(FATAL_ERROR
        "VCPKG_TARGET_TRIPLET='${VCPKG_TARGET_TRIPLET}' ends in '-static'. "
        "Philotechnia requires dynamic Qt linkage under LGPL 3.0 "
        "(see CLAUDE.md rule 3). Use a dynamic triplet: "
        "x64-windows / x64-osx / arm64-osx / x64-linux / arm64-linux.")
endif()

# -----------------------------------------------------------------------------
# Qt autogen settings
# -----------------------------------------------------------------------------
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# -----------------------------------------------------------------------------
# Dependencies (via vcpkg manifest mode — see vcpkg.json)
# -----------------------------------------------------------------------------
find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

# -----------------------------------------------------------------------------
# Qt dynamic-linking assertion (CLAUDE.md rule 3)
# -----------------------------------------------------------------------------
# This is a hard configure-time failure if Qt6::Core was resolved as a static
# library. It is the executable form of CLAUDE.md rule 3 and must remain in
# place from the first M0a commit onward. See spec §15.1.
get_target_property(_qt_core_type Qt6::Core TYPE)
if(NOT _qt_core_type STREQUAL "SHARED_LIBRARY")
    message(FATAL_ERROR
        "Qt6::Core is linked as ${_qt_core_type}; Philotechnia requires "
        "dynamic Qt linkage under LGPL 3.0 (see CLAUDE.md rule 3). "
        "Use a dynamic vcpkg triplet (e.g. x64-windows, not x64-windows-static).")
endif()

# libgit2, yyjson, Catch2 v3
find_package(unofficial-git2 CONFIG REQUIRED)
find_package(yyjson CONFIG REQUIRED)
find_package(Catch2 3 CONFIG REQUIRED)

# -----------------------------------------------------------------------------
# Testing
# -----------------------------------------------------------------------------
enable_testing()
include(CTest)
include(Catch)

# -----------------------------------------------------------------------------
# Subdirectories
# -----------------------------------------------------------------------------
add_subdirectory(src)
add_subdirectory(tests)

# -----------------------------------------------------------------------------
# Packaging (CPack)
# -----------------------------------------------------------------------------
set(CPACK_PACKAGE_NAME             "Philotechnia")
set(CPACK_PACKAGE_VENDOR           "Philotechnia")
set(CPACK_PACKAGE_VERSION          "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
set(CPACK_PACKAGE_HOMEPAGE_URL     "${PROJECT_HOMEPAGE_URL}")
set(CPACK_PACKAGE_CONTACT          "Philotechnia maintainers")

if(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
elseif(WIN32)
    set(CPACK_GENERATOR "WIX")
    # WIX requires a stable upgrade GUID. Replace with a real GUID before the
    # first public release (reference: https://www.guidgenerator.com/).
    set(CPACK_WIX_UPGRADE_GUID "00000000-0000-0000-0000-000000000001")
elseif(UNIX)
    # Linux produces both .deb (DEB generator) and .AppImage (External
    # generator). The AppImage path is driven by a CPack External script
    # that calls linuxdeploy + linuxdeploy-plugin-qt against the staged
    # install tree — see packaging/linux/cpack_appimage.cmake.in and
    # spec §15.1.
    set(CPACK_GENERATOR "DEB;External")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "${CPACK_PACKAGE_CONTACT}")

    # .desktop and icon install rules — Linux only. macOS (DragNDrop) and
    # Windows (WIX) have their own bundling stories.
    install(FILES      "${CMAKE_SOURCE_DIR}/packaging/linux/philotechnia.desktop"
            DESTINATION share/applications)
    install(FILES      "${CMAKE_SOURCE_DIR}/packaging/linux/philotechnia.png"
            DESTINATION share/icons/hicolor/256x256/apps)

    # Resolve qmake for linuxdeploy-plugin-qt. The plugin needs a qmake
    # binary to locate Qt's libraries and plugin directories when
    # assembling the AppDir. Qt6::qmake is the canonical imported target;
    # we fall back to PATH lookup if it is not present.
    if(TARGET Qt6::qmake)
        get_target_property(PHILOTECHNIA_QMAKE_EXECUTABLE
            Qt6::qmake IMPORTED_LOCATION)
    endif()
    if(NOT PHILOTECHNIA_QMAKE_EXECUTABLE)
        find_program(PHILOTECHNIA_QMAKE_EXECUTABLE NAMES qmake6 qmake)
    endif()
    if(NOT PHILOTECHNIA_QMAKE_EXECUTABLE)
        message(WARNING
            "qmake not found; linuxdeploy-plugin-qt will attempt to locate Qt "
            "on its own during cpack. If AppImage packaging fails, install Qt "
            "or set QMAKE in the cpack environment.")
    endif()

    # Bake the qmake path and project metadata into the CPack External
    # script. The script runs in a fresh CMake context at cpack time and
    # cannot see the main project's scope directly.
    configure_file(
        "${CMAKE_SOURCE_DIR}/packaging/linux/cpack_appimage.cmake.in"
        "${CMAKE_BINARY_DIR}/cpack_appimage.cmake"
        @ONLY
    )

    set(CPACK_EXTERNAL_ENABLE_STAGING ON)
    set(CPACK_EXTERNAL_PACKAGE_SCRIPT "${CMAKE_BINARY_DIR}/cpack_appimage.cmake")
endif()

include(CPack)
