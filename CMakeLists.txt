# Header-only C++ implementation of LRU cache with all the gems.
# Features: thread safety, serialization, memory monitoring, statistics, etc.
# SPDX-FileCopyrightText: Copyright © 2024 Anatoly Petrov <petrov.projects@gmail.com>
# SPDX-License-Identifier: MIT

# Project CMake

cmake_minimum_required(VERSION 3.29)
project(lru_cache)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/example)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/test)