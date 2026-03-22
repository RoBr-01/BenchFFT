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
option(ENABLE_KISSFFT     "Enable KissFFT backend"     OFF)
option(ENABLE_POCKETFFT   "Enable PocketFFT backend"   OFF)
option(ENABLE_PFFFT       "Enable PFFFT backend"       ON)
option(ENABLE_KFR         "Enable KFR backend"         OFF)  # Broken on AppleClang 17 / ARM64 (vec.hpp issue in KFR 6.0.2)
option(ENABLE_SIGNALSMITH "Enable Signalsmith backend" ON)
option(ENABLE_JUCE        "Enable JUCE backend"        ON)

# ============================================================================
# CPM - CMake Package Manager
# ============================================================================
# Vendored copy lives at cmake/CPM.cmake in the source tree.
# Disable CPM's self-update mechanism - it tries to download a newer version
# of itself at configure time which fails in offline/restricted environments.
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
        
        add_library(pffft_lib STATIC
            ${pffft_SOURCE_DIR}/src/pffft.c
            ${pffft_SOURCE_DIR}/src/pffft_double.c
            ${pffft_SOURCE_DIR}/src/pffft_common.c
        )
        
        # Sources use bare includes like "pffft.h" and "pffft_double.h".
        # Public headers live in include/pffft/, internal headers in src/.
        # Both must be on the include path so the .c files can find everything.
        target_include_directories(pffft_lib
            PUBLIC  ${pffft_SOURCE_DIR}/include/pffft   # exposes pffft.h to consumers too
            PRIVATE ${pffft_SOURCE_DIR}/include/pffft   # so .c files find pffft_double.h etc.
                    ${pffft_SOURCE_DIR}/src              # internal: pffft_priv_impl.h, fmv.h, simd/
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
# Backend: JUCE (large DSP framework via CPM)
# ============================================================================
if(ENABLE_JUCE)
    message(STATUS "Fetching JUCE (this may take a while, it's a large framework)...")
    
    CPMAddPackage(
        NAME juce
        GITHUB_REPOSITORY juce-framework/JUCE
        GIT_TAG 8.0.0
        DOWNLOAD_ONLY YES
    )
    
    if(juce_ADDED)
        message(STATUS "✓ JUCE downloaded via CPM")
        
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
        
        # ----------------------------------------------------------------
        # JUCE module unity-build sources
        # ----------------------------------------------------------------
        set(JUCE_SOURCES
            ${juce_SOURCE_DIR}/modules/juce_core/juce_core.cpp
            ${juce_SOURCE_DIR}/modules/juce_audio_basics/juce_audio_basics.cpp
            ${juce_SOURCE_DIR}/modules/juce_audio_formats/juce_audio_formats.cpp
            ${juce_SOURCE_DIR}/modules/juce_dsp/juce_dsp.cpp
        )
        
        if(APPLE)
            # JUCE's ObjC++ unity files must be compiled as Objective-C++.
            # We set this per-file so the main benchmark source is unaffected.
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

        # ----------------------------------------------------------------
        # juce_stubs.cpp — provides juce_compilationDate / juce_compilationTime
        # that JUCE normally auto-generates during a full JUCE build.
        #
        # We look for it next to CMakeLists.txt first (project root), then
        # fall back to src/, then inc/ — so the project keeps working
        # regardless of where the file lives.
        # ----------------------------------------------------------------
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/juce_stubs.cpp")
            set(JUCE_STUBS_SRC "${CMAKE_CURRENT_SOURCE_DIR}/juce_stubs.cpp")
        elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/juce_stubs.cpp")
            set(JUCE_STUBS_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/juce_stubs.cpp")
        elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/inc/juce_stubs.cpp")
            set(JUCE_STUBS_SRC "${CMAKE_CURRENT_SOURCE_DIR}/inc/juce_stubs.cpp")
        else()
            # Neither location found — generate a minimal stub on the fly so
            # the build doesn't fail outright.
            message(STATUS "  juce_stubs.cpp not found — generating a minimal stub")
            set(JUCE_STUBS_SRC "${CMAKE_CURRENT_BINARY_DIR}/juce_stubs.cpp")
            file(WRITE "${JUCE_STUBS_SRC}"
"// Auto-generated by CMake — provides JUCE compilation timestamp symbols.\n"
"namespace juce\n"
"{\n"
"    extern const char* const juce_compilationDate = __DATE__;\n"
"    extern const char* const juce_compilationTime = __TIME__;\n"
"}\n"
            )
        endif()

        message(STATUS "  Using juce_stubs.cpp from: ${JUCE_STUBS_SRC}")

        # The stubs file is plain C++ (no ObjC), so add it separately without
        # the "-x objective-c++" flag that the JUCE unity files carry.
        target_sources(fft_benchmark PRIVATE
            ${JUCE_SOURCES}
            ${JUCE_STUBS_SRC}
        )
        
        message(STATUS "    JUCE modules: juce_core, juce_audio_basics, juce_audio_formats, juce_dsp")
    else()
        message(WARNING "✗ JUCE could not be fetched")
        set(ENABLE_JUCE OFF)
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
message(STATUS "  ${ENABLE_JUCE}        JUCE")
message(STATUS "")
message(STATUS "To toggle backends:")
message(STATUS "  cmake -DENABLE_FFTW=OFF -DENABLE_KFR=ON ..")
message(STATUS "═══════════════════════════════════════════════════════════")
message(STATUS "")