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

add_executable(test_dfmpmcq test_dfmpmcq_main.cpp)
target_compile_definitions(test_dfmpmcq PRIVATE $<$<BOOL:${ENABLE_MEMORY_PROFILING}>:ENABLE_MEMORY_PROFILING>)

add_executable(test_dfmpscq test_dfmpscq_main.cpp)
target_compile_definitions(test_dfmpscq PRIVATE $<$<BOOL:${ENABLE_MEMORY_PROFILING}>:ENABLE_MEMORY_PROFILING>)

add_executable(test_sfmpmcq test_sfmpmcq_main.cpp)
target_compile_definitions(test_sfmpmcq PRIVATE $<$<BOOL:${ENABLE_MEMORY_PROFILING}>:ENABLE_MEMORY_PROFILING>)

add_executable(test_sfmpscq test_sfmpscq_main.cpp)
target_compile_definitions(test_sfmpscq PRIVATE $<$<BOOL:${ENABLE_MEMORY_PROFILING}>:ENABLE_MEMORY_PROFILING>)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(
        EXECUTABLES
        test_mpsc test_mpmc test_mpmcdd test_mpmcsl
        test_dfmpmcq test_dfmpscq test_sfmpmcq test_sfmpscq
    )
else ()
    add_executable(stress_test_mpmc stress_test_mpmc_main.cpp)
    target_compile_definitions(stress_test_mpmc PRIVATE $<$<BOOL:${ENABLE_MEMORY_PROFILING}>:ENABLE_MEMORY_PROFILING>)
    set(
        EXECUTABLES
        test_mpsc test_mpmc test_mpmcdd test_mpmcsl
        test_dfmpmcq test_dfmpscq test_sfmpmcq test_sfmpscq
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
    add_test(NAME test_dfmpmcq COMMAND test_dfmpmcq)
    add_test(NAME test_dfmpscq COMMAND test_dfmpscq)
    add_test(NAME test_sfmpmcq COMMAND test_sfmpmcq)
    add_test(NAME test_sfmpscq COMMAND test_sfmpscq)
endif ()
