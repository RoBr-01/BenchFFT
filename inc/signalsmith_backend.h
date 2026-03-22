#pragma once

#include <vector>

#include "fft.h"
#include "fft_backend.h"

class SignalsmithBackend : public FFTBackend {
   public:
    SignalsmithBackend() : real_fft_(0), complex_fft_(0) {
        // Will be resized in setup()
    }

    ~SignalsmithBackend() override {
        cleanup();
    }

    void setup(int size) override {
        cleanup();
        size_ = size;

        // Use RealFFT for real-to-complex transforms
        real_fft_.setSize(size);

        // Use FFT for complex-to-complex transforms
        complex_fft_.setSize(size);

        // Allocate buffers
        real_buffer_.resize(size);
        complex_buffer_.resize(size);
        complex_input_buffer_.resize(
            size);  // staging buffer for C2C (avoids per-call heap alloc)
    }

    void cleanup() override {
        real_buffer_.clear();
        complex_buffer_.clear();
    }

    void forward(const float* in, std::complex<float>* out) override {
        // Copy input
        std::copy(in, in + size_, real_buffer_.begin());

        // Perform real FFT using RealFFT class
        real_fft_.fft(real_buffer_.data(), complex_buffer_.data());

        // Copy output (RealFFT produces size/2 + 1 complex values)
        std::copy(complex_buffer_.begin(),
                  complex_buffer_.begin() + size_ / 2 + 1,
                  out);
    }

    void backward(const std::complex<float>* in, float* out) override {
        // Copy input (size/2 + 1 complex values)
        std::copy(in, in + size_ / 2 + 1, complex_buffer_.begin());

        // Perform inverse real FFT
        real_fft_.ifft(complex_buffer_.data(), real_buffer_.data());

        // Signalsmith's ifft() does NOT normalize, so we must divide by size
        for (int i = 0; i < size_; ++i) {
            out[i] = real_buffer_[i] / size_;
        }
    }

    void forward_complex(const std::complex<float>* in,
                         std::complex<float>* out) override {
        // Copy into pre-allocated input buffer (complex_fft_.fft() may modify
        // its input in-place; we must not clobber the caller's buffer)
        std::copy(in, in + size_, complex_input_buffer_.begin());
        complex_fft_.fft(complex_input_buffer_.data(), out);
    }

    void backward_complex(const std::complex<float>* in,
                          std::complex<float>* out) override {
        std::copy(in, in + size_, complex_input_buffer_.begin());
        complex_fft_.ifft(complex_input_buffer_.data(), out);
        // Signalsmith ifft() does NOT normalize
        const float inv = 1.0f / static_cast<float>(size_);
        for (int i = 0; i < size_; ++i)
            out[i] *= inv;
    }

    std::string name() const override {
        return "Signalsmith";
    }

    bool supports_size(int size) const override {
        // Signalsmith FFT works with any size, but is fast for 2^a * 3^b
        return size > 0;
    }

    bool supports_complex() const override {
        return true;
    }

   private:
    signalsmith::fft::RealFFT<float> real_fft_;  // For real-to-complex
    signalsmith::fft::FFT<float> complex_fft_;   // For complex-to-complex
    std::vector<float> real_buffer_;
    std::vector<std::complex<float>> complex_buffer_;
    std::vector<std::complex<float>>
        complex_input_buffer_;  // Avoids per-call allocation
};