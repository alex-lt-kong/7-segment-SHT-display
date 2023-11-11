cmake_minimum_required(VERSION 3.18.4)

project("Temperature and humidity monitor")
add_definitions(-DPROJECT_NAME="${PROJECT_NAME}")

# Always generate compile_commands.json for clangd, etc.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_C_STANDARD 11)

set(CMAKE_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include(cmake/compiler-flags.cmake)
include(cmake/sanitizers.cmake)

add_subdirectory(src)
