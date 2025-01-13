project(quart)

cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 20)

set(LLVM_VERSION 17)

find_package(LLVM ${LLVM_VERSION} REQUIRED)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")

include_directories(${LLVM_INCLUDE_DIRS})
link_directories(${LLVM_LIBRARY_DIRS})
link_libraries(LLVM)
add_definitions(${LLVM_DEFINITIONS})

llvm_map_components_to_libnames(libraries core)

file(GLOB_RECURSE SOURCES quart/*.cpp)

include_directories(.)

add_compile_options(-Wall -Wextra -Wno-redundant-move -Wno-unused-variable -Wno-reorder -Wno-switch -Wno-unused-parameter -Wno-non-pod-varargs)
add_compile_options(-O3 -fno-exceptions -g)

add_link_options(-O3 -g -rdynamic)

add_compile_definitions(QUART_PATH="${CMAKE_SOURCE_DIR}/lib")

add_executable(quart ${SOURCES})
target_link_libraries(quart ${libraries})