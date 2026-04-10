cmake_minimum_required(VERSION 3.15)
project(mas2xm VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -pedantic)
endif()

# ----------------------------------------------------------------------
# Core library
# ----------------------------------------------------------------------
# Everything in src/ except main.c is packaged as a static library so that
# both the mas2xm CLI and the test inspectors that poke at Module internals
# (check_vib, trace_write) can link against the same object code.

add_library(mas2xm_core STATIC
    src/mas_read.c
    src/xm_write.c
    src/effects.c
    src/io_util.c
)
target_include_directories(mas2xm_core PUBLIC src)

# ----------------------------------------------------------------------
# Main executable
# ----------------------------------------------------------------------

add_executable(mas2xm src/main.c)
target_link_libraries(mas2xm PRIVATE mas2xm_core)

# ----------------------------------------------------------------------
# Tests
# ----------------------------------------------------------------------
# All test executables land under build/test/ so they do not pollute the
# source tree. batch_test.sh picks them up from that location.

option(MAS2XM_BUILD_TESTS "Build the test inspectors and roundtrip harness" ON)

if(MAS2XM_BUILD_TESTS)
    set(TEST_OUTPUT_DIR "${CMAKE_BINARY_DIR}/test")

    # Standalone tests: each is a single .c file that links only against
    # libc. They inspect .xm or .mas files directly without going through
    # the mas2xm_core parser.
    set(STANDALONE_TESTS
        check_all_samp
        check_inst
        check_inst2
        check_samp
        check_xm
        cmp_orders
        cmp_patt
        count_hdr
        debug_dump
        full_cmp
        quick_orders
        roundtrip_test
        trace_patt
    )

    foreach(t ${STANDALONE_TESTS})
        add_executable(${t} test/${t}.c)
        set_target_properties(${t} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${TEST_OUTPUT_DIR}")
        if(NOT MSVC)
            # roundtrip_test uses snprintf heavily and trips
            # -Wformat-truncation on GCC; the truncation is intentional.
            target_compile_options(${t} PRIVATE -Wno-format-truncation)
        endif()
    endforeach()

    # Tests that link against mas2xm_core because they reuse the MAS
    # reader to walk a Module struct in memory.
    set(CORE_LINKED_TESTS
        check_vib
        trace_write
    )

    foreach(t ${CORE_LINKED_TESTS})
        add_executable(${t} test/${t}.c)
        target_link_libraries(${t} PRIVATE mas2xm_core)
        # check_vib/trace_write #include "src/types.h" style paths, so
        # they need the project root on the include path in addition to
        # src/ (which mas2xm_core already exposes).
        target_include_directories(${t} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
        set_target_properties(${t} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${TEST_OUTPUT_DIR}")
    endforeach()

    # CTest entry for the roundtrip harness. Runs only when an XM fixture
    # is supplied via -DMAS2XM_TEST_XM=<path>.
    enable_testing()
    if(DEFINED MAS2XM_TEST_XM AND EXISTS "${MAS2XM_TEST_XM}")
        add_test(NAME roundtrip
            COMMAND $<TARGET_FILE:roundtrip_test> "${MAS2XM_TEST_XM}"
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
    endif()
endif()
