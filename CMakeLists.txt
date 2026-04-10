cmake_minimum_required(VERSION 3.15)
project(mas2xm VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -pedantic)
endif()

add_executable(mas2xm
    src/main.c
    src/mas_read.c
    src/xm_write.c
    src/effects.c
    src/io_util.c
)

target_include_directories(mas2xm PRIVATE src)

# ----------------------------------------------------------------------
# Tests
# ----------------------------------------------------------------------
# Build the one-for-all roundtrip unit test by default. The test relies
# on `mas2xm.exe` being in `build/` (the same directory it produces) and
# on `mmutil.exe` being on PATH or under devkitPro. See test/TEST_TOOLS.md.

option(MAS2XM_BUILD_TESTS "Build the roundtrip unit test" ON)

if(MAS2XM_BUILD_TESTS)
    add_executable(roundtrip_test test/roundtrip_test.c)
    if(NOT MSVC)
        target_compile_options(roundtrip_test PRIVATE -Wno-format-truncation)
    endif()

    enable_testing()
    # Run the roundtrip test on the pinned reference inputs. CTest will
    # report PASS/FAIL based on the test's exit code.
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test/kuk.mas")
        # The .mas test fixtures are not directly usable by roundtrip_test
        # (which takes XM input), but they exist for the other inspectors.
    endif()

    # The full roundtrip test is gated on having a known XM input file.
    # Provide a CTest entry that runs the test against any *.xm passed
    # via -DMAS2XM_TEST_XM=<path>.
    if(DEFINED MAS2XM_TEST_XM AND EXISTS "${MAS2XM_TEST_XM}")
        add_test(NAME roundtrip
            COMMAND $<TARGET_FILE:roundtrip_test> "${MAS2XM_TEST_XM}"
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
    endif()
endif()
