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

# Optimization flags
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -DNDEBUG")
    set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g")
elseif(MSVC)
    set(CMAKE_CXX_FLAGS_RELEASE "/O2 /DNDEBUG")
    set(CMAKE_CXX_FLAGS_DEBUG "/Od /Zi")
endif()

# ============================================================================
# Options - FFT Backend Selection
# ============================================================================
option(ENABLE_FFTW        "Enable FFTW backend"        ON)
option(ENABLE_KISSFFT     "Enable KissFFT backend"     ON)
option(ENABLE_POCKETFFT   "Enable PocketFFT backend"   ON)
option(ENABLE_PFFFT       "Enable PFFFT backend"       ON)
option(ENABLE_KFR         "Enable KFR backend"         OFF)  # OFF by default (compilation issues on some platforms)
option(ENABLE_SIGNALSMITH "Enable Signalsmith backend" OFF)

# ============================================================================
# CPM - CMake Package Manager
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
        SHOW_PROGRESS
    )
endif()

include(${CPM_DOWNLOAD_LOCATION})

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
    # Try to find FFTW using pkg-config first (most reliable)
    find_package(PkgConfig QUIET)
    
    if(PkgConfig_FOUND)
        pkg_check_modules(FFTW3F fftw3f)
    endif()
    
    # Manual search if pkg-config fails
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
            NAMES fftw3f libfftw3f.dylib
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
        message(STATUS "  Include: ${FFTW3F_INCLUDE_DIRS}")
        message(STATUS "  Library: ${FFTW3F_LINK_LIBRARIES}")
        
        target_include_directories(fft_benchmark PRIVATE ${FFTW3F_INCLUDE_DIRS})
        # Use LINK_LIBRARIES which has full paths, not LIBRARIES which only has names
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
        message(STATUS "✓ KissFFT added via CPM")
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
        message(STATUS "✓ PocketFFT added via CPM")
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
        message(STATUS "✓ PFFFT added via CPM")
        
        # Create library from source files
        # Need to include pffft_common.c for aligned_malloc/aligned_free
        add_library(pffft_lib STATIC
            ${pffft_SOURCE_DIR}/pffft.c
            ${pffft_SOURCE_DIR}/pffft_double.c
            ${pffft_SOURCE_DIR}/pffft_common.c
        )
        
        target_include_directories(pffft_lib PUBLIC ${pffft_SOURCE_DIR})
        
        # Link with math library on Unix
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
    # KFR requires specific architecture settings
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm|ARM|aarch64|AARCH64")
        set(KFR_ARCH "neon" CACHE STRING "" FORCE)
    else()
        # Use generic x86-64 to avoid AVX issues
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
        message(STATUS "✓ KFR added via CPM")
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
        message(STATUS "✓ Signalsmith DSP added via CPM")
        target_include_directories(fft_benchmark PRIVATE ${signalsmith_dsp_SOURCE_DIR})
        target_compile_definitions(fft_benchmark PRIVATE HAVE_SIGNALSMITH)
    else()
        message(WARNING "✗ Signalsmith DSP could not be fetched")
        set(ENABLE_SIGNALSMITH OFF)
    endif()
endif()

# ============================================================================
# Platform-specific Libraries
# ============================================================================
if(UNIX)
    target_link_libraries(fft_benchmark PRIVATE pthread m)
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
message(STATUS "Enabled FFT Backends:")
message(STATUS "  ${ENABLE_FFTW}        FFTW")
message(STATUS "  ${ENABLE_KISSFFT}        KissFFT")
message(STATUS "  ${ENABLE_POCKETFFT}        PocketFFT")
message(STATUS "  ${ENABLE_PFFFT}        PFFFT")
message(STATUS "  ${ENABLE_KFR}        KFR")
message(STATUS "  ${ENABLE_SIGNALSMITH}        Signalsmith")
message(STATUS "")
message(STATUS "To toggle backends:")
message(STATUS "  cmake -DENABLE_FFTW=OFF -DENABLE_KFR=ON ..")
message(STATUS "═══════════════════════════════════════════════════════════")
message(STATUS "")
