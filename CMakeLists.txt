cmake_minimum_required(VERSION 3.15)

# Suppress FetchContent deprecation warnings
if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)
endif()

project(fft_benchmark CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable optimizations for benchmarking
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -DNDEBUG")
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# ============================================================================
# CPM - CMake Package Manager (auto-downloads dependencies)
# ============================================================================
set(CPM_DOWNLOAD_VERSION 0.38.1)

if(CPM_SOURCE_CACHE)
    set(CPM_DOWNLOAD_LOCATION "${CPM_SOURCE_CACHE}/cpm/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
elseif(DEFINED ENV{CPM_SOURCE_CACHE})
    set(CPM_DOWNLOAD_LOCATION "$ENV{CPM_SOURCE_CACHE}/cpm/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
else()
    set(CPM_DOWNLOAD_LOCATION "${CMAKE_BINARY_DIR}/cmake/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
endif()

if(NOT (EXISTS ${CPM_DOWNLOAD_LOCATION}))
    message(STATUS "Downloading CPM.cmake to ${CPM_DOWNLOAD_LOCATION}")
    file(DOWNLOAD
        https://github.com/cpm-cmake/CPM.cmake/releases/download/v${CPM_DOWNLOAD_VERSION}/CPM.cmake
        ${CPM_DOWNLOAD_LOCATION}
    )
endif()

include(${CPM_DOWNLOAD_LOCATION})

set(CMAKE_OSX_ARCHITECTURES "arm64")

message(STATUS "Compiler ID: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "Compiler Version: ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Compiler Path: ${CMAKE_CXX_COMPILER}")
# ============================================================================
# Backend Options
# ============================================================================
option(ENABLE_FFTW "Enable FFTW backend" ON)
option(ENABLE_KISSFFT "Enable KissFFT backend" ON)
option(ENABLE_POCKETFFT "Enable PocketFFT backend" ON)
option(ENABLE_PFFFT "Enable PFFFT backend" ON)
option(ENABLE_KFR "Enable KFR backend" ON) # OFF by default (heavy dependency)
option(ENABLE_SIGNALSMITH "Enable Signalsmith backend" OFF)

# ============================================================================
# Source files
# ============================================================================
set(SOURCES src/fft_benchmark.cpp)
set(HEADERS
    inc/fft_backend.h
    inc/fftw_backend.h
    inc/kissfft_backend.h
    inc/pocketfft_backend.h
    inc/pffft_backend.h
    # inc/kfr_backend.h
    inc/other_backends.h
)

include_directories(${CMAKE_SOURCE_DIR}/inc)

add_executable(fft_benchmark ${SOURCES} ${HEADERS})
target_include_directories(fft_benchmark PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# ============================================================================
# Dependencies via CPM
# ============================================================================

# FFTW - Try system install first, otherwise skip (needs separate installation)
if(ENABLE_FFTW)
    set(FFTW_PREFIX /opt/homebrew/Cellar/fftw/3.3.10_2)

    set(FFTW3_INCLUDE_DIR ${FFTW_PREFIX}/include)
    set(FFTW3F_LIBRARY ${FFTW_PREFIX}/lib/libfftw3f.dylib)

    if(EXISTS ${FFTW3_INCLUDE_DIR} AND EXISTS ${FFTW3F_LIBRARY})
        message(STATUS "✓ FFTW found at ${FFTW_PREFIX}")
        target_include_directories(fft_benchmark PRIVATE ${FFTW3_INCLUDE_DIR})
        target_link_libraries(fft_benchmark PRIVATE ${FFTW3F_LIBRARY})
        target_compile_definitions(fft_benchmark PRIVATE HAVE_FFTW)
    else()
        message(FATAL_ERROR "FFTW not found at ${FFTW_PREFIX}")
    endif()
endif()

# KissFFT - Header-only with separate implementation
if(ENABLE_KISSFFT)
    CPMAddPackage(
        NAME kissfft
        GITHUB_REPOSITORY mborgerding/kissfft
        GIT_TAG master
        OPTIONS
        "KISSFFT_DATATYPE float"
        "KISSFFT_STATIC ON"
    )

    if(kissfft_ADDED)
        message(STATUS "✓ KissFFT fetched via CPM")
        target_link_libraries(fft_benchmark PRIVATE kissfft::kissfft)
        target_compile_definitions(fft_benchmark PRIVATE HAVE_KISSFFT)
    endif()
endif()

# PocketFFT - Header-only
if(ENABLE_POCKETFFT)
    CPMAddPackage(
        NAME pocketfft
        GITHUB_REPOSITORY mreineck/pocketfft
        GIT_TAG cpp
        DOWNLOAD_ONLY YES
    )

    if(pocketfft_ADDED)
        message(STATUS "✓ PocketFFT fetched via CPM")
        target_include_directories(fft_benchmark PRIVATE ${pocketfft_SOURCE_DIR})
        target_compile_definitions(fft_benchmark PRIVATE HAVE_POCKETFFT)
    endif()
endif()

# PFFFT - Pretty Fast FFT (download only, we'll compile directly)
if(ENABLE_PFFFT)
    CPMAddPackage(
        NAME pffft
        GITHUB_REPOSITORY marton78/pffft
        GIT_TAG master
        DOWNLOAD_ONLY YES
    )

    if(pffft_ADDED)
        message(STATUS "✓ PFFFT fetched via CPM")

        # Compile PFFFT sources directly (avoids CMake issues in greenffts subdir)
        add_library(pffft_local STATIC
            ${pffft_SOURCE_DIR}/pffft.c
            ${pffft_SOURCE_DIR}/pffft_double.c
        )
        target_include_directories(pffft_local PUBLIC ${pffft_SOURCE_DIR})

        target_link_libraries(fft_benchmark PRIVATE pffft_local)
        target_include_directories(fft_benchmark PRIVATE ${pffft_SOURCE_DIR})
        target_compile_definitions(fft_benchmark PRIVATE HAVE_PFFFT)
    endif()
endif()

# KFR - DSP Framework (large dependency, disabled by default)
# Before fetching KFR
# if(ENABLE_KFR)
#     set(CMAKE_OSX_ARCHITECTURES "x86_64")
#     set(KFR_ARCH "avx2" CACHE STRING "" FORCE)
#     set(KFR_ENABLE_MULTIARCH OFF CACHE BOOL "" FORCE)
#     set(KFR_ENABLE_DETECT_CPU OFF CACHE BOOL "" FORCE)

#     CPMAddPackage(
#         NAME kfr
#         GITHUB_REPOSITORY kfrlib/kfr
#         GIT_TAG 6.0.2
#     )

#     if(kfr_ADDED)
#         message(STATUS "✓ KFR fetched via CPM")
#         target_link_libraries(fft_benchmark PRIVATE kfr kfr_dft)
#         target_compile_definitions(fft_benchmark PRIVATE HAVE_KFR)
#     endif()
# endif()

if(ENABLE_SIGNALSMITH)
    CPMAddPackage(
        NAME dsp
        GITHUB_REPOSITORY Signalsmith-Audio/dsp
        GIT_TAG main
        DOWNLOAD_ONLY YES
    )

    if(dsp_ADDED)
        message(STATUS "✓ Signalsmith fetched via CPM")
        # Include the "include" folder, not the repo root
        target_include_directories(fft_benchmark PRIVATE ${dsp_SOURCE_DIR}/include)
        target_compile_definitions(fft_benchmark PRIVATE HAVE_SIGNALSMITH)
    endif()
endif()

# ============================================================================
# Platform-specific settings
# ============================================================================
if(UNIX AND NOT APPLE)
    target_link_libraries(fft_benchmark PRIVATE pthread m)
endif()

# ============================================================================
# Summary
# ============================================================================
message(STATUS "")
message(STATUS "FFT Benchmark Configuration:")
message(STATUS "=============================")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "")
message(STATUS "Enabled backends:")

if(FFTW3F_FOUND OR FFTW3F_LIBRARY)
    message(STATUS "  ✓ FFTW")
else()
    message(STATUS "  ✗ FFTW (not found)")
endif()

if(TARGET kissfft::kissfft)
    message(STATUS "  ✓ KissFFT")
else()
    message(STATUS "  ✗ KissFFT")
endif()

if(pocketfft_ADDED)
    message(STATUS "  ✓ PocketFFT")
else()
    message(STATUS "  ✗ PocketFFT")
endif()

if(pffft_ADDED)
    message(STATUS "  ✓ PFFFT")
else()
    message(STATUS "  ✗ PFFFT")
endif()

if(kfr_ADDED)
    message(STATUS "  ✓ KFR")
else()
    message(STATUS "  ✗ KFR")
endif()

if(dsp_ADDED)
    message(STATUS "  ✓ Signalsmith")
else()
    message(STATUS "  ✗ Signalsmith")
endif()

message(STATUS "")
message(STATUS "To enable/disable backends, use:")
message(STATUS "  cmake -DENABLE_FFTW=ON -DENABLE_KFR=OFF ..")
message(STATUS "")
