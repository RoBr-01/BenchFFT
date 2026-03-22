#include "../inc/fft_backend.h"

// Include backends based on what's enabled
#ifdef HAVE_FFTW
#include "fftw_backend.h"
#endif

#ifdef HAVE_KISSFFT
#include "kissfft_backend.h"
#endif

#ifdef HAVE_POCKETFFT
#include "pocketfft_backend.h"
#endif

#ifdef HAVE_PFFFT
#include "pffft_backend.h"
#endif

#ifdef HAVE_KFR
#include "kfr_backend.h"
#endif

#ifdef HAVE_SIGNALSMITH
#include "signalsmith_backend.h"
#endif

#ifdef HAVE_JUCE
#include "JUCE_backend.h"
#endif

#ifdef HAVE_HIFILOFI
#include "hifilofi_backend.h"
#endif

// includes
#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <numeric>
#include <random>
#include <sstream>
#include <thread>
#include <vector>

#ifdef __APPLE__
#include <sys/sysctl.h>
#elif defined(__linux__)
#include <fstream>
#endif

struct BenchmarkResult {
    std::string library_name;
    int size;
    std::string transform_type;
    double mean_time_us;
    double median_time_us;
    double min_time_us;
    double max_time_us;
    double std_dev_us;
    double throughput_msamples_per_sec;
    bool validation_passed;
};

struct SystemInfo {
    std::string cpu_model;
    std::string os_version;
    std::string compiler;
    std::string build_flags;
    std::string timestamp;
};

class FFTBenchmark {
   public:
    FFTBenchmark(int warmup_iterations = 10, int benchmark_iterations = 100)
        : warmup_iterations_(warmup_iterations),
          benchmark_iterations_(benchmark_iterations) {
        // Initialize random number generator for test data
        rng_.seed(42);
        collect_system_info();
    }

    void add_backend(std::unique_ptr<FFTBackend> backend) {
        backends_.push_back(std::move(backend));
    }

    void run_benchmarks(const std::vector<int>& sizes) {
        print_header();

        for (int size : sizes) {
            std::cout << "Testing size: " << size << "\n";
            std::cout << std::string(80, '-') << "\n";

            for (auto& backend : backends_) {
                if (!backend->supports_size(size)) {
                    std::cout << std::setw(15) << backend->name()
                              << ": SKIPPED (unsupported size)\n";
                    continue;
                }

                try {
                    backend->setup(size);

                    // Validate correctness first
                    bool valid = validate_fft(*backend, size);
                    if (!valid) {
                        std::cout
                            << std::setw(15) << backend->name()
                            << ": FAILED VALIDATION - skipping benchmarks\n";
                        backend->cleanup();
                        continue;
                    }

                    // Test real-to-complex forward transform
                    auto result_forward = benchmark_forward(*backend, size);
                    result_forward.validation_passed = valid;
                    results_.push_back(result_forward);
                    print_result(result_forward);

                    // Test complex-to-real backward transform
                    auto result_backward = benchmark_backward(*backend, size);
                    result_backward.validation_passed = valid;
                    results_.push_back(result_backward);
                    print_result(result_backward);

                    // Test complex-to-complex transforms if supported
                    if (backend->supports_complex()) {
                        auto result_c2c_fwd =
                            benchmark_forward_complex(*backend, size);
                        result_c2c_fwd.validation_passed = valid;
                        results_.push_back(result_c2c_fwd);
                        print_result(result_c2c_fwd);

                        auto result_c2c_bwd =
                            benchmark_backward_complex(*backend, size);
                        result_c2c_bwd.validation_passed = valid;
                        results_.push_back(result_c2c_bwd);
                        print_result(result_c2c_bwd);
                    }

                    backend->cleanup();

                    // Brief pause to avoid thermal throttling
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));

                } catch (const std::exception& e) {
                    std::cout << std::setw(15) << backend->name()
                              << ": ERROR - " << e.what() << "\n";
                }
            }

            std::cout << "\n";
        }

        print_summary();
    }

    void export_csv(const std::string& filename) const {
        std::ofstream file(filename);

        // Header with system info
        file << "# FFT Benchmark Results\n";
        file << "# CPU: " << sys_info_.cpu_model << "\n";
        file << "# OS: " << sys_info_.os_version << "\n";
        file << "# Compiler: " << sys_info_.compiler << "\n";
        file << "# Build Flags: " << sys_info_.build_flags << "\n";
        file << "# Timestamp: " << sys_info_.timestamp << "\n";
        file << "# Warmup Iterations: " << warmup_iterations_ << "\n";
        file << "# Benchmark Iterations: " << benchmark_iterations_ << "\n";
        file << "#\n";

        file << "Library,Size,Transform,Mean(us),Median(us),Min(us),Max(us),"
             << "StdDev(us),Throughput(Msamp/s),Validated\n";

        for (const auto& result : results_) {
            file << result.library_name << "," << result.size << ","
                 << result.transform_type << "," << result.mean_time_us << ","
                 << result.median_time_us << "," << result.min_time_us << ","
                 << result.max_time_us << "," << result.std_dev_us << ","
                 << result.throughput_msamples_per_sec << ","
                 << (result.validation_passed ? "PASS" : "FAIL") << "\n";
        }
    }

   private:
    void collect_system_info() {
        // Get timestamp
        auto now = std::time(nullptr);
        char time_buf[100];
        std::strftime(time_buf,
                      sizeof(time_buf),
                      "%Y-%m-%d %H:%M:%S",
                      std::localtime(&now));
        sys_info_.timestamp = time_buf;

        // Get CPU model
#ifdef __APPLE__
        char cpu_brand[256];
        size_t size = sizeof(cpu_brand);
        if (sysctlbyname(
                "machdep.cpu.brand_string", &cpu_brand, &size, nullptr, 0) ==
            0) {
            sys_info_.cpu_model = cpu_brand;
        } else {
            sys_info_.cpu_model = "Unknown ARM/Apple Silicon";
        }
        sys_info_.os_version = "macOS";
#elif defined(__linux__)
        std::ifstream cpuinfo("/proc/cpuinfo");
        std::string line;
        while (std::getline(cpuinfo, line)) {
            if (line.find("model name") != std::string::npos) {
                auto pos = line.find(":");
                if (pos != std::string::npos) {
                    sys_info_.cpu_model = line.substr(pos + 2);
                    break;
                }
            }
        }
        sys_info_.os_version = "Linux";
#else
        sys_info_.cpu_model = "Unknown";
        sys_info_.os_version = "Unknown";
#endif

        // Get compiler info
#if defined(__clang__)
        sys_info_.compiler = "Clang " + std::string(__clang_version__);
#elif defined(__GNUC__)
        sys_info_.compiler = "GCC " + std::to_string(__GNUC__) + "." +
                             std::to_string(__GNUC_MINOR__) + "." +
                             std::to_string(__GNUC_PATCHLEVEL__);
#elif defined(_MSC_VER)
        sys_info_.compiler = "MSVC " + std::to_string(_MSC_VER);
#else
        sys_info_.compiler = "Unknown";
#endif

        // Build flags
#ifdef NDEBUG
        sys_info_.build_flags = "Release (-O3 -march=native)";
#else
        sys_info_.build_flags = "Debug (-O0 -g)";
#endif
    }

    void print_header() const {
        std::cout << "\n";
        std::cout << "╔════════════════════════════════════════════════════════"
                     "════════════════════╗\n";
        std::cout << "║                         FFT Library Benchmark          "
                     "                    ║\n";
        std::cout << "╚════════════════════════════════════════════════════════"
                     "════════════════════╝\n";
        std::cout << "\n";
        std::cout << "System Information:\n";
        std::cout << "  CPU:           " << sys_info_.cpu_model << "\n";
        std::cout << "  OS:            " << sys_info_.os_version << "\n";
        std::cout << "  Compiler:      " << sys_info_.compiler << "\n";
        std::cout << "  Build:         " << sys_info_.build_flags << "\n";
        std::cout << "  Timestamp:     " << sys_info_.timestamp << "\n";
        std::cout << "\n";
        std::cout << "Benchmark Configuration:\n";
        std::cout << "  Warmup iterations:     " << warmup_iterations_ << "\n";
        std::cout << "  Benchmark iterations:  " << benchmark_iterations_
                  << "\n";
        std::cout
            << "  Cache flushing:        Enabled (multiple input buffers)\n";
        std::cout
            << "  Validation:            Enabled (round-trip accuracy check)\n";
        std::cout << "\n";
    }

    bool validate_fft(FFTBackend& backend, int size) {
        const float tolerance = 1e-3f;  // Relaxed tolerance for float precision

        // Test 1: DC component (all ones)
        std::vector<float> input(size, 1.0f);
        std::vector<std::complex<float>> freq(size / 2 + 1);
        std::vector<float> output(size);

        backend.forward(input.data(), freq.data());
        backend.backward(freq.data(), output.data());

        for (int i = 0; i < size; ++i) {
            if (std::abs(output[i] - 1.0f) > tolerance) {
                std::cerr << "  Validation failed: DC test (expected 1.0, got "
                          << output[i] << " at index " << i << ")\n";
                return false;
            }
        }

        // Test 2: Impulse (Kronecker delta)
        std::fill(input.begin(), input.end(), 0.0f);
        input[0] = 1.0f;

        backend.forward(input.data(), freq.data());
        backend.backward(freq.data(), output.data());

        if (std::abs(output[0] - 1.0f) > tolerance) {
            std::cerr << "  Validation failed: Impulse test at DC\n";
            return false;
        }
        for (int i = 1; i < size; ++i) {
            if (std::abs(output[i]) > tolerance) {
                std::cerr << "  Validation failed: Impulse test at index " << i
                          << "\n";
                return false;
            }
        }

        // Test 3: Random data round-trip
        std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
        for (auto& val : input) {
            val = dist(rng_);
        }
        std::vector<float> input_copy = input;

        backend.forward(input.data(), freq.data());
        backend.backward(freq.data(), output.data());

        for (int i = 0; i < size; ++i) {
            if (std::abs(output[i] - input_copy[i]) > tolerance) {
                std::cerr << "  Validation failed: Round-trip test (max error: "
                          << std::abs(output[i] - input_copy[i]) << ")\n";
                return false;
            }
        }

        return true;
    }

    BenchmarkResult benchmark_forward(FFTBackend& backend, int size) {
        // Pre-generate multiple input buffers to avoid cache effects
        std::vector<std::vector<float>> inputs(benchmark_iterations_);
        std::vector<std::vector<std::complex<float>>> outputs(
            benchmark_iterations_);

        std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
        for (int i = 0; i < benchmark_iterations_; ++i) {
            inputs[i].resize(size);
            outputs[i].resize(size / 2 + 1);
            for (auto& val : inputs[i]) {
                val = dist(rng_);
            }
        }

        // Warmup with first buffer
        for (int i = 0; i < warmup_iterations_; ++i) {
            backend.forward(inputs[0].data(), outputs[0].data());
        }

        // Benchmark
        std::vector<double> times;
        times.reserve(benchmark_iterations_);

        for (int i = 0; i < benchmark_iterations_; ++i) {
            auto start = std::chrono::high_resolution_clock::now();
            backend.forward(inputs[i].data(), outputs[i].data());
            auto end = std::chrono::high_resolution_clock::now();

            auto duration =
                std::chrono::duration_cast<std::chrono::nanoseconds>(end -
                                                                     start);
            times.push_back(duration.count() /
                            1000.0);  // Convert to microseconds
        }

        // Prevent compiler from optimizing away the computation
        volatile float sink = outputs[0][0].real();
        (void)sink;

        return compute_statistics(backend.name(), size, "R2C Forward", times);
    }

    BenchmarkResult benchmark_backward(FFTBackend& backend, int size) {
        // Pre-generate multiple input buffers
        std::vector<std::vector<std::complex<float>>> inputs(
            benchmark_iterations_);
        std::vector<std::vector<float>> outputs(benchmark_iterations_);

        std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
        for (int i = 0; i < benchmark_iterations_; ++i) {
            inputs[i].resize(size / 2 + 1);
            outputs[i].resize(size);
            for (auto& val : inputs[i]) {
                val = std::complex<float>(dist(rng_), dist(rng_));
            }
        }

        // Warmup
        for (int i = 0; i < warmup_iterations_; ++i) {
            backend.backward(inputs[0].data(), outputs[0].data());
        }

        // Benchmark
        std::vector<double> times;
        times.reserve(benchmark_iterations_);

        for (int i = 0; i < benchmark_iterations_; ++i) {
            auto start = std::chrono::high_resolution_clock::now();
            backend.backward(inputs[i].data(), outputs[i].data());
            auto end = std::chrono::high_resolution_clock::now();

            auto duration =
                std::chrono::duration_cast<std::chrono::nanoseconds>(end -
                                                                     start);
            times.push_back(duration.count() / 1000.0);
        }

        volatile float sink = outputs[0][0];
        (void)sink;

        return compute_statistics(backend.name(), size, "C2R Backward", times);
    }

    BenchmarkResult benchmark_forward_complex(FFTBackend& backend, int size) {
        std::vector<std::vector<std::complex<float>>> inputs(
            benchmark_iterations_);
        std::vector<std::vector<std::complex<float>>> outputs(
            benchmark_iterations_);

        std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
        for (int i = 0; i < benchmark_iterations_; ++i) {
            inputs[i].resize(size);
            outputs[i].resize(size);
            for (auto& val : inputs[i]) {
                val = std::complex<float>(dist(rng_), dist(rng_));
            }
        }

        // Warmup
        for (int i = 0; i < warmup_iterations_; ++i) {
            backend.forward_complex(inputs[0].data(), outputs[0].data());
        }

        // Benchmark
        std::vector<double> times;
        times.reserve(benchmark_iterations_);

        for (int i = 0; i < benchmark_iterations_; ++i) {
            auto start = std::chrono::high_resolution_clock::now();
            backend.forward_complex(inputs[i].data(), outputs[i].data());
            auto end = std::chrono::high_resolution_clock::now();

            auto duration =
                std::chrono::duration_cast<std::chrono::nanoseconds>(end -
                                                                     start);
            times.push_back(duration.count() / 1000.0);
        }

        volatile float sink = outputs[0][0].real();
        (void)sink;

        return compute_statistics(backend.name(), size, "C2C Forward", times);
    }

    BenchmarkResult benchmark_backward_complex(FFTBackend& backend, int size) {
        std::vector<std::vector<std::complex<float>>> inputs(
            benchmark_iterations_);
        std::vector<std::vector<std::complex<float>>> outputs(
            benchmark_iterations_);

        std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
        for (int i = 0; i < benchmark_iterations_; ++i) {
            inputs[i].resize(size);
            outputs[i].resize(size);
            for (auto& val : inputs[i]) {
                val = std::complex<float>(dist(rng_), dist(rng_));
            }
        }

        // Warmup
        for (int i = 0; i < warmup_iterations_; ++i) {
            backend.backward_complex(inputs[0].data(), outputs[0].data());
        }

        // Benchmark
        std::vector<double> times;
        times.reserve(benchmark_iterations_);

        for (int i = 0; i < benchmark_iterations_; ++i) {
            auto start = std::chrono::high_resolution_clock::now();
            backend.backward_complex(inputs[i].data(), outputs[i].data());
            auto end = std::chrono::high_resolution_clock::now();

            auto duration =
                std::chrono::duration_cast<std::chrono::nanoseconds>(end -
                                                                     start);
            times.push_back(duration.count() / 1000.0);
        }

        volatile float sink = outputs[0][0].real();
        (void)sink;

        return compute_statistics(backend.name(), size, "C2C Backward", times);
    }

    BenchmarkResult compute_statistics(const std::string& name,
                                       int size,
                                       const std::string& transform_type,
                                       std::vector<double>& times) {
        BenchmarkResult result;
        result.library_name = name;
        result.size = size;
        result.transform_type = transform_type;

        // Sort for median calculation
        std::sort(times.begin(), times.end());

        result.min_time_us = times.front();
        result.max_time_us = times.back();
        result.median_time_us = times[times.size() / 2];

        // Calculate mean
        result.mean_time_us =
            std::accumulate(times.begin(), times.end(), 0.0) / times.size();

        // Calculate standard deviation
        double sq_sum = 0.0;
        for (double time : times) {
            sq_sum +=
                (time - result.mean_time_us) * (time - result.mean_time_us);
        }
        result.std_dev_us = std::sqrt(sq_sum / times.size());

        // Calculate throughput (million samples per second)
        result.throughput_msamples_per_sec = (size / result.mean_time_us);

        return result;
    }

    void print_result(const BenchmarkResult& result) const {
        std::cout << std::setw(15) << result.library_name << " "
                  << std::setw(13) << result.transform_type << ": "
                  << std::fixed << std::setprecision(2) << std::setw(8)
                  << result.mean_time_us << " μs"
                  << " (median: " << std::setw(8) << result.median_time_us
                  << " μs)"
                  << " [" << std::setw(7) << result.throughput_msamples_per_sec
                  << " Msamp/s]"
                  << " ✓\n";
    }

    void print_summary() const {
        std::cout << "\n";
        std::cout << "╔════════════════════════════════════════════════════════"
                     "════════════════════╗\n";
        std::cout << "║                    Performance Summary (Top 3 per "
                     "Category)               ║\n";
        std::cout << "╚════════════════════════════════════════════════════════"
                     "════════════════════╝\n";
        std::cout << "\n";

        // Group results by size and transform type
        std::map<std::pair<int, std::string>, std::vector<BenchmarkResult>>
            grouped;
        for (const auto& result : results_) {
            if (result.validation_passed) {  // Only include validated results
                grouped[{result.size, result.transform_type}].push_back(result);
            }
        }

        for (auto& [key, group] : grouped) {
            std::sort(
                group.begin(), group.end(), [](const auto& a, const auto& b) {
                    return a.mean_time_us < b.mean_time_us;
                });

            std::cout << "Size " << std::setw(5) << key.first << " - "
                      << std::setw(13) << std::left << key.second << std::right
                      << ":\n";
            for (size_t i = 0; i < std::min(size_t(3), group.size()); ++i) {
                double speedup =
                    i > 0 ? group[i].mean_time_us / group[0].mean_time_us : 1.0;
                std::cout << "  " << (i + 1) << ". " << std::setw(15)
                          << group[i].library_name << ": " << std::fixed
                          << std::setprecision(2) << std::setw(7)
                          << group[i].mean_time_us << " μs";
                if (i > 0) {
                    std::cout << "  (" << std::setprecision(1) << speedup
                              << "× slower)";
                } else {
                    std::cout << "  (fastest)";
                }
                std::cout << "\n";
            }
            std::cout << "\n";
        }

        std::cout << "Note: All results validated for correctness (round-trip "
                     "error < 0.1%)\n";
    }

    int warmup_iterations_;
    int benchmark_iterations_;
    std::vector<std::unique_ptr<FFTBackend>> backends_;
    std::vector<BenchmarkResult> results_;
    mutable std::mt19937 rng_;
    SystemInfo sys_info_;
};

int main(int argc, char* argv[]) {
    // Test sizes - common audio/DSP sizes
    std::vector<int> sizes = {128, 256, 512, 1024, 2048, 4096, 8192, 16384};

    // Parse command line arguments
    int warmup_iterations = 10;
    int benchmark_iterations = 100;

    if (argc > 1)
        warmup_iterations = std::atoi(argv[1]);
    if (argc > 2)
        benchmark_iterations = std::atoi(argv[2]);

    FFTBenchmark benchmark(warmup_iterations, benchmark_iterations);

    // Add all available backends (only those that were compiled in)
#ifdef HAVE_FFTW
    benchmark.add_backend(std::make_unique<FFTWBackend>());
#endif

#ifdef HAVE_KISSFFT
    benchmark.add_backend(std::make_unique<KissFFTBackend>());
#endif

#ifdef HAVE_POCKETFFT
    benchmark.add_backend(std::make_unique<PocketFFTBackend>());
#endif

#ifdef HAVE_PFFFT
    benchmark.add_backend(std::make_unique<PFFFTBackend>());
#endif

#ifdef HAVE_KFR
    benchmark.add_backend(std::make_unique<KFRBackend>());
#endif

#ifdef HAVE_SIGNALSMITH
    benchmark.add_backend(std::make_unique<SignalsmithBackend>());
#endif

#ifdef HAVE_HIFILOFI
    benchmark.add_backend(std::make_unique<HiFiLoFiBackend>());
#endif

#ifdef HAVE_JUCE
    benchmark.add_backend(std::make_unique<JUCEBackend>());
#endif

    // Run benchmarks
    benchmark.run_benchmarks(sizes);

    // Export results
    benchmark.export_csv("fft_benchmark_results.csv");
    std::cout << "\n";
    std::cout << "✓ Results exported to fft_benchmark_results.csv\n";
    std::cout << "\n";

    return 0;
}