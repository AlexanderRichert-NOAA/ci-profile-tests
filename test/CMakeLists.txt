cmake_minimum_required(VERSION 3.12)
project(multiprofiler_test C Fortran)

enable_testing()

# ---------------------------------------------------------------------------
# Profiling framework
#
# CI_MODE (ON): include Profiling.cmake directly from the sibling
# cmake/ directory.  Used by the repository's own CI workflow, which
# checks out the whole tree.
#
# CI_MODE (OFF, default): pull ci-multiprofiler via FetchContent,
# as any external project would.  Set GIT_REPOSITORY to your fork/mirror
# and GIT_TAG to a specific tag or SHA for reproducibility.
# ---------------------------------------------------------------------------
option(CI_MODE
    "Include Profiling.cmake from the local source tree (for repo-internal CI)"
    OFF)

if(CI_MODE)
    include("${CMAKE_CURRENT_LIST_DIR}/../cmake/Profiling.cmake")
else()
    include(FetchContent)
    FetchContent_Declare(
        ci_multiprofiler
        GIT_REPOSITORY https://github.com/AlexanderRichert-NOAA/ci-multiprofiler.git
        GIT_TAG        main
    )
    FetchContent_MakeAvailable(ci_multiprofiler)
    include("${ci_multiprofiler_SOURCE_DIR}/cmake/Profiling.cmake")
endif()

# ---------------------------------------------------------------------------
# Targets
# ---------------------------------------------------------------------------
add_executable(dummy_app main.c compute.c)
add_executable(sum_fortran sum.f90)

# ---------------------------------------------------------------------------
# Tests
#
# Multiple tests share executables but differ in their arguments, demonstrating
# that each test's profiling artifacts are stored in an isolated per-test
# directory and are never clobbered across runs.
# ---------------------------------------------------------------------------
add_test(NAME sort_small    COMMAND dummy_app   1000)
add_test(NAME sort_large    COMMAND dummy_app   5000)
add_test(NAME sum_fortran_small COMMAND sum_fortran 1000)
add_test(NAME sum_fortran_large COMMAND sum_fortran 5000)
