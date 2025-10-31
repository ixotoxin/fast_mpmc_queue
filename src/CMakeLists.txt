cmake_minimum_required(VERSION 3.25)

add_executable(test_mpsc test_mpsc_main.cpp)
add_executable(test_mpmc test_mpmc_main.cpp)
add_executable(test_fast_mpsc test_fast_mpsc_main.cpp)
add_executable(test_fast_mpmc test_fast_mpmc_main.cpp)
add_executable(mpmc_stress_test mpmc_stress_test.cpp)

if (WIN32)
    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        find_program(GCC_EXECUTABLE NAMES gcc g++)
        if(GCC_EXECUTABLE)
            get_filename_component(GCC_BIN_DIR "${GCC_EXECUTABLE}" DIRECTORY)
        endif()
    endif ()
    install(
        TARGETS test_mpsc test_mpmc test_fast_mpsc test_fast_mpmc mpmc_stress_test
        RUNTIME_DEPENDENCIES
        PRE_EXCLUDE_REGEXES "api-ms-" "ext-ms-"
        POST_EXCLUDE_REGEXES ".*system32/.*\\.dll"
        DIRECTORIES "${GCC_BIN_DIR}"
        DESTINATION bin
    )
else ()
    install(
        TARGETS test_mpsc test_mpmc test_fast_mpsc test_fast_mpmc mpmc_stress_test
        DESTINATION bin
    )
endif ()

if (BUILD_TESTS)
    add_test(NAME test_mpsc COMMAND test_mpsc)
    add_test(NAME test_mpmc COMMAND test_mpmc)
    add_test(NAME test_fast_mpsc COMMAND test_fast_mpsc)
    add_test(NAME test_fast_mpmc COMMAND test_fast_mpmc)
endif ()
