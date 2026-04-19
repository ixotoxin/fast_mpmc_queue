cmake_minimum_required(VERSION 3.25)

option(ENABLE_MEMORY_PROFILING "Enable memory profiling" OFF)

add_executable(test_mpscq test_mpscq_main.cpp)
target_compile_definitions(test_mpscq PRIVATE $<$<BOOL:${ENABLE_MEMORY_PROFILING}>:ENABLE_MEMORY_PROFILING>)

add_executable(test_mpmcq test_mpmcq_main.cpp)
target_compile_definitions(test_mpmcq PRIVATE $<$<BOOL:${ENABLE_MEMORY_PROFILING}>:ENABLE_MEMORY_PROFILING>)

add_executable(test_mpmcqdd test_mpmcqdd_main.cpp)
target_compile_definitions(test_mpmcqdd PRIVATE $<$<BOOL:${ENABLE_MEMORY_PROFILING}>:ENABLE_MEMORY_PROFILING>)

add_executable(test_mpmcqsl test_mpmcqsl_main.cpp)
target_compile_definitions(test_mpmcqsl PRIVATE $<$<BOOL:${ENABLE_MEMORY_PROFILING}>:ENABLE_MEMORY_PROFILING>)

add_executable(test_dfmpscq test_dfmpscq_main.cpp)
target_compile_definitions(test_dfmpscq PRIVATE $<$<BOOL:${ENABLE_MEMORY_PROFILING}>:ENABLE_MEMORY_PROFILING>)

add_executable(test_dfmpmcq test_dfmpmcq_main.cpp)
target_compile_definitions(test_dfmpmcq PRIVATE $<$<BOOL:${ENABLE_MEMORY_PROFILING}>:ENABLE_MEMORY_PROFILING>)

add_executable(test_sfmpscq test_sfmpscq_main.cpp)
target_compile_definitions(test_sfmpscq PRIVATE $<$<BOOL:${ENABLE_MEMORY_PROFILING}>:ENABLE_MEMORY_PROFILING>)

add_executable(test_sfmpmcq test_sfmpmcq_main.cpp)
target_compile_definitions(test_sfmpmcq PRIVATE $<$<BOOL:${ENABLE_MEMORY_PROFILING}>:ENABLE_MEMORY_PROFILING>)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(
        EXECUTABLES
        test_mpscq test_mpmcq test_mpmcqdd test_mpmcqsl
        test_dfmpscq test_dfmpmcq test_sfmpscq test_sfmpmcq
    )
else ()
    add_executable(stress_test_mpmcq stress_test_mpmcq_main.cpp)
    target_compile_definitions(stress_test_mpmcq PRIVATE $<$<BOOL:${ENABLE_MEMORY_PROFILING}>:ENABLE_MEMORY_PROFILING>)
    set(
        EXECUTABLES
        test_mpscq test_mpmcq test_mpmcqdd test_mpmcqsl
        test_dfmpscq test_dfmpmcq test_sfmpscq test_sfmpmcq
        stress_test_mpmcq
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
    add_test(NAME test_mpscq COMMAND test_mpscq)
    add_test(NAME test_mpmcq COMMAND test_mpmcq)
    add_test(NAME test_mpmcqdd COMMAND test_mpmcqdd)
    add_test(NAME test_mpmcqsl COMMAND test_mpmcqsl)
    add_test(NAME test_dfmpmcq COMMAND test_dfmpmcq)
    add_test(NAME test_dfmpscq COMMAND test_dfmpscq)
    add_test(NAME test_sfmpmcq COMMAND test_sfmpmcq)
    add_test(NAME test_sfmpscq COMMAND test_sfmpscq)
endif ()
