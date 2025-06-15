cmake_minimum_required(VERSION 3.12)

project(quart)

set(CMAKE_CXX_STANDARD 20)

set(NEEDED_LLVM_VERSION 20)
find_package(LLVM CONFIG REQUIRED)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")

if(LLVM_VERSION_MAJOR LESS NEEDED_LLVM_VERSION)
    message(FATAL_ERROR "Found LLVM Major version ${LLVM_VERSION_MAJOR} but need ${NEEDED_LLVM_VERSION}.")
endif()

file(GLOB_RECURSE SOURCES quart/*.cpp)
add_executable(quart ${SOURCES})

target_include_directories(quart PRIVATE . ${LLVM_INCLUDE_DIRS})

target_compile_options(quart PRIVATE -Wall -Wextra -Wno-redundant-move -Wno-unused-variable -Wno-reorder -Wno-switch -Wno-unused-parameter -Wno-non-pod-varargs)
target_compile_options(quart PRIVATE -g -fno-exceptions)

target_compile_definitions(quart PRIVATE QUART_PATH="${CMAKE_SOURCE_DIR}/lib")
target_compile_definitions(quart PRIVATE ${LLVM_DEFINITIONS})

target_link_options(quart PRIVATE -g)

target_link_directories(quart PRIVATE ${LLVM_LIBRARY_DIRS})
target_link_libraries(quart PRIVATE LLVM)