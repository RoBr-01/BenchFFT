# FFT Library Benchmark

A comprehensive benchmark suite for comparing FFT library performance in C++.

## Supported Libraries

- **FFTW3** - The "Fastest Fourier Transform in the West"
- **KissFFT** - Simple, portable FFT library
- **PocketFFT** - Modern, header-only FFT implementation
- **PFFFT** - Pretty Fast FFT (SIMD-optimized)
- **KFR** - Fast, modern C++ DSP framework
- **Signalsmith** - Audio DSP library with FFT
- **HiFiLoFi** - Lightweight FFT library
- **JUCE** - Professional audio framework

## Features

- **Unified Interface**: Single abstraction layer for all libraries
- **Multiple Transform Types**: Real-to-complex, complex-to-real, complex-to-complex
- **Statistical Analysis**: Mean, median, min, max, standard deviation
- **Throughput Calculation**: Measures samples per second
- **CSV Export**: Results exported for further analysis
- **Warmup Iterations**: Ensures stable measurements
- **Flexible Size Testing**: Test any FFT sizes you need

## Building

### Prerequisites

Install the libraries you want to benchmark. On Ubuntu/Debian:

```bash
# FFTW
sudo apt-get install libfftw3-dev

# KissFFT
git clone https://github.com/mborgerding/kissfft
cd kissfft && mkdir build && cd build
cmake .. && make && sudo make install

# PocketFFT (header-only)
git clone https://github.com/mreineck/pocketfft
sudo cp pocketfft/pocketfft_hdronly.h /usr/local/include/

# PFFFT
git clone https://github.com/marton78/pffft
cd pffft && mkdir build && cd build
cmake .. && make && sudo make install

# KFR
git clone https://github.com/kfrlib/kfr
cd kfr && mkdir build && cd build
cmake .. -DENABLE_TESTS=OFF && make && sudo make install
```

### Compile the Benchmark

```bash
mkdir build && cd build
cmake ..
make
```

### Enable/Disable Specific Libraries

```bash
cmake .. -DENABLE_FFTW=ON \
         -DENABLE_KISSFFT=ON \
         -DENABLE_POCKETFFT=ON \
         -DENABLE_PFFFT=ON \
         -DENABLE_KFR=ON \
         -DENABLE_SIGNALSMITH=OFF \
         -DENABLE_HIFILOFI=OFF \
         -DENABLE_JUCE=OFF
```

## Running

Basic usage:
```bash
./fft_benchmark
```

With custom iteration counts:
```bash
./fft_benchmark <warmup_iterations> <benchmark_iterations>
# Example: 20 warmup iterations, 200 benchmark iterations
./fft_benchmark 20 200
```

## Output

The benchmark produces:

1. **Console output** with real-time results:
   ```
   FFT Library Benchmark
   =====================
   Warmup iterations: 10
   Benchmark iterations: 100

   Testing size: 1024
   ------------------------------------------------------------
           FFTW   R2C Forward:    12.45 μs (median:    12.38 μs) [ 82.23 Msamp/s]
           FFTW  C2R Backward:    13.21 μs (median:    13.15 μs) [ 77.50 Msamp/s]
   ...
   ```

2. **CSV file** (`fft_benchmark_results.csv`) for further analysis

## Interpreting Results

- **Mean time**: Average execution time across all iterations
- **Median time**: Middle value (less affected by outliers)
- **Throughput**: Higher is better (millions of samples per second)
- **Standard deviation**: Lower indicates more consistent performance

## Customization

### Testing Different Sizes

Edit `fft_benchmark.cpp` and modify the `sizes` vector:

```cpp
std::vector<int> sizes = {
    64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768
};
```

### Adding Your Own Backend

1. Create a new header file inheriting from `FFTBackend`
2. Implement all virtual methods
3. Add to `fft_benchmark.cpp`:
   ```cpp
   #include "your_backend.h"
   // In main():
   benchmark.add_backend(std::make_unique<YourBackend>());
   ```

### Testing In-Place Transforms

Some libraries support in-place transforms. To test these, modify the interface to add:

```cpp
virtual void forward_inplace(std::complex<float>* data) = 0;
```

## Implementation Notes

### Library-Specific Considerations

- **FFTW**: Uses FFTW_MEASURE for optimal plans (slower setup, faster execution)
- **PFFFT**: Requires sizes to be multiples of 16 for best performance
- **JUCE**: Only supports power-of-2 sizes
- **KFR**: Automatically normalizes inverse transforms

### Normalization

Different libraries handle inverse FFT normalization differently:
- FFTW: No normalization (divide by N manually)
- KissFFT: No normalization (divide by N manually)
- PocketFFT: Normalized by default
- KFR: Normalized by default

The benchmark wrappers handle this to ensure fair comparison.

### Memory Alignment

Some libraries (FFTW, PFFFT) benefit from aligned memory. The wrappers use library-specific allocation functions where applicable.

## Benchmarking Best Practices

1. **Close background applications** to reduce noise
2. **Disable CPU frequency scaling** for consistent results:
   ```bash
   sudo cpupower frequency-set --governor performance
   ```
3. **Pin to specific CPU cores** to avoid cache issues:
   ```bash
   taskset -c 0 ./fft_benchmark
   ```
4. **Run multiple times** and compare results
5. **Test realistic problem sizes** for your application

## Analyzing Results

Load the CSV into Python/R/Excel for visualization:

```python
import pandas as pd
import matplotlib.pyplot as plt

df = pd.read_csv('fft_benchmark_results.csv')

# Plot throughput by size
for transform in df['Transform'].unique():
    data = df[df['Transform'] == transform]
    for lib in data['Library'].unique():
        lib_data = data[data['Library'] == lib]
        plt.plot(lib_data['Size'], lib_data['Throughput(Msamp/s)'], 
                marker='o', label=lib)
    
    plt.xlabel('FFT Size')
    plt.ylabel('Throughput (Msamples/s)')
    plt.title(f'{transform} Performance')
    plt.legend()
    plt.xscale('log', base=2)
    plt.grid(True)
    plt.show()
```

## Common Issues

### Library Not Found

If CMake can't find a library:
```bash
# Specify paths manually
cmake .. -DCMAKE_PREFIX_PATH="/path/to/library"
```

### Compilation Errors

- Ensure C++17 support: `g++ --version` (GCC 7+ or Clang 5+)
- Check library headers are accessible
- Verify library ABI compatibility

### Unexpected Results

- Check CPU governor settings
- Verify no thermal throttling
- Confirm consistent compiler optimization flags
- Test with different iteration counts

## License

This benchmark framework is provided as-is for comparison purposes. Each FFT library has its own license - please check individual library licenses for usage in your projects.

## Contributing

To add support for additional FFT libraries, please:
1. Create a backend header following the existing pattern
2. Update CMakeLists.txt with detection logic
3. Update this README with build instructions

## References

- FFTW: http://www.fftw.org/
- KissFFT: https://github.com/mborgerding/kissfft
- PocketFFT: https://github.com/mreineck/pocketfft
- PFFFT: https://github.com/marton78/pffft
- KFR: https://github.com/kfrlib/kfr
- Signalsmith: https://github.com/Signalsmith-Audio/
- JUCE: https://juce.com/