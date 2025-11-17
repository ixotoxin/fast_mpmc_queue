cmake_minimum_required(VERSION 3.25)

option(ENABLE_MEMORY_PROFILING "Enable memory profiling" OFF)

add_executable(test_mpsc test_mpsc_main.cpp)
target_compile_definitions(test_mpsc PRIVATE $<$<BOOL:${ENABLE_MEMORY_PROFILING}>:ENABLE_MEMORY_PROFILING>)

add_executable(test_mpmc test_mpmc_main.cpp)
target_compile_definitions(test_mpmc PRIVATE $<$<BOOL:${ENABLE_MEMORY_PROFILING}>:ENABLE_MEMORY_PROFILING>)

add_executable(test_mpmcdd test_mpmcdd_main.cpp)
target_compile_definitions(test_mpmcdd PRIVATE $<$<BOOL:${ENABLE_MEMORY_PROFILING}>:ENABLE_MEMORY_PROFILING>)

add_executable(test_mpmcsl test_mpmcsl_main.cpp)
target_compile_definitions(test_mpmcsl PRIVATE $<$<BOOL:${ENABLE_MEMORY_PROFILING}>:ENABLE_MEMORY_PROFILING>)

add_executable(test_fast_mpsc test_fast_mpsc_main.cpp)
target_compile_definitions(test_fast_mpsc PRIVATE $<$<BOOL:${ENABLE_MEMORY_PROFILING}>:ENABLE_MEMORY_PROFILING>)

add_executable(test_fast_mpmc test_fast_mpmc_main.cpp)
target_compile_definitions(test_fast_mpmc PRIVATE $<$<BOOL:${ENABLE_MEMORY_PROFILING}>:ENABLE_MEMORY_PROFILING>)

add_executable(test_fastest_mpmc test_fastest_mpmc_main.cpp)
target_compile_definitions(test_fastest_mpmc PRIVATE $<$<BOOL:${ENABLE_MEMORY_PROFILING}>:ENABLE_MEMORY_PROFILING>)

add_executable(test_fastest_mpsc test_fastest_mpsc_main.cpp)
target_compile_definitions(test_fastest_mpsc PRIVATE $<$<BOOL:${ENABLE_MEMORY_PROFILING}>:ENABLE_MEMORY_PROFILING>)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(
        EXECUTABLES
        test_mpsc test_mpmc test_mpmcdd test_mpmcsl
        test_fast_mpsc test_fast_mpmc
        test_fastest_mpsc test_fastest_mpmc
    )
else ()
    add_executable(stress_test_mpmc stress_test_mpmc_main.cpp)
    target_compile_definitions(stress_test_mpmc PRIVATE $<$<BOOL:${ENABLE_MEMORY_PROFILING}>:ENABLE_MEMORY_PROFILING>)
    set(
        EXECUTABLES
        test_mpsc test_mpmc test_mpmcdd test_mpmcsl
        test_fast_mpsc test_fast_mpmc
        test_fastest_mpsc test_fastest_mpmc
        stress_test_mpmc
    )
endif ()

if (WIN32)
    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        find_program(GCC_EXECUTABLE NAMES gcc g++)
        if(GCC_EXECUTABLE)
            get_filename_component(GCC_BIN_DIR "${GCC_EXECUTABLE}" DIRECTORY)
        endif()
    endif ()
    install(
        TARGETS ${EXECUTABLES}
        RUNTIME_DEPENDENCIES
        PRE_EXCLUDE_REGEXES "api-ms-" "ext-ms-"
        POST_EXCLUDE_REGEXES ".*system32/.*\\.dll"
        DIRECTORIES "${GCC_BIN_DIR}"
        DESTINATION bin
    )
else ()
    install(
        TARGETS ${EXECUTABLES}
        DESTINATION bin
    )
endif ()

if (BUILD_TESTS)
    add_test(NAME test_mpsc COMMAND test_mpsc)
    add_test(NAME test_mpmc COMMAND test_mpmc)
    add_test(NAME test_mpmcdd COMMAND test_mpmcdd)
    add_test(NAME test_mpmcsl COMMAND test_mpmcsl)
    add_test(NAME test_fast_mpsc COMMAND test_fast_mpsc)
    add_test(NAME test_fast_mpmc COMMAND test_fast_mpmc)
    add_test(NAME test_fastest_mpsc COMMAND test_fastest_mpsc)
    add_test(NAME test_fastest_mpmc COMMAND test_fastest_mpmc)
endif ()
