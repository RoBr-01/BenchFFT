#pragma once

#include "fft_backend.h"
#include "pffft.h"
#include <vector>

class PFFFTBackend : public FFTBackend {
public:
    ~PFFFTBackend() override {
        cleanup();  // Safe to call - cleanup is idempotent
    }
    
    void setup(int size) override {
        cleanup();
        size_ = size;
        
        // PFFFT requires size to be a multiple of 16 for SIMD
        setup_real_ = pffft_new_setup(size, PFFFT_REAL);
        setup_complex_ = pffft_new_setup(size, PFFFT_COMPLEX);
        
        // Allocate aligned work buffers
        work_buffer_ = static_cast<float*>(pffft_aligned_malloc(size * sizeof(float) * 2));
        input_buffer_ = static_cast<float*>(pffft_aligned_malloc(size * sizeof(float) * 2));
        output_buffer_ = static_cast<float*>(pffft_aligned_malloc(size * sizeof(float) * 2));
    }
    
    void cleanup() override {
        // Set to nullptr INSIDE the if blocks to make this truly idempotent
        if (setup_real_) {
            pffft_destroy_setup(setup_real_);
            setup_real_ = nullptr;
        }
        if (setup_complex_) {
            pffft_destroy_setup(setup_complex_);
            setup_complex_ = nullptr;
        }
        if (work_buffer_) {
            pffft_aligned_free(work_buffer_);
            work_buffer_ = nullptr;
        }
        if (input_buffer_) {
            pffft_aligned_free(input_buffer_);
            input_buffer_ = nullptr;
        }
        if (output_buffer_) {
            pffft_aligned_free(output_buffer_);
            output_buffer_ = nullptr;
        }
    }
    
    void forward(const float* in, std::complex<float>* out) override {
        std::copy(in, in + size_, input_buffer_);
        
        pffft_transform_ordered(setup_real_, input_buffer_, output_buffer_,
                               work_buffer_, PFFFT_FORWARD);
        
        // PFFFT stores real FFT in a packed format: [r0, r1, i1, r2, i2, ..., rN/2]
        // Convert to standard complex format
        out[0] = std::complex<float>(output_buffer_[0], 0);
        for (int i = 1; i < size_/2; ++i) {
            out[i] = std::complex<float>(output_buffer_[2*i], output_buffer_[2*i+1]);
        }
        out[size_/2] = std::complex<float>(output_buffer_[1], 0);
    }
    
    void backward(const std::complex<float>* in, float* out) override {
        // Convert from standard complex format to PFFFT packed format
        output_buffer_[0] = in[0].real();
        output_buffer_[1] = in[size_/2].real();
        for (int i = 1; i < size_/2; ++i) {
            output_buffer_[2*i] = in[i].real();
            output_buffer_[2*i+1] = in[i].imag();
        }
        
        pffft_transform_ordered(setup_real_, output_buffer_, input_buffer_,
                               work_buffer_, PFFFT_BACKWARD);
        
        // Normalize
        for (int i = 0; i < size_; ++i) {
            out[i] = input_buffer_[i] / size_;
        }
    }
    
    void forward_complex(const std::complex<float>* in, 
                        std::complex<float>* out) override {
        std::copy(reinterpret_cast<const float*>(in),
                 reinterpret_cast<const float*>(in) + size_ * 2,
                 input_buffer_);
        
        pffft_transform_ordered(setup_complex_, input_buffer_, output_buffer_,
                               work_buffer_, PFFFT_FORWARD);
        
        std::copy(output_buffer_, output_buffer_ + size_ * 2,
                 reinterpret_cast<float*>(out));
    }
    
    void backward_complex(const std::complex<float>* in, 
                         std::complex<float>* out) override {
        std::copy(reinterpret_cast<const float*>(in),
                 reinterpret_cast<const float*>(in) + size_ * 2,
                 input_buffer_);
        
        pffft_transform_ordered(setup_complex_, input_buffer_, output_buffer_,
                               work_buffer_, PFFFT_BACKWARD);
        
        // Normalize
        for (int i = 0; i < size_ * 2; ++i) {
            output_buffer_[i] /= size_;
        }
        
        std::copy(output_buffer_, output_buffer_ + size_ * 2,
                 reinterpret_cast<float*>(out));
    }
    
    std::string name() const override { return "PFFFT"; }
    
    bool supports_size(int size) const override {
        // PFFFT requires sizes to be multiples of 16 for optimal performance
        return (size % 16 == 0) || (size >= 32 && (size & (size - 1)) == 0);
    }
    
private:
    PFFFT_Setup* setup_real_ = nullptr;
    PFFFT_Setup* setup_complex_ = nullptr;
    float* work_buffer_ = nullptr;
    float* input_buffer_ = nullptr;
    float* output_buffer_ = nullptr;
};
