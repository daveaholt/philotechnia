# -----------------------------------------------------------------------------
# src/ — application sources
# -----------------------------------------------------------------------------
# M0a: a placeholder main.cpp that prints build metadata and exits. Real
# subdirectories (core/, storage/, vcs/, ui/, commands/) land in M0b and
# beyond per the planned directory structure in CLAUDE.md.

add_executable(philotechnia
    main.cpp
)

target_link_libraries(philotechnia
    PRIVATE
        Qt6::Core
        Qt6::Widgets
)

# Pass version metadata to the binary so `--version` / log output is honest
# across builds.
target_compile_definitions(philotechnia
    PRIVATE
        PHILOTECHNIA_VERSION="${PROJECT_VERSION}"
)

# On Windows, link as a GUI application so no console window opens alongside
# the UI. Qt provides a Windows entry-point wrapper.
if(WIN32)
    set_target_properties(philotechnia PROPERTIES WIN32_EXECUTABLE ON)
endif()

# Install rule feeds CPack.
install(TARGETS philotechnia
    RUNTIME DESTINATION bin
    BUNDLE  DESTINATION .
)
