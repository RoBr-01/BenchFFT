#pragma once

#include "fft_backend.h"
#include <fftw3.h>

class FFTWBackend : public FFTBackend {
public:
    ~FFTWBackend() override {
        cleanup();
    }
    
    void setup(int size) override {
        cleanup();
        size_ = size;
        
        // Allocate aligned memory for FFTW
        real_in_ = fftwf_alloc_real(size);
        complex_out_ = reinterpret_cast<std::complex<float>*>(fftwf_alloc_complex(size));
        complex_in_ = reinterpret_cast<std::complex<float>*>(fftwf_alloc_complex(size));
        real_out_ = fftwf_alloc_real(size);
        
        // Create plans (FFTW_MEASURE for best performance, FFTW_ESTIMATE for faster setup)
        plan_forward_ = fftwf_plan_dft_r2c_1d(size, real_in_, 
                                              reinterpret_cast<fftwf_complex*>(complex_out_),
                                              FFTW_MEASURE);
        plan_backward_ = fftwf_plan_dft_c2r_1d(size,
                                               reinterpret_cast<fftwf_complex*>(complex_in_),
                                               real_out_,
                                               FFTW_MEASURE);
        plan_forward_c2c_ = fftwf_plan_dft_1d(size,
                                              reinterpret_cast<fftwf_complex*>(complex_in_),
                                              reinterpret_cast<fftwf_complex*>(complex_out_),
                                              FFTW_FORWARD, FFTW_MEASURE);
        plan_backward_c2c_ = fftwf_plan_dft_1d(size,
                                               reinterpret_cast<fftwf_complex*>(complex_in_),
                                               reinterpret_cast<fftwf_complex*>(complex_out_),
                                               FFTW_BACKWARD, FFTW_MEASURE);
    }
    
    void cleanup() override {
        if (plan_forward_) fftwf_destroy_plan(plan_forward_);
        if (plan_backward_) fftwf_destroy_plan(plan_backward_);
        if (plan_forward_c2c_) fftwf_destroy_plan(plan_forward_c2c_);
        if (plan_backward_c2c_) fftwf_destroy_plan(plan_backward_c2c_);
        if (real_in_) fftwf_free(real_in_);
        if (complex_out_) fftwf_free(complex_out_);
        if (complex_in_) fftwf_free(complex_in_);
        if (real_out_) fftwf_free(real_out_);
        
        plan_forward_ = nullptr;
        plan_backward_ = nullptr;
        plan_forward_c2c_ = nullptr;
        plan_backward_c2c_ = nullptr;
        real_in_ = nullptr;
        complex_out_ = nullptr;
        complex_in_ = nullptr;
        real_out_ = nullptr;
    }
    
    void forward(const float* in, std::complex<float>* out) override {
        std::copy(in, in + size_, real_in_);
        fftwf_execute(plan_forward_);
        std::copy(complex_out_, complex_out_ + size_/2 + 1, out);
    }
    
    void backward(const std::complex<float>* in, float* out) override {
        std::copy(in, in + size_/2 + 1, complex_in_);
        fftwf_execute(plan_backward_);
        // FFTW doesn't normalize, so we need to divide by size
        for (int i = 0; i < size_; ++i) {
            out[i] = real_out_[i] / size_;
        }
    }
    
    void forward_complex(const std::complex<float>* in, 
                        std::complex<float>* out) override {
        std::copy(in, in + size_, complex_in_);
        fftwf_execute(plan_forward_c2c_);
        std::copy(complex_out_, complex_out_ + size_, out);
    }
    
    void backward_complex(const std::complex<float>* in, 
                         std::complex<float>* out) override {
        std::copy(in, in + size_, complex_in_);
        fftwf_execute(plan_backward_c2c_);
        // Normalize
        for (int i = 0; i < size_; ++i) {
            out[i] = complex_out_[i] / static_cast<float>(size_);
        }
    }
    
    std::string name() const override { return "FFTW"; }
    
private:
    fftwf_plan plan_forward_ = nullptr;
    fftwf_plan plan_backward_ = nullptr;
    fftwf_plan plan_forward_c2c_ = nullptr;
    fftwf_plan plan_backward_c2c_ = nullptr;
    float* real_in_ = nullptr;
    std::complex<float>* complex_out_ = nullptr;
    std::complex<float>* complex_in_ = nullptr;
    float* real_out_ = nullptr;
};