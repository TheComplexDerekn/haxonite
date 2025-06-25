# Part of the Haxonite project, under the MIT License.
# Copyright 2025 Derek Noonburg

cmake_minimum_required(VERSION 3.10)

project(haxonite)

enable_language(CXX)
set(CMAKE_CXX_STANDARD 17)

# There are apparently no standard FindXXX.cmake files for:
# - cairo
# - pcre

# This sets ${FREETYPE_INCLUDE_DIRS}, which is needed for Cairo.
find_package(Freetype REQUIRED)

find_package(PNG REQUIRED)
find_package(JPEG REQUIRED)
find_package(Fontconfig REQUIRED)
find_package(double-conversion REQUIRED)
find_package(X11 REQUIRED)

add_subdirectory(bytecode)
add_subdirectory(compiler)
add_subdirectory(runtime)
add_subdirectory(util)
