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

// These are in other_backends.h, which handles its own ifdefs
#include "other_backends.h"

#include <iostream>
#include <iomanip>
#include <chrono>
#include <vector>
#include <memory>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <random>
#include <fstream>
#include <map>

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
};

class FFTBenchmark {
public:
    FFTBenchmark(int warmup_iterations = 10, int benchmark_iterations = 100)
        : warmup_iterations_(warmup_iterations)
        , benchmark_iterations_(benchmark_iterations)
    {
        // Initialize random number generator for test data
        rng_.seed(42);
    }
    
    void add_backend(std::unique_ptr<FFTBackend> backend) {
        backends_.push_back(std::move(backend));
    }
    
    void run_benchmarks(const std::vector<int>& sizes) {
        std::cout << "FFT Library Benchmark\n";
        std::cout << "=====================\n";
        std::cout << "Warmup iterations: " << warmup_iterations_ << "\n";
        std::cout << "Benchmark iterations: " << benchmark_iterations_ << "\n\n";
        
        for (int size : sizes) {
            std::cout << "Testing size: " << size << "\n";
            std::cout << std::string(60, '-') << "\n";
            
            for (auto& backend : backends_) {
                if (!backend->supports_size(size)) {
                    std::cout << std::setw(15) << backend->name() 
                             << ": SKIPPED (unsupported size)\n";
                    continue;
                }
                
                try {
                    backend->setup(size);
                    
                    // Test real-to-complex forward transform
                    auto result_forward = benchmark_forward(*backend, size);
                    results_.push_back(result_forward);
                    print_result(result_forward);
                    
                    // Test complex-to-real backward transform
                    auto result_backward = benchmark_backward(*backend, size);
                    results_.push_back(result_backward);
                    print_result(result_backward);
                    
                    // Test complex-to-complex transforms if supported
                    if (backend->supports_complex()) {
                        auto result_c2c_fwd = benchmark_forward_complex(*backend, size);
                        results_.push_back(result_c2c_fwd);
                        print_result(result_c2c_fwd);
                        
                        auto result_c2c_bwd = benchmark_backward_complex(*backend, size);
                        results_.push_back(result_c2c_bwd);
                        print_result(result_c2c_bwd);
                    }
                    
                    backend->cleanup();
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
        file << "Library,Size,Transform,Mean(us),Median(us),Min(us),Max(us),"
             << "StdDev(us),Throughput(Msamp/s)\n";
        
        for (const auto& result : results_) {
            file << result.library_name << ","
                 << result.size << ","
                 << result.transform_type << ","
                 << result.mean_time_us << ","
                 << result.median_time_us << ","
                 << result.min_time_us << ","
                 << result.max_time_us << ","
                 << result.std_dev_us << ","
                 << result.throughput_msamples_per_sec << "\n";
        }
    }

private:
    BenchmarkResult benchmark_forward(FFTBackend& backend, int size) {
        // Generate random input data
        std::vector<float> input(size);
        std::vector<std::complex<float>> output(size/2 + 1);
        
        std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
        for (auto& val : input) {
            val = dist(rng_);
        }
        
        // Warmup
        for (int i = 0; i < warmup_iterations_; ++i) {
            backend.forward(input.data(), output.data());
        }
        
        // Benchmark
        std::vector<double> times;
        times.reserve(benchmark_iterations_);
        
        for (int i = 0; i < benchmark_iterations_; ++i) {
            auto start = std::chrono::high_resolution_clock::now();
            backend.forward(input.data(), output.data());
            auto end = std::chrono::high_resolution_clock::now();
            
            auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
            times.push_back(duration.count() / 1000.0); // Convert to microseconds
        }
        
        return compute_statistics(backend.name(), size, "R2C Forward", times);
    }
    
    BenchmarkResult benchmark_backward(FFTBackend& backend, int size) {
        std::vector<std::complex<float>> input(size/2 + 1);
        std::vector<float> output(size);
        
        std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
        for (auto& val : input) {
            val = std::complex<float>(dist(rng_), dist(rng_));
        }
        
        // Warmup
        for (int i = 0; i < warmup_iterations_; ++i) {
            backend.backward(input.data(), output.data());
        }
        
        // Benchmark
        std::vector<double> times;
        times.reserve(benchmark_iterations_);
        
        for (int i = 0; i < benchmark_iterations_; ++i) {
            auto start = std::chrono::high_resolution_clock::now();
            backend.backward(input.data(), output.data());
            auto end = std::chrono::high_resolution_clock::now();
            
            auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
            times.push_back(duration.count() / 1000.0);
        }
        
        return compute_statistics(backend.name(), size, "C2R Backward", times);
    }
    
    BenchmarkResult benchmark_forward_complex(FFTBackend& backend, int size) {
        std::vector<std::complex<float>> input(size);
        std::vector<std::complex<float>> output(size);
        
        std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
        for (auto& val : input) {
            val = std::complex<float>(dist(rng_), dist(rng_));
        }
        
        // Warmup
        for (int i = 0; i < warmup_iterations_; ++i) {
            backend.forward_complex(input.data(), output.data());
        }
        
        // Benchmark
        std::vector<double> times;
        times.reserve(benchmark_iterations_);
        
        for (int i = 0; i < benchmark_iterations_; ++i) {
            auto start = std::chrono::high_resolution_clock::now();
            backend.forward_complex(input.data(), output.data());
            auto end = std::chrono::high_resolution_clock::now();
            
            auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
            times.push_back(duration.count() / 1000.0);
        }
        
        return compute_statistics(backend.name(), size, "C2C Forward", times);
    }
    
    BenchmarkResult benchmark_backward_complex(FFTBackend& backend, int size) {
        std::vector<std::complex<float>> input(size);
        std::vector<std::complex<float>> output(size);
        
        std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
        for (auto& val : input) {
            val = std::complex<float>(dist(rng_), dist(rng_));
        }
        
        // Warmup
        for (int i = 0; i < warmup_iterations_; ++i) {
            backend.backward_complex(input.data(), output.data());
        }
        
        // Benchmark
        std::vector<double> times;
        times.reserve(benchmark_iterations_);
        
        for (int i = 0; i < benchmark_iterations_; ++i) {
            auto start = std::chrono::high_resolution_clock::now();
            backend.backward_complex(input.data(), output.data());
            auto end = std::chrono::high_resolution_clock::now();
            
            auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
            times.push_back(duration.count() / 1000.0);
        }
        
        return compute_statistics(backend.name(), size, "C2C Backward", times);
    }
    
    BenchmarkResult compute_statistics(const std::string& name, int size,
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
        result.mean_time_us = std::accumulate(times.begin(), times.end(), 0.0) / times.size();
        
        // Calculate standard deviation
        double sq_sum = 0.0;
        for (double time : times) {
            sq_sum += (time - result.mean_time_us) * (time - result.mean_time_us);
        }
        result.std_dev_us = std::sqrt(sq_sum / times.size());
        
        // Calculate throughput (million samples per second)
        result.throughput_msamples_per_sec = (size / result.mean_time_us);
        
        return result;
    }
    
    void print_result(const BenchmarkResult& result) const {
        std::cout << std::setw(15) << result.library_name 
                 << " " << std::setw(12) << result.transform_type
                 << ": " << std::fixed << std::setprecision(2)
                 << std::setw(8) << result.mean_time_us << " μs"
                 << " (median: " << std::setw(8) << result.median_time_us << " μs)"
                 << " [" << std::setw(6) << result.throughput_msamples_per_sec << " Msamp/s]\n";
    }
    
    void print_summary() const {
        std::cout << "\n";
        std::cout << "Summary - Fastest Libraries by Transform Type and Size\n";
        std::cout << std::string(80, '=') << "\n";
        
        // Group results by size and transform type
        std::map<std::pair<int, std::string>, std::vector<BenchmarkResult>> grouped;
        for (const auto& result : results_) {
            grouped[{result.size, result.transform_type}].push_back(result);
        }
        
        for (auto& [key, group] : grouped) {
            std::sort(group.begin(), group.end(), 
                     [](const auto& a, const auto& b) { 
                         return a.mean_time_us < b.mean_time_us; 
                     });
            
            std::cout << "Size " << key.first << " - " << key.second << ":\n";
            for (size_t i = 0; i < std::min(size_t(3), group.size()); ++i) {
                std::cout << "  " << (i+1) << ". " << std::setw(15) << group[i].library_name
                         << ": " << std::fixed << std::setprecision(2)
                         << group[i].mean_time_us << " μs\n";
            }
            std::cout << "\n";
        }
    }
    
    int warmup_iterations_;
    int benchmark_iterations_;
    std::vector<std::unique_ptr<FFTBackend>> backends_;
    std::vector<BenchmarkResult> results_;
    std::mt19937 rng_;
};

int main(int argc, char* argv[]) {
    // Test sizes - common audio/DSP sizes
    std::vector<int> sizes = {
        128, 256, 512, 1024, 2048, 4096, 8192, 16384
    };
    
    // Parse command line arguments
    int warmup_iterations = 10;
    int benchmark_iterations = 100;
    
    if (argc > 1) warmup_iterations = std::atoi(argv[1]);
    if (argc > 2) benchmark_iterations = std::atoi(argv[2]);
    
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
    std::cout << "\nResults exported to fft_benchmark_results.csv\n";
    
    return 0;
}