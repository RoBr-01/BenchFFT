#pragma once

#ifdef HAVE_HIFILOFI

#include "fft_backend.h"
#include "AudioFFT.h"
#include <vector>

class HiFiLoFiBackend : public FFTBackend {
public:
    HiFiLoFiBackend() = default;
    
    ~HiFiLoFiBackend() override {
        cleanup();
    }
    
    void setup(int size) override {
        cleanup();
        size_ = size;
        
        // Initialize AudioFFT
        fft_.init(size);
        
        // Allocate buffers
        real_buffer_.resize(size);
        re_buffer_.resize(fft_.ComplexSize());  // size/2 + 1
        im_buffer_.resize(fft_.ComplexSize());
    }
    
    void cleanup() override {
        real_buffer_.clear();
        re_buffer_.clear();
        im_buffer_.clear();
    }
    
    void forward(const float* in, std::complex<float>* out) override {
        // Copy input
        std::copy(in, in + size_, real_buffer_.begin());
        
        // Perform FFT - AudioFFT uses separate real/imag arrays
        fft_.fft(real_buffer_.data(), re_buffer_.data(), im_buffer_.data());
        
        // Combine into std::complex
        for (size_t i = 0; i < fft_.ComplexSize(); ++i) {
            out[i] = std::complex<float>(re_buffer_[i], im_buffer_[i]);
        }
    }
    
    void backward(const std::complex<float>* in, float* out) override {
        // Split std::complex into separate real/imag arrays
        for (size_t i = 0; i < fft_.ComplexSize(); ++i) {
            re_buffer_[i] = in[i].real();
            im_buffer_[i] = in[i].imag();
        }
        
        // Perform IFFT
        fft_.ifft(real_buffer_.data(), re_buffer_.data(), im_buffer_.data());
        
        // Copy output (AudioFFT normalizes automatically)
        std::copy(real_buffer_.begin(), real_buffer_.end(), out);
    }
    
    void forward_complex(const std::complex<float>* in, 
                        std::complex<float>* out) override {
        // AudioFFT doesn't have complex-to-complex FFT
        // We could implement it by treating it as two real FFTs, but
        // for now, just throw an exception or leave unimplemented
        throw std::runtime_error("HiFi-LoFi AudioFFT does not support complex-to-complex transforms");
    }
    
    void backward_complex(const std::complex<float>* in, 
                         std::complex<float>* out) override {
        throw std::runtime_error("HiFi-LoFi AudioFFT does not support complex-to-complex transforms");
    }
    
    std::string name() const override { return "HiFi-LoFi"; }
    
    bool supports_size(int size) const override {
        // AudioFFT works with any size (uses FFTReal internally)
        return size > 0;
    }
    
    bool supports_complex() const override {
        return false;  // Only real-to-complex
    }
    
private:
    audiofft::AudioFFT fft_;
    std::vector<float> real_buffer_;
    std::vector<float> re_buffer_;  // Real part of complex output
    std::vector<float> im_buffer_;  // Imaginary part of complex output
};

#endif // HAVE_HIFILOFI
