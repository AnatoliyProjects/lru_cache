# Header-only C++ implementation of LRU cache with all the gems.
# Features: thread safety, serialization, memory monitoring, statistics, etc.
# SPDX-FileCopyrightText: Copyright © 2024 Anatoly Petrov <petrov.projects@gmail.com>
# SPDX-License-Identifier: MIT

# CMake for lru_cache/test/lru_cache_test.cpp

cmake_minimum_required(VERSION 3.29)
project(lru_cache_gtest)

add_executable(lru_cache_gtest ${CMAKE_CURRENT_LIST_DIR}/lru_cache_test.cpp)
target_compile_features(lru_cache_gtest PUBLIC cxx_std_20)
target_include_directories(lru_cache_gtest PUBLIC ${CMAKE_CURRENT_LIST_DIR}/../include)

enable_testing()

if(NOT DEFINED GTEST_DIR)
    set(GTEST_DIR "${CMAKE_CURRENT_LIST_DIR}/../../googletest")
    message(WARNING "-D GTEST_DIR is not set.\nDefault: '${GTEST_DIR}'")
endif()

if (NOT EXISTS ${GTEST_DIR})
    set(MSG "gtest is not found at: '${GTEST_DIR}'.\n"
            "Use -D GTEST_DIR=... to specify the appropriate dir.")
    message(FATAL_ERROR ${MSG})
endif()

set(BUILD_GMOCK ON)  # for EXPECT_THAT and matchers
set(INSTALL_GTEST OFF)
add_subdirectory(${GTEST_DIR} ${CMAKE_CURRENT_BINARY_DIR}/cmake-build-gtest)
target_link_libraries(lru_cache_gtest PUBLIC GTest::gtest_main)
target_link_libraries(lru_cache_gtest PUBLIC GTest::gmock_main)

include(GoogleTest)
gtest_discover_tests(lru_cache_gtest)
