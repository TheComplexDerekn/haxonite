# Part of the Haxonite project, under the MIT License.
# Copyright 2025 Derek Noonburg

include_directories("${PROJECT_SOURCE_DIR}/bytecode")
include_directories("${PROJECT_SOURCE_DIR}/runtime")
include_directories("${PROJECT_SOURCE_DIR}/util")
include_directories("${PROJECT_BINARY_DIR}/util")
include_directories(${FREETYPE_INCLUDE_DIRS})

add_executable(haxrun
  haxrun.cpp
  Hash.cpp
  runtime_alloc.cpp
  runtime_datetime.cpp
  runtime_File.cpp
  runtime_format.cpp
  runtime_gfx.cpp
  runtime_Map.cpp
  runtime_math.cpp
  runtime_random.cpp
  runtime_regex.cpp
  runtime_serdeser.cpp
  runtime_Set.cpp
  runtime_String.cpp
  runtime_StringBuf.cpp
  runtime_system.cpp
  runtime_Vector.cpp
  CairoXCBGfx.cpp
)
target_link_libraries(haxrun bytecode util
                      cairo png jpeg fontconfig double-conversion pcre2-8 icuuc
                      xcb-icccm xcb-shm xcb-xkb xcb-randr xcb xkbcommon-x11 xkbcommon
)

add_executable(hax hax.cpp)
target_link_libraries(hax util)
