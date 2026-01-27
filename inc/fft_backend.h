#pragma once

#include <complex>
#include <string>
#include <vector>

// Abstract interface for FFT libraries
class FFTBackend {
public:
    virtual ~FFTBackend() = default;
    
    // Setup/teardown
    virtual void setup(int size) = 0;
    virtual void cleanup() = 0;
    
    // Forward FFT: time domain -> frequency domain
    virtual void forward(const float* in, std::complex<float>* out) = 0;
    
    // Backward FFT: frequency domain -> time domain
    virtual void backward(const std::complex<float>* in, float* out) = 0;
    
    // Complex-to-complex transforms (not all libraries support this efficiently)
    virtual void forward_complex(const std::complex<float>* in, 
                                std::complex<float>* out) = 0;
    virtual void backward_complex(const std::complex<float>* in, 
                                 std::complex<float>* out) = 0;
    
    // Metadata
    virtual std::string name() const = 0;
    virtual bool supports_size(int size) const { return true; }
    virtual bool supports_complex() const { return true; }
    
    // Get current size
    int get_size() const { return size_; }
    
protected:
    int size_ = 0;
};