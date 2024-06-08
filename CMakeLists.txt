cmake_minimum_required(VERSION 3.18.4)

project("Temperature and humidity monitor")
add_definitions(-DPROJECT_NAME="${PROJECT_NAME}")

# Always generate compile_commands.json for clangd, etc.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_C_STANDARD 17)

set(CMAKE_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include(cmake/compiler-flags.cmake)
include(cmake/sanitizers.cmake)

# Make __FILE__ relative to project root directory instead of absolute directory of the build OS.
add_compile_options(-ffile-prefix-map=${CMAKE_SOURCE_DIR}=.)

add_subdirectory(src)
