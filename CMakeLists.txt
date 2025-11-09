cmake_minimum_required(VERSION 3.31)
project(axenc)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")

set(CMAKE_CXX_COMPILER_LAUNCHER ccache)

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM: ${LLVM_PACKAGE_VERSION}")

file(GLOB_RECURSE COMPILER_SRC_FILES "${CMAKE_SOURCE_DIR}/src/*.cpp")

add_executable(axenc ${COMPILER_SRC_FILES})

target_include_directories(axenc PRIVATE ${LLVM_INCLUDE_DIRS})
target_include_directories(axenc PRIVATE ${CMAKE_SOURCE_DIR}/include)

target_compile_definitions(axenc PRIVATE ${LLVM_DEFINITIONS})

target_link_libraries(axenc PRIVATE LLVM)
