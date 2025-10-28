cmake_minimum_required(VERSION 3.25)

include(FetchContent)

option(INSTALL_GTEST "Enable installation of GoogleTest" OFF)

message(STATUS "Fetch GoogleTest")

FetchContent_Declare(
    googletest
    GIT_REPOSITORY  https://github.com/google/googletest.git
    GIT_TAG         main
)

FetchContent_MakeAvailable(googletest)

add_library(GTest::GTest INTERFACE IMPORTED)
target_link_libraries(GTest::GTest INTERFACE gtest_main)

add_executable(test_lib_fast_queue fast_queue.cpp)
target_link_libraries(test_lib_fast_queue GTest::gtest GTest::gtest_main)
add_test(NAME test_lib_fast_queue COMMAND test_lib_fast_queue)
