#pragma once

#include "fft_backend.h"
#include "kiss_fft.h"
#include "kiss_fftr.h"
#include <vector>

class KissFFTBackend : public FFTBackend {
public:
    ~KissFFTBackend() override {
        cleanup();
    }
    
    void setup(int size) override {
        cleanup();
        size_ = size;
        
        // Setup real FFT
        cfg_forward_ = kiss_fftr_alloc(size, 0, nullptr, nullptr);
        cfg_backward_ = kiss_fftr_alloc(size, 1, nullptr, nullptr);
        
        // Setup complex FFT
        cfg_forward_c2c_ = kiss_fft_alloc(size, 0, nullptr, nullptr);
        cfg_backward_c2c_ = kiss_fft_alloc(size, 1, nullptr, nullptr);
        
        // Allocate buffers
        real_buffer_.resize(size);
        complex_buffer_.resize(size);
        kiss_complex_buffer_.resize(size);
    }
    
    void cleanup() override {
        if (cfg_forward_) kiss_fftr_free(cfg_forward_);
        if (cfg_backward_) kiss_fftr_free(cfg_backward_);
        if (cfg_forward_c2c_) kiss_fft_free(cfg_forward_c2c_);
        if (cfg_backward_c2c_) kiss_fft_free(cfg_backward_c2c_);
        
        cfg_forward_ = nullptr;
        cfg_backward_ = nullptr;
        cfg_forward_c2c_ = nullptr;
        cfg_backward_c2c_ = nullptr;
    }
    
    void forward(const float* in, std::complex<float>* out) override {
        kiss_fftr(cfg_forward_, in, 
                  reinterpret_cast<kiss_fft_cpx*>(out));
    }
    
    void backward(const std::complex<float>* in, float* out) override {
        kiss_fftri(cfg_backward_, 
                   reinterpret_cast<const kiss_fft_cpx*>(in), 
                   out);
        // Normalize
        for (int i = 0; i < size_; ++i) {
            out[i] /= size_;
        }
    }
    
    void forward_complex(const std::complex<float>* in, 
                        std::complex<float>* out) override {
        kiss_fft(cfg_forward_c2c_, 
                 reinterpret_cast<const kiss_fft_cpx*>(in),
                 reinterpret_cast<kiss_fft_cpx*>(out));
    }
    
    void backward_complex(const std::complex<float>* in, 
                         std::complex<float>* out) override {
        kiss_fft(cfg_backward_c2c_, 
                 reinterpret_cast<const kiss_fft_cpx*>(in),
                 reinterpret_cast<kiss_fft_cpx*>(out));
        // Normalize
        for (int i = 0; i < size_; ++i) {
            out[i] /= size_;
        }
    }
    
    std::string name() const override { return "KissFFT"; }
    
private:
    kiss_fftr_cfg cfg_forward_ = nullptr;
    kiss_fftr_cfg cfg_backward_ = nullptr;
    kiss_fft_cfg cfg_forward_c2c_ = nullptr;
    kiss_fft_cfg cfg_backward_c2c_ = nullptr;
    
    std::vector<float> real_buffer_;
    std::vector<std::complex<float>> complex_buffer_;
    std::vector<kiss_fft_cpx> kiss_complex_buffer_;
};