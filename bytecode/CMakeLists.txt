# Part of the Haxonite project, under the MIT License.
# Copyright 2025 Derek Noonburg

include_directories("${PROJECT_SOURCE_DIR}/util")

add_library(bytecode
  BytecodeDefs.cpp
  BytecodeEngine.cpp
  BytecodeFile.cpp
  Heap.cpp
)

add_executable(bcasm bcasm.cpp)
target_link_libraries(bcasm bytecode)

add_executable(bclink bclink.cpp)
target_link_libraries(bclink bytecode)

add_executable(bcdisasm bcdisasm.cpp)
target_link_libraries(bcdisasm bytecode)

add_executable(bcrun bcrun.cpp)
target_link_libraries(bcrun bytecode util)

