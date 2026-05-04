cmake_minimum_required(VERSION 3.15)

project(BenchFFT
    VERSION 1.0.0
    LANGUAGES CXX C
    DESCRIPTION "FFT Library Benchmark Suite"
)

include(cmake/Checks.cmake)
CHECK_GIT_SUBMODULES()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(${PROJECT_NAME})
target_sources(${PROJECT_NAME} PRIVATE src/fft_benchmark.cpp)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/inc)

# Backend: FFTW (via source build)
set(ENABLE_FLOAT ON CACHE BOOL "single-precision" FORCE)
set(ENABLE_THREADS OFF CACHE BOOL "Enable threading" FORCE) # For multi-core
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)

set(ENABLE_NEON ON CACHE BOOL "Enable NEON SIMD" FORCE)
set(DISABLE_ALL_SIMD OFF CACHE BOOL "" FORCE)

# For macOS ARM64, explicitly set the architecture
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum OS X deployment version" FORCE)
endif()

# Add FFTW with proper C flags
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/lib/fftw-3.3.11)

# The issue: FFTW's CMake doesn't properly set up the float API
# Manually add the missing defines
target_compile_definitions(fftw3f PRIVATE FFTW_SINGLE)
target_compile_definitions(fftw3f PUBLIC FFTW_SINGLE)

# Link your executable
target_link_libraries(${PROJECT_NAME} PRIVATE fftw3f)

# Add the include directory (the build directory for generated headers)
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/fftw-3.3.11/api
    ${CMAKE_CURRENT_BINARY_DIR}/lib/fftw-3.3.11
)

target_compile_options(${PROJECT_NAME} PRIVATE -O3)
target_link_options(${PROJECT_NAME} PRIVATE -flto)


# Backend: KissFFT (via source build)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/lib/kissfft)
target_link_libraries(${PROJECT_NAME} PRIVATE kissfft::kissfft)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/lib/kissfft)


# Backend: PocketFFT (header-only via source)
# Add as an interface (header-only lib)
add_library(pocketfft INTERFACE)
target_include_directories(pocketfft INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/lib/pocketfft)

# Then add to our build
target_link_libraries(${PROJECT_NAME} PRIVATE pocketfft)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/lib/pocketfft)


# Backend: PFFFT (compiled from source)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/lib/pffft)
target_link_libraries(${PROJECT_NAME} PRIVATE PFFFT::PFFFT)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/lib/pffft/include/pffft)

# Backend: KFR (heavy DSP framework via source)
set(KFR_ENABLE_DFT ON CACHE BOOL "" FORCE)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/lib/kfr)
target_link_libraries(${PROJECT_NAME} PRIVATE kfr kfr_dft)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/lib/kfr/include/kfr)

# Backend: Signalsmith (header-only via source)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/lib/signalsmith)
target_link_libraries(${PROJECT_NAME} PRIVATE signalsmith-dsp)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/lib/signalsmith)

# Backend: JUCE (large DSP framework via CPM)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/lib/juce)
target_link_libraries(${PROJECT_NAME}
    PRIVATE
    juce::juce_dsp
)
target_compile_definitions(${PROJECT_NAME}
    PRIVATE
    JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1
    JUCE_STANDALONE_APPLICATION=1
)

# Backend: HifiLofi
add_library(hifilofi)
target_sources(hifilofi PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/lib/hifilofi/AudioFFT.cpp)
target_include_directories(hifilofi PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/lib/hifilofi/AudioFFT.h)

target_link_libraries(${PROJECT_NAME} PRIVATE hifilofi)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/lib/hifilofi)
