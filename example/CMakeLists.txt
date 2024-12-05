# Header-only C++ implementation of LRU cache with all the gems.
# Features: thread safety, serialization, memory monitoring, statistics, etc.
# SPDX-FileCopyrightText: Copyright © 2024 Anatoly Petrov <petrov.projects@gmail.com>
# SPDX-License-Identifier: MIT

# CMake for lru_cache/example/main.cpp

cmake_minimum_required(VERSION 3.29)
project(lru_cache_example)

add_executable(lru_cache_example ${CMAKE_CURRENT_LIST_DIR}/main.cpp)
target_compile_features(lru_cache_example PUBLIC cxx_std_20)
target_include_directories(lru_cache_example PUBLIC ${CMAKE_CURRENT_LIST_DIR}/../include)
