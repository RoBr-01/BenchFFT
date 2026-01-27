cmake_minimum_required(VERSION 3.21)

# ---- ccache (optional) ----
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    message(STATUS "ccache found!")
    set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
endif()

project(CPPTemplate VERSION 0.0.1 LANGUAGES CXX)

message(STATUS "Compiler ID: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "Compiler Version: ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Compiler Path: ${CMAKE_CXX_COMPILER}")

include(cmake/CPM.cmake)

# ---- language ----
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---- preset / build-type naming ----
if(DEFINED ENV{CMAKE_PRESET_NAME})
    set(PRESET_NAME $ENV{CMAKE_PRESET_NAME})
elseif(DEFINED CMAKE_BUILD_TYPE)
    set(PRESET_NAME ${CMAKE_BUILD_TYPE})
else()
    set(PRESET_NAME default)
endif()

set(TARGET_NAME ${PROJECT_NAME}_${PRESET_NAME})

# ---- sources & target ----
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cpp)
add_executable(${TARGET_NAME} ${SOURCES})

target_include_directories(${TARGET_NAME}
    PRIVATE
    inc
    lib
)

# ---- warnings (Debug only) ----
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Enabling strong warnings + sanitizers")

    target_compile_options(${TARGET_NAME} PRIVATE
        -O0 -g
        -Wall -Wextra -Wpedantic
        -Wshadow -Wconversion -Wdouble-promotion
        -Wnon-virtual-dtor -Wold-style-cast
        -Wcast-align -Woverloaded-virtual
        -Wnull-dereference -Wmisleading-indentation
        -Wfloat-equal -Wswitch-enum -Wundef
        -Wformat=2 -Wduplicated-cond
        -Wlogical-op -Warray-bounds
    )

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        message(STATUS "Clang|GNU detected")

        target_compile_options(${TARGET_NAME} PRIVATE
            -fsanitize=address,undefined,bounds,pointer-compare,pointer-subtract
            -fno-omit-frame-pointer
            -fsanitize-recover=undefined
        )
        target_link_options(${TARGET_NAME}  PRIVATE
            -fsanitize=address,undefined,bounds,pointer-compare,pointer-subtract
            -fno-omit-frame-pointer
            -fsanitize-recover=undefined
        )
    endif()
endif()

# ---- aggressive Release tuning ----
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    message(STATUS "Enabling optimisations")
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(${TARGET_NAME} PRIVATE
            -O3
            -flto
            -fno-rtti
            -fno-exceptions
            -ffast-math
            -DNDEBUG
        )
    endif()
endif()
