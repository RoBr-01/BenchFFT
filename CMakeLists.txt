cmake_minimum_required(VERSION 3.15)

# Suppress FetchContent deprecation warnings
if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)
endif()

project(fft_benchmark
    VERSION 1.0.0
    LANGUAGES CXX C
    DESCRIPTION "FFT Library Benchmark Suite"
)

# ============================================================================
# Build Configuration
# ============================================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Default to Release build for performance
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ============================================================================
# Global Optimization Flags (applied to ALL backends)
# ============================================================================

# Clear any existing flags to avoid duplication
set(CMAKE_C_FLAGS "")
set(CMAKE_CXX_FLAGS "")
set(CMAKE_C_FLAGS_RELEASE "")
set(CMAKE_CXX_FLAGS_RELEASE "")
set(CMAKE_C_FLAGS_DEBUG "")
set(CMAKE_CXX_FLAGS_DEBUG "")

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    # Release build flags
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
    set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG")

    # Debug build flags
    set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g")
    set(CMAKE_C_FLAGS_DEBUG "-O0 -g")

    # Architecture-specific optimization - detect CPU at compile time
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64|ARM64")
        if(APPLE)
            # Apple Silicon - use apple-m1 optimization
            set(ARCH_FLAGS "-mcpu=apple-m1 -mtune=native")
            message(STATUS "Apple Silicon detected - optimizing for M1/M2/M3")
        else()
            # Linux ARM64
            set(ARCH_FLAGS "-march=native -mtune=native")
            message(STATUS "ARM64 detected - using -march=native")
        endif()
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64")
        set(ARCH_FLAGS "-march=native -mtune=native")
        message(STATUS "x86_64 detected - using -march=native")
    else()
        set(ARCH_FLAGS "")
        message(STATUS "Unknown architecture - using generic optimizations")
    endif()

    # Add architecture flags to release build
    if(ARCH_FLAGS)
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${ARCH_FLAGS}")
        set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} ${ARCH_FLAGS}")
    endif()

    # Additional performance flags for release (safe for most code)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -ffast-math -funroll-loops")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -ffast-math -funroll-loops")

elseif(MSVC)
    set(CMAKE_CXX_FLAGS_RELEASE "/O2 /DNDEBUG /arch:AVX2")
    set(CMAKE_C_FLAGS_RELEASE "/O2 /DNDEBUG /arch:AVX2")
    set(CMAKE_CXX_FLAGS_DEBUG "/Od /Zi")
    set(CMAKE_C_FLAGS_DEBUG "/Od /Zi")
endif()

# Set the flags for the current build type
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS_${CMAKE_BUILD_TYPE}}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS_${CMAKE_BUILD_TYPE}}")

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C flags: ${CMAKE_C_FLAGS}")
message(STATUS "CXX flags: ${CMAKE_CXX_FLAGS}")

# ============================================================================
# Options - FFT Backend Selection
# ============================================================================
option(ENABLE_FFTW "Enable FFTW backend" ON)
option(ENABLE_KISSFFT "Enable KissFFT backend" ON)
option(ENABLE_POCKETFFT "Enable PocketFFT backend" ON)
option(ENABLE_PFFFT "Enable PFFFT backend" ON)
option(ENABLE_KFR "Enable KFR backend" OFF)
option(ENABLE_SIGNALSMITH "Enable Signalsmith backend" ON)
option(ENABLE_JUCE "Enable JUCE backend" ON)
option(ENABLE_FFTS "Enable FFTS backend (x86_64 only)" ON)

# ============================================================================
# CPM - CMake Package Manager
# ============================================================================
set(CPM_DONT_UPDATE_MODULE_PATH ON CACHE BOOL "" FORCE)
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake")

# ============================================================================
# Main Executable
# ============================================================================
add_executable(fft_benchmark
    src/fft_benchmark.cpp
)

target_include_directories(fft_benchmark
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
)

# ============================================================================
# Backend: FFTW (via system package manager)
# ============================================================================
if(ENABLE_FFTW)
    find_package(PkgConfig QUIET)

    if(PkgConfig_FOUND)
        pkg_check_modules(FFTW3F fftw3f)
    endif()

    if(NOT FFTW3F_FOUND)
        find_path(FFTW3_INCLUDE_DIR
            NAMES fftw3.h
            PATHS
            /usr/local/include
            /usr/include
            /opt/homebrew/include
            /opt/local/include
        )

        find_library(FFTW3F_LIBRARY
            NAMES fftw3f libfftw3f
            PATHS
            /usr/local/lib
            /usr/lib
            /opt/homebrew/lib
            /opt/local/lib
        )

        if(FFTW3_INCLUDE_DIR AND FFTW3F_LIBRARY)
            set(FFTW3F_FOUND TRUE)
            set(FFTW3F_INCLUDE_DIRS ${FFTW3_INCLUDE_DIR})
            set(FFTW3F_LINK_LIBRARIES ${FFTW3F_LIBRARY})
        endif()
    endif()

    if(FFTW3F_FOUND)
        message(STATUS "✓ FFTW found")
        target_include_directories(fft_benchmark PRIVATE ${FFTW3F_INCLUDE_DIRS})
        target_link_libraries(fft_benchmark PRIVATE ${FFTW3F_LINK_LIBRARIES})
        target_compile_definitions(fft_benchmark PRIVATE HAVE_FFTW)
    else()
        message(WARNING "✗ FFTW not found - install with: brew install fftw (macOS) or apt-get install libfftw3-dev (Ubuntu)")
        set(ENABLE_FFTW OFF)
    endif()
endif()

# ============================================================================
# Backend: KissFFT (via CPM)
# ============================================================================
if(ENABLE_KISSFFT)
    # Suppress KissFFT alignment warnings
    add_compile_options(-Wno-cast-align)

    CPMAddPackage(
        NAME kissfft
        GITHUB_REPOSITORY mborgerding/kissfft
        GIT_TAG 131.1.0
        OPTIONS
        "KISSFFT_DATATYPE float"
        "KISSFFT_STATIC ON"
        "KISSFFT_TEST OFF"
        "KISSFFT_TOOLS OFF"
    )

    if(kissfft_ADDED)
        message(STATUS "✓ KissFFT added")
        target_link_libraries(fft_benchmark PRIVATE kissfft::kissfft)
        target_compile_definitions(fft_benchmark PRIVATE HAVE_KISSFFT)
    else()
        message(WARNING "✗ KissFFT could not be fetched")
        set(ENABLE_KISSFFT OFF)
    endif()
endif()

# ============================================================================
# Backend: PocketFFT (header-only via CPM)
# ============================================================================
if(ENABLE_POCKETFFT)
    CPMAddPackage(
        NAME pocketfft
        GITHUB_REPOSITORY mreineck/pocketfft
        GIT_TAG cpp
        DOWNLOAD_ONLY YES
    )

    if(pocketfft_ADDED)
        message(STATUS "✓ PocketFFT added")
        target_include_directories(fft_benchmark PRIVATE ${pocketfft_SOURCE_DIR})
        target_compile_definitions(fft_benchmark PRIVATE HAVE_POCKETFFT)
    else()
        message(WARNING "✗ PocketFFT could not be fetched")
        set(ENABLE_POCKETFFT OFF)
    endif()
endif()

# ============================================================================
# Backend: PFFFT (compiled from source via CPM)
# ============================================================================
if(ENABLE_PFFFT)
    CPMAddPackage(
        NAME pffft
        GITHUB_REPOSITORY marton78/pffft
        GIT_TAG master
        DOWNLOAD_ONLY YES
    )

    if(pffft_ADDED)
        message(STATUS "✓ PFFFT added")

        add_library(pffft_lib STATIC
            ${pffft_SOURCE_DIR}/src/pffft.c
            ${pffft_SOURCE_DIR}/src/pffft_double.c
            ${pffft_SOURCE_DIR}/src/pffft_common.c
        )

        target_include_directories(pffft_lib
            PUBLIC ${pffft_SOURCE_DIR}/include/pffft
            PRIVATE ${pffft_SOURCE_DIR}/include/pffft
            ${pffft_SOURCE_DIR}/src
        )

        if(UNIX)
            target_link_libraries(pffft_lib PRIVATE m)
        endif()

        target_link_libraries(fft_benchmark PRIVATE pffft_lib)
        target_compile_definitions(fft_benchmark PRIVATE HAVE_PFFFT)
    else()
        message(WARNING "✗ PFFFT could not be fetched")
        set(ENABLE_PFFFT OFF)
    endif()
endif()

# ============================================================================
# Backend: KFR (heavy DSP framework via CPM)
# ============================================================================
if(ENABLE_KFR)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm|ARM|aarch64|AARCH64")
        set(KFR_ARCH "neon" CACHE STRING "" FORCE)
    else()
        set(KFR_ARCH "sse2" CACHE STRING "" FORCE)
    endif()

    set(KFR_ENABLE_MULTIARCH OFF CACHE BOOL "" FORCE)
    set(KFR_ENABLE_DETECT_CPU OFF CACHE BOOL "" FORCE)

    CPMAddPackage(
        NAME kfr
        GITHUB_REPOSITORY kfrlib/kfr
        GIT_TAG 6.0.2
        OPTIONS
        "KFR_ENABLE_MULTIARCH OFF"
        "KFR_ENABLE_DETECT_CPU OFF"
    )

    if(kfr_ADDED)
        message(STATUS "✓ KFR added")
        target_link_libraries(fft_benchmark PRIVATE kfr kfr_dft)
        target_compile_definitions(fft_benchmark PRIVATE HAVE_KFR)
    else()
        message(WARNING "✗ KFR could not be fetched")
        set(ENABLE_KFR OFF)
    endif()
endif()

# ============================================================================
# Backend: Signalsmith (header-only via CPM)
# ============================================================================
if(ENABLE_SIGNALSMITH)
    CPMAddPackage(
        NAME signalsmith_dsp
        GITHUB_REPOSITORY Signalsmith-Audio/dsp
        GIT_TAG main
        DOWNLOAD_ONLY YES
    )

    if(signalsmith_dsp_ADDED)
        message(STATUS "✓ Signalsmith DSP added")
        target_include_directories(fft_benchmark PRIVATE ${signalsmith_dsp_SOURCE_DIR})
        target_compile_definitions(fft_benchmark PRIVATE HAVE_SIGNALSMITH)
    else()
        message(WARNING "✗ Signalsmith DSP could not be fetched")
        set(ENABLE_SIGNALSMITH OFF)
    endif()
endif()

# ============================================================================
# Backend: JUCE (large DSP framework via CPM)
# ============================================================================
if(ENABLE_JUCE)
    message(STATUS "Fetching JUCE (this may take a while)...")

    CPMAddPackage(
        NAME juce
        GITHUB_REPOSITORY juce-framework/JUCE
        GIT_TAG 8.0.0
        DOWNLOAD_ONLY YES
    )

    if(juce_ADDED)
        message(STATUS "✓ JUCE downloaded")

        target_include_directories(fft_benchmark PRIVATE
            ${juce_SOURCE_DIR}/modules
        )

        target_compile_definitions(fft_benchmark PRIVATE
            HAVE_JUCE
            JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1
            JUCE_STANDALONE_APPLICATION=1
            JUCE_USE_CURL=0
            JUCE_WEB_BROWSER=0
            JUCE_MODAL_LOOPS_PERMITTED=0
        )

        set(JUCE_SOURCES
            ${juce_SOURCE_DIR}/modules/juce_core/juce_core.cpp
            ${juce_SOURCE_DIR}/modules/juce_audio_basics/juce_audio_basics.cpp
            ${juce_SOURCE_DIR}/modules/juce_audio_formats/juce_audio_formats.cpp
            ${juce_SOURCE_DIR}/modules/juce_dsp/juce_dsp.cpp
        )

        if(APPLE)
            set_source_files_properties(${JUCE_SOURCES} PROPERTIES
                COMPILE_FLAGS "-x objective-c++"
            )

            target_link_libraries(fft_benchmark PRIVATE
                "-framework Accelerate"
                "-framework AudioToolbox"
                "-framework Cocoa"
                "-framework IOKit"
                "-framework CoreFoundation"
                "-framework Security"
            )
        endif()

        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/juce_stubs.cpp")
            set(JUCE_STUBS_SRC "${CMAKE_CURRENT_SOURCE_DIR}/juce_stubs.cpp")
        elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/juce_stubs.cpp")
            set(JUCE_STUBS_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/juce_stubs.cpp")
        elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/inc/juce_stubs.cpp")
            set(JUCE_STUBS_SRC "${CMAKE_CURRENT_SOURCE_DIR}/inc/juce_stubs.cpp")
        else()
            message(STATUS "  Generating minimal juce_stubs.cpp")
            set(JUCE_STUBS_SRC "${CMAKE_CURRENT_BINARY_DIR}/juce_stubs.cpp")
            file(WRITE "${JUCE_STUBS_SRC}"
                "namespace juce {\n"
                "    extern const char* const juce_compilationDate = __DATE__;\n"
                "    extern const char* const juce_compilationTime = __TIME__;\n"
                "}\n"
            )
        endif()

        target_sources(fft_benchmark PRIVATE
            ${JUCE_SOURCES}
            ${JUCE_STUBS_SRC}
        )
    else()
        message(WARNING "✗ JUCE could not be fetched")
        set(ENABLE_JUCE OFF)
    endif()
endif()

# ============================================================================
# Backend: FFTS (x86_64 only - known issues on Apple Silicon)
# ============================================================================
if(ENABLE_FFTS)
    # Check if we're on Apple Silicon - FFTS is known to be broken
    # if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64|ARM64" AND APPLE)
    #     message(WARNING "FFTS is disabled on Apple Silicon - NEON assembly incompatible with macOS")
    #     message(WARNING "  Use FFTW, PFFFT, or JUCE for best performance on Apple Silicon")
    #     set(ENABLE_FFTS OFF CACHE BOOL "FFTS backend" FORCE)
    # else()
    CPMAddPackage(
        NAME FFTS
        GITHUB_REPOSITORY RoBr-01/FFTS
        GIT_TAG dev
    )

    if(FFTS_ADDED)
        message(STATUS "✓ FFTS added")

        if(TARGET ffts_static)
            target_link_libraries(fft_benchmark PRIVATE ffts_static)
        elseif(TARGET ffts)
            target_link_libraries(fft_benchmark PRIVATE ffts)
        endif()

        target_include_directories(fft_benchmark PRIVATE
            ${FFTS_SOURCE_DIR}/include
        )

        target_compile_definitions(fft_benchmark PRIVATE HAVE_FFTS)
    else()
        message(WARNING "✗ FFTS could not be fetched")
        set(ENABLE_FFTS OFF)
    endif()
endif()
# endif()

# ============================================================================
# Platform-specific Libraries
# ============================================================================
if(UNIX AND NOT APPLE)
    target_link_libraries(fft_benchmark PRIVATE pthread m)
elseif(APPLE)
    target_link_libraries(fft_benchmark PRIVATE m)
endif()

# ============================================================================
# Suppress common warnings for backends
# ============================================================================
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang")
    # Suppress cast-align warnings from KissFFT and other backends
    add_compile_options(-Wno-cast-align)
endif()

# ============================================================================
# Build Summary
# ============================================================================
message(STATUS "")
message(STATUS "═══════════════════════════════════════════════════════════")
message(STATUS "FFT Benchmark Configuration Summary")
message(STATUS "═══════════════════════════════════════════════════════════")
message(STATUS "Build Type:    ${CMAKE_BUILD_TYPE}")
message(STATUS "Compiler:      ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Platform:      ${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "")
message(STATUS "Optimization Flags:")
message(STATUS "  ${CMAKE_CXX_FLAGS}")
message(STATUS "")
message(STATUS "Enabled FFT Backends:")
message(STATUS "  ${ENABLE_FFTW}        FFTW")
message(STATUS "  ${ENABLE_KISSFFT}     KissFFT")
message(STATUS "  ${ENABLE_POCKETFFT}   PocketFFT")
message(STATUS "  ${ENABLE_PFFFT}       PFFFT")
message(STATUS "  ${ENABLE_KFR}         KFR")
message(STATUS "  ${ENABLE_SIGNALSMITH} Signalsmith")
message(STATUS "  ${ENABLE_JUCE}        JUCE")
message(STATUS "  ${ENABLE_FFTS}        FFTS (x86_64 only)")
message(STATUS "")
message(STATUS "To toggle backends:")
message(STATUS "  cmake -DENABLE_FFTW=OFF -DENABLE_KFR=ON ..")
message(STATUS "═══════════════════════════════════════════════════════════")
message(STATUS "")
