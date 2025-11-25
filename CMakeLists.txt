cmake_minimum_required(VERSION 3.31)
project(axenc)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")

set(CMAKE_CXX_COMPILER_LAUNCHER ccache)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Building with AddressSanitizer")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer -g")
    set(CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} -fsanitize=address")
endif()

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM: ${LLVM_PACKAGE_VERSION}")

file(GLOB_RECURSE LIB_SRC_FILES "${CMAKE_SOURCE_DIR}/src/*.cpp")
add_library(axenlang ${LIB_SRC_FILES})
target_include_directories(axenlang PUBLIC ${LLVM_INCLUDE_DIRS})
target_include_directories(axenlang PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_compile_definitions(axenlang PUBLIC ${LLVM_DEFINITIONS})
target_link_libraries(axenlang PUBLIC LLVM)

file(GLOB_RECURSE AXENC_SRC_FILES "${CMAKE_SOURCE_DIR}/compiler/*.cpp")
add_executable(axenc ${AXENC_SRC_FILES})
target_link_libraries(axenc PRIVATE axenlang)

file(GLOB_RECURSE AXENLSP_SRC_FILES "${CMAKE_SOURCE_DIR}/lsp/*.cpp")
add_executable(axenlsp ${AXENLSP_SRC_FILES})
target_link_libraries(axenlsp PRIVATE axenlang)
