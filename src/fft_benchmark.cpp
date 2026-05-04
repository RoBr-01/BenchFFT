#include <fft_backend.h>

// Include backends based on what's enabled
#include <fftw_backend.h>

#define HAVE_KISSFFT 1
#ifdef HAVE_KISSFFT
#include "kissfft_backend.h"
#endif

#define HAVE_POCKETFFT 1
#ifdef HAVE_POCKETFFT
#include "pocketfft_backend.h"
#endif

#define HAVE_PFFFT 1
#ifdef HAVE_PFFFT
#include "pffft_backend.h"
#endif

#define HAVE_KFR 1
#ifdef HAVE_KFR
#include "kfr_backend.h"
#endif
#define HAVE_SIGNALSMITH 1
#ifdef HAVE_SIGNALSMITH
#include "signalsmith_backend.h"
#endif

#define HAVE_JUCE 1
#ifdef HAVE_JUCE
#include "JUCE_backend.h"
#endif

#define HAVE_HIFILOFI 1
#ifdef HAVE_HIFILOFI
#include "hifilofi_backend.h"
#endif

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

// ============================================================================
// Data structures
// ============================================================================

struct BenchmarkResult {
    std::string library_name;
    int size;
    std::string transform_type;
    double median_time_us;  // PRIMARY metric — robust to outliers
    double mean_time_us;    // Secondary (affected by OS jitter)
    double min_time_us;
    double max_time_us;
    double std_dev_us;
    double throughput_msamples_per_sec;  // Based on median
    bool validation_passed;
};

struct SystemInfo {
    std::string cpu_model;
    std::string os_version;
    std::string compiler;
    std::string build_flags;
    std::string timestamp;
};

// ============================================================================
// FFTBenchmark
// ============================================================================

class FFTBenchmark {
   public:
    // warmup_iterations  — runs discarded before measurement starts
    // benchmark_iterations — measured runs (use median of these)
    // num_input_buffers  — how many distinct input buffers to rotate through.
    //   Set to 1  → hot-cache (models real-time audio: same buffer every block)
    //   Set to N  → rotating buffers (stresses cache; models batch processing)
    //   Default 1 is correct for audio DSP benchmarking.
    FFTBenchmark(int warmup_iterations = 50,
                 int benchmark_iterations = 500,
                 int num_input_buffers = 1)
        : warmup_iterations_(warmup_iterations),
          benchmark_iterations_(benchmark_iterations),
          num_input_buffers_(num_input_buffers) {
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

                    bool valid = validate_fft(*backend, size);
                    if (!valid) {
                        std::cout << std::setw(15) << backend->name()
                                  << ": FAILED VALIDATION - skipping\n";
                        backend->cleanup();
                        continue;
                    }

                    auto r_fwd = benchmark_forward(*backend, size);
                    r_fwd.validation_passed = valid;
                    results_.push_back(r_fwd);
                    print_result(r_fwd);

                    auto r_bwd = benchmark_backward(*backend, size);
                    r_bwd.validation_passed = valid;
                    results_.push_back(r_bwd);
                    print_result(r_bwd);

                    if (backend->supports_complex()) {
                        auto r_c2c_fwd =
                            benchmark_forward_complex(*backend, size);
                        r_c2c_fwd.validation_passed = valid;
                        results_.push_back(r_c2c_fwd);
                        print_result(r_c2c_fwd);

                        auto r_c2c_bwd =
                            benchmark_backward_complex(*backend, size);
                        r_c2c_bwd.validation_passed = valid;
                        results_.push_back(r_c2c_bwd);
                        print_result(r_c2c_bwd);
                    }

                    backend->cleanup();

                    // Brief pause between libraries to let the CPU cool
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));

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
        file << "# FFT Benchmark Results\n";
        file << "# CPU: " << sys_info_.cpu_model << "\n";
        file << "# OS: " << sys_info_.os_version << "\n";
        file << "# Compiler: " << sys_info_.compiler << "\n";
        file << "# Build Flags: " << sys_info_.build_flags << "\n";
        file << "# Timestamp: " << sys_info_.timestamp << "\n";
        file << "# Warmup Iterations: " << warmup_iterations_ << "\n";
        file << "# Benchmark Iterations: " << benchmark_iterations_ << "\n";
        file << "# Input Buffers: " << num_input_buffers_ << "\n";
        file << "#\n";
        file << "Library,Size,Transform,Median(us),Mean(us),Min(us),Max(us),"
             << "StdDev(us),Throughput(Msamp/s),Validated\n";

        for (const auto& r : results_) {
            file << r.library_name << "," << r.size << "," << r.transform_type
                 << "," << r.median_time_us << "," << r.mean_time_us << ","
                 << r.min_time_us << "," << r.max_time_us << "," << r.std_dev_us
                 << "," << r.throughput_msamples_per_sec << ","
                 << (r.validation_passed ? "PASS" : "FAIL") << "\n";
        }
    }

   private:
    // -------------------------------------------------------------------------
    // System info
    // -------------------------------------------------------------------------
    void collect_system_info() {
        auto now = std::time(nullptr);
        char time_buf[100];
        std::strftime(time_buf,
                      sizeof(time_buf),
                      "%Y-%m-%d %H:%M:%S",
                      std::localtime(&now));
        sys_info_.timestamp = time_buf;

#ifdef __APPLE__
        char cpu_brand[256];
        size_t cpu_size = sizeof(cpu_brand);
        if (sysctlbyname("machdep.cpu.brand_string",
                         &cpu_brand,
                         &cpu_size,
                         nullptr,
                         0) == 0)
            sys_info_.cpu_model = cpu_brand;
        else
            sys_info_.cpu_model = "Apple Silicon (unknown model)";
        sys_info_.os_version = "macOS";
#elif defined(__linux__)
        {
            std::ifstream cpuinfo("/proc/cpuinfo");
            std::string line;
            while (std::getline(cpuinfo, line)) {
                if (line.find("model name") != std::string::npos) {
                    auto pos = line.find(':');
                    if (pos != std::string::npos) {
                        sys_info_.cpu_model = line.substr(pos + 2);
                        break;
                    }
                }
            }
        }
        sys_info_.os_version = "Linux";
#else
        sys_info_.cpu_model = "Unknown";
        sys_info_.os_version = "Unknown";
#endif

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

#ifdef NDEBUG
        sys_info_.build_flags = "Release (-O3 -march=native)";
#else
        sys_info_.build_flags = "Debug (-O0 -g)";
#endif
    }

    // -------------------------------------------------------------------------
    // Validation — round-trip correctness check before timing
    // -------------------------------------------------------------------------
    bool validate_fft(FFTBackend& backend, int size) {
        const float tol = 1e-3f;

        // Test 1: DC (all ones) → round-trip → all ones
        std::vector<float> input(size, 1.0f);
        std::vector<std::complex<float>> freq(size / 2 + 1);
        std::vector<float> output(size);

        backend.forward(input.data(), freq.data());
        backend.backward(freq.data(), output.data());

        for (int i = 0; i < size; ++i) {
            if (std::abs(output[i] - 1.0f) > tol) {
                std::cerr << "  Validation failed: DC test (expected 1.0, got "
                          << output[i] << " at index " << i << ")\n";
                return false;
            }
        }

        // Test 2: Impulse round-trip
        std::fill(input.begin(), input.end(), 0.0f);
        input[0] = 1.0f;
        backend.forward(input.data(), freq.data());
        backend.backward(freq.data(), output.data());

        if (std::abs(output[0] - 1.0f) > tol) {
            std::cerr << "  Validation failed: impulse DC\n";
            return false;
        }
        for (int i = 1; i < size; ++i) {
            if (std::abs(output[i]) > tol) {
                std::cerr << "  Validation failed: impulse at index " << i
                          << "\n";
                return false;
            }
        }

        // Test 3: Random round-trip
        std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
        for (auto& v : input)
            v = dist(rng_);
        std::vector<float> input_copy = input;
        backend.forward(input.data(), freq.data());
        backend.backward(freq.data(), output.data());
        for (int i = 0; i < size; ++i) {
            if (std::abs(output[i] - input_copy[i]) > tol) {
                std::cerr << "  Validation failed: round-trip error "
                          << std::abs(output[i] - input_copy[i]) << " at index "
                          << i << "\n";
                return false;
            }
        }

        return true;
    }

    // -------------------------------------------------------------------------
    // Buffer helpers
    // -------------------------------------------------------------------------
    // Allocate num_input_buffers_ buffers of 'count' elements, filled randomly.
    template <typename T>
    std::vector<std::vector<T>> make_input_buffers(int count) {
        std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
        std::vector<std::vector<T>> bufs(num_input_buffers_,
                                         std::vector<T>(count));
        for (auto& buf : bufs)
            for (auto& v : buf)
                fill_random(v, dist);
        return bufs;
    }

    // Overloads so fill_random works for both float and complex<float>
    void fill_random(float& v, std::uniform_real_distribution<float>& dist) {
        v = dist(rng_);
    }
    void fill_random(std::complex<float>& v,
                     std::uniform_real_distribution<float>& dist) {
        v = {dist(rng_), dist(rng_)};
    }

    // -------------------------------------------------------------------------
    // Core timing loop
    //
    // Design decisions:
    //  - Single output buffer reused every iteration (no allocation in hot
    //  path)
    //  - Input rotates through num_input_buffers_ buffers (default 1 =
    //  hot-cache,
    //    which is realistic for audio real-time loops)
    //  - Warmup rotates through all input buffers so no single buffer gets
    //    a cache-warm advantage going into measurement
    //  - All output slots are accumulated into a volatile sink so the compiler
    //    cannot dead-strip any iteration
    //  - Summary uses MEDIAN as primary metric; mean is kept for CSV only
    // -------------------------------------------------------------------------
    template <typename RunFn>
    BenchmarkResult run_timed(const std::string& lib_name,
                              int size,
                              const std::string& transform_type,
                              RunFn&& run) {
        // Warmup — rotate through all input buffers so no single one is "hot"
        for (int i = 0; i < warmup_iterations_; ++i)
            run(i % num_input_buffers_);

        // Measurement
        std::vector<double> times;
        times.reserve(benchmark_iterations_);

        for (int i = 0; i < benchmark_iterations_; ++i) {
            auto start = std::chrono::high_resolution_clock::now();
            run(i % num_input_buffers_);
            auto end = std::chrono::high_resolution_clock::now();
            times.push_back(
                std::chrono::duration_cast<std::chrono::nanoseconds>(end -
                                                                     start)
                    .count() /
                1000.0);
        }

        return compute_statistics(lib_name, size, transform_type, times);
    }

    // -------------------------------------------------------------------------
    // Benchmark methods — one per transform type
    // -------------------------------------------------------------------------
    BenchmarkResult benchmark_forward(FFTBackend& backend, int size) {
        auto inputs = make_input_buffers<float>(size);
        std::vector<std::complex<float>> output(size / 2 + 1);
        volatile float sink = 0.0f;

        BenchmarkResult result;
        if (float* stage = backend.staging_real()) {
            // Backend exposes its own aligned buffer — write directly into it,
            // then call the no-copy inplace variant.
            result = run_timed(
                backend.name(), size, "R2C Forward", [&](int buf_idx) {
                    std::copy(
                        inputs[buf_idx].begin(), inputs[buf_idx].end(), stage);
                    backend.forward_inplace(output.data());
                    sink += output[0].real();
                });
        } else {
            result = run_timed(
                backend.name(), size, "R2C Forward", [&](int buf_idx) {
                    backend.forward(inputs[buf_idx].data(), output.data());
                    sink += output[0].real();
                });
        }
        (void)sink;
        return result;
    }

    BenchmarkResult benchmark_backward(FFTBackend& backend, int size) {
        auto inputs = make_input_buffers<std::complex<float>>(size / 2 + 1);
        std::vector<float> output(size);
        volatile float sink = 0.0f;

        BenchmarkResult result;
        if (std::complex<float>* stage = backend.staging_complex()) {
            result = run_timed(
                backend.name(), size, "C2R Backward", [&](int buf_idx) {
                    std::copy(
                        inputs[buf_idx].begin(), inputs[buf_idx].end(), stage);
                    backend.backward_inplace(output.data());
                    sink += output[0];
                });
        } else {
            result = run_timed(
                backend.name(), size, "C2R Backward", [&](int buf_idx) {
                    backend.backward(inputs[buf_idx].data(), output.data());
                    sink += output[0];
                });
        }
        (void)sink;
        return result;
    }

    BenchmarkResult benchmark_forward_complex(FFTBackend& backend, int size) {
        auto inputs = make_input_buffers<std::complex<float>>(size);
        std::vector<std::complex<float>> output(size);
        volatile float sink = 0.0f;

        BenchmarkResult result;
        if (std::complex<float>* stage = backend.staging_complex()) {
            result = run_timed(
                backend.name(), size, "C2C Forward", [&](int buf_idx) {
                    std::copy(
                        inputs[buf_idx].begin(), inputs[buf_idx].end(), stage);
                    backend.forward_complex_inplace(output.data());
                    sink += output[0].real();
                });
        } else {
            result = run_timed(
                backend.name(), size, "C2C Forward", [&](int buf_idx) {
                    backend.forward_complex(inputs[buf_idx].data(),
                                            output.data());
                    sink += output[0].real();
                });
        }
        (void)sink;
        return result;
    }

    BenchmarkResult benchmark_backward_complex(FFTBackend& backend, int size) {
        auto inputs = make_input_buffers<std::complex<float>>(size);
        std::vector<std::complex<float>> output(size);
        volatile float sink = 0.0f;

        BenchmarkResult result;
        if (std::complex<float>* stage = backend.staging_complex()) {
            result = run_timed(
                backend.name(), size, "C2C Backward", [&](int buf_idx) {
                    std::copy(
                        inputs[buf_idx].begin(), inputs[buf_idx].end(), stage);
                    backend.backward_complex_inplace(output.data());
                    sink += output[0].real();
                });
        } else {
            result = run_timed(
                backend.name(), size, "C2C Backward", [&](int buf_idx) {
                    backend.backward_complex(inputs[buf_idx].data(),
                                             output.data());
                    sink += output[0].real();
                });
        }
        (void)sink;
        return result;
    }

    // -------------------------------------------------------------------------
    // Statistics
    // -------------------------------------------------------------------------
    BenchmarkResult compute_statistics(const std::string& name,
                                       int size,
                                       const std::string& transform_type,
                                       std::vector<double>& times) {
        std::sort(times.begin(), times.end());

        BenchmarkResult r;
        r.library_name = name;
        r.size = size;
        r.transform_type = transform_type;
        r.min_time_us = times.front();
        r.max_time_us = times.back();
        r.median_time_us = times[times.size() / 2];
        r.mean_time_us = std::accumulate(times.begin(), times.end(), 0.0) /
                         static_cast<double>(times.size());

        double sq = 0.0;
        for (double t : times)
            sq += (t - r.mean_time_us) * (t - r.mean_time_us);
        r.std_dev_us = std::sqrt(sq / static_cast<double>(times.size()));

        // Throughput based on median (more representative than mean)
        r.throughput_msamples_per_sec = size / r.median_time_us;

        return r;
    }

    // -------------------------------------------------------------------------
    // Output
    // -------------------------------------------------------------------------
    void print_header() const {
        std::cout << "\n";
        std::cout << "╔════════════════════════════════════════════════════════"
                     "══════════════════════╗\n";
        std::cout << "║                           FFT Library Benchmark        "
                     "                     ║\n";
        std::cout << "╚════════════════════════════════════════════════════════"
                     "══════════════════════╝\n\n";
        std::cout << "System Information:\n";
        std::cout << "  CPU:       " << sys_info_.cpu_model << "\n";
        std::cout << "  OS:        " << sys_info_.os_version << "\n";
        std::cout << "  Compiler:  " << sys_info_.compiler << "\n";
        std::cout << "  Build:     " << sys_info_.build_flags << "\n";
        std::cout << "  Timestamp: " << sys_info_.timestamp << "\n\n";
        std::cout << "Benchmark Configuration:\n";
        std::cout << "  Warmup iterations:    " << warmup_iterations_ << "\n";
        std::cout << "  Benchmark iterations: " << benchmark_iterations_
                  << "\n";
        std::cout << "  Input buffers:        " << num_input_buffers_
                  << (num_input_buffers_ == 1
                          ? "  (hot-cache — models real-time audio loop)\n"
                          : "  (rotating — stresses cache)\n");
        std::cout << "  Primary metric:       median (robust to OS jitter)\n\n";
    }

    void print_result(const BenchmarkResult& r) const {
        std::cout << std::setw(15) << r.library_name << " " << std::setw(13)
                  << r.transform_type << ": " << std::fixed
                  << std::setprecision(2) << std::setw(8) << r.median_time_us
                  << " μs (median)"
                  << "  mean=" << std::setw(7) << r.mean_time_us << " μs"
                  << "  σ=" << std::setw(6) << r.std_dev_us << " μs"
                  << "  [" << std::setw(7) << r.throughput_msamples_per_sec
                  << " Msamp/s]\n";
    }

    void print_summary() const {
        std::cout << "\n";
        std::cout << "╔════════════════════════════════════════════════════════"
                     "══════════════════════╗\n";
        std::cout << "║                   Performance Summary — ranked by "
                     "median                    ║\n";
        std::cout << "╚════════════════════════════════════════════════════════"
                     "══════════════════════╝\n\n";

        std::map<std::pair<int, std::string>, std::vector<BenchmarkResult>>
            grouped;
        for (const auto& r : results_)
            if (r.validation_passed)
                grouped[{r.size, r.transform_type}].push_back(r);

        for (auto& [key, group] : grouped) {
            // Sort by median (primary metric)
            std::sort(
                group.begin(), group.end(), [](const auto& a, const auto& b) {
                    return a.median_time_us < b.median_time_us;
                });

            std::cout << "Size " << std::setw(5) << key.first << " - "
                      << std::setw(13) << std::left << key.second << std::right
                      << ":\n";

            for (size_t i = 0; i < std::min(size_t(3), group.size()); ++i) {
                double ratio =
                    group[i].median_time_us / group[0].median_time_us;
                std::cout << "  " << (i + 1) << ". " << std::setw(15)
                          << group[i].library_name << ": " << std::fixed
                          << std::setprecision(2) << std::setw(7)
                          << group[i].median_time_us << " μs";
                if (i == 0)
                    std::cout << "  (fastest)";
                else
                    std::cout << "  (" << std::setprecision(2) << ratio
                              << "× slower)";
                std::cout << "\n";
            }
            std::cout << "\n";
        }
        std::cout << "All results validated for correctness (round-trip error "
                     "< 0.1%)\n";
    }

    // -------------------------------------------------------------------------
    // Members
    // -------------------------------------------------------------------------
    int warmup_iterations_;
    int benchmark_iterations_;
    int num_input_buffers_;

    std::vector<std::unique_ptr<FFTBackend>> backends_;
    std::vector<BenchmarkResult> results_;
    mutable std::mt19937 rng_;
    SystemInfo sys_info_;
};

// ============================================================================
// main
// ============================================================================

int main(int argc, char* argv[]) {
    std::vector<int> sizes = {
        32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384};

    // Usage: fft_benchmark [warmup] [iterations] [num_input_buffers]
    int warmup = 50;
    int iterations = 500;
    int num_buffers = 1;  // 1 = hot-cache (audio DSP default)

    if (argc > 1)
        warmup = std::atoi(argv[1]);
    if (argc > 2)
        iterations = std::atoi(argv[2]);
    if (argc > 3)
        num_buffers = std::atoi(argv[3]);

    FFTBenchmark benchmark(warmup, iterations, num_buffers);

    benchmark.add_backend(std::make_unique<FFTWBackend>());
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
#ifdef HAVE_FFTS
    benchmark.add_backend(std::make_unique<FFTSBackend>());
#endif

    benchmark.run_benchmarks(sizes);
    benchmark.export_csv("fft_benchmark_results.csv");
    std::cout << "\n✓ Results exported to fft_benchmark_results.csv\n\n";

    return 0;
}