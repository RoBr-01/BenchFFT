#pragma once

#include "fft_backend.h"
#include "pocketfft_hdronly.h"
#include <vector>

class PocketFFTBackend : public FFTBackend {
public:
    void setup(int size) override {
        size_ = size;
        shape_ = {static_cast<size_t>(size)};
        stride_real_ = {sizeof(float)};
        stride_complex_ = {sizeof(std::complex<float>)};
        axes_ = {0};
        
        real_buffer_.resize(size);
        complex_buffer_.resize(size);
    }
    
    void cleanup() override {
        real_buffer_.clear();
        complex_buffer_.clear();
    }
    
    void forward(const float* in, std::complex<float>* out) override {
        std::copy(in, in + size_, real_buffer_.begin());
        
        pocketfft::r2c(shape_, stride_real_, stride_complex_, axes_,
                      pocketfft::FORWARD,
                      real_buffer_.data(), out, 1.0f);
    }
    
    void backward(const std::complex<float>* in, float* out) override {
        std::copy(in, in + size_/2 + 1, complex_buffer_.begin());
        
        pocketfft::c2r(shape_, stride_complex_, stride_real_, axes_,
                      pocketfft::BACKWARD,
                      complex_buffer_.data(), out, 1.0f / size_);
    }
    
    void forward_complex(const std::complex<float>* in, 
                        std::complex<float>* out) override {
        std::copy(in, in + size_, complex_buffer_.begin());
        
        pocketfft::c2c(shape_, stride_complex_, stride_complex_, axes_,
                      pocketfft::FORWARD,
                      complex_buffer_.data(), out, 1.0f);
    }
    
    void backward_complex(const std::complex<float>* in, 
                         std::complex<float>* out) override {
        std::copy(in, in + size_, complex_buffer_.begin());
        
        pocketfft::c2c(shape_, stride_complex_, stride_complex_, axes_,
                      pocketfft::BACKWARD,
                      complex_buffer_.data(), out, 1.0f / size_);
    }
    
    std::string name() const override { return "PocketFFT"; }
    
private:
    pocketfft::shape_t shape_;
    pocketfft::stride_t stride_real_;
    pocketfft::stride_t stride_complex_;
    pocketfft::shape_t axes_;
    
    std::vector<float> real_buffer_;
    std::vector<std::complex<float>> complex_buffer_;
};