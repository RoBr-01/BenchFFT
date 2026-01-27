#pragma once

#include "fft_backend.h"
#include <vector>

// Note: These are template implementations. You'll need to include the actual library headers
// and adjust based on their specific APIs

// Signalsmith DSP includes an FFT implementation
// https://github.com/Signalsmith-Audio/dsp

#ifdef HAVE_SIGNALSMITH
#include <signalsmith-dsp/fft.h>

class SignalsmithBackend : public FFTBackend {
public:
    void setup(int size) override {
        size_ = size;
        fft_.setSize(size);
        real_buffer_.resize(size);
        complex_buffer_.resize(size);
    }
    
    void cleanup() override {
        real_buffer_.clear();
        complex_buffer_.clear();
    }
    
    void forward(const float* in, std::complex<float>* out) override {
        std::copy(in, in + size_, real_buffer_.begin());
        fft_.fft(real_buffer_, complex_buffer_);
        std::copy(complex_buffer_.begin(), complex_buffer_.end(), out);
    }
    
    void backward(const std::complex<float>* in, float* out) override {
        std::copy(in, in + size_, complex_buffer_.begin());
        fft_.ifft(complex_buffer_, real_buffer_);
        std::copy(real_buffer_.begin(), real_buffer_.end(), out);
    }
    
    void forward_complex(const std::complex<float>* in, 
                        std::complex<float>* out) override {
        std::copy(in, in + size_, complex_buffer_.begin());
        fft_.fft(complex_buffer_, complex_buffer_);
        std::copy(complex_buffer_.begin(), complex_buffer_.end(), out);
    }
    
    void backward_complex(const std::complex<float>* in, 
                         std::complex<float>* out) override {
        std::copy(in, in + size_, complex_buffer_.begin());
        fft_.ifft(complex_buffer_, complex_buffer_);
        std::copy(complex_buffer_.begin(), complex_buffer_.end(), out);
    }
    
    std::string name() const override { return "Signalsmith"; }
    
private:
    signalsmith::fft::FFT<float> fft_;
    std::vector<float> real_buffer_;
    std::vector<std::complex<float>> complex_buffer_;
};
#endif

// https://github.com/HiFi-LoFi/AudioFFT
#ifdef HAVE_HIFILOFI
#include "hifilofi.hpp"

class HiFiLoFiBackend : public FFTBackend {
public:
    void setup(int size) override {
        size_ = size;
        real_buffer_.resize(size);
        complex_buffer_.resize(size);
    }
    
    void cleanup() override {
        real_buffer_.clear();
        complex_buffer_.clear();
    }
    
    void forward(const float* in, std::complex<float>* out) override {
        std::copy(in, in + size_, real_buffer_.begin());
        // HiFiLoFi API - adjust based on actual library
        hifilofi::fft(real_buffer_.data(), out, size_);
    }
    
    void backward(const std::complex<float>* in, float* out) override {
        // HiFiLoFi API - adjust based on actual library
        hifilofi::ifft(in, real_buffer_.data(), size_);
        std::copy(real_buffer_.begin(), real_buffer_.end(), out);
    }
    
    void forward_complex(const std::complex<float>* in, 
                        std::complex<float>* out) override {
        hifilofi::fft_complex(in, out, size_);
    }
    
    void backward_complex(const std::complex<float>* in, 
                         std::complex<float>* out) override {
        hifilofi::ifft_complex(in, out, size_);
    }
    
    std::string name() const override { return "HiFiLoFi"; }
    
private:
    std::vector<float> real_buffer_;
    std::vector<std::complex<float>> complex_buffer_;
};
#endif

// JUCE Framework FFT
// https://juce.com/
#ifdef HAVE_JUCE
#include <juce_dsp/juce_dsp.h>

class JUCEBackend : public FFTBackend {
public:
    void setup(int size) override {
        size_ = size;
        // JUCE FFT requires power-of-2 sizes
        int order = static_cast<int>(std::log2(size));
        fft_ = std::make_unique<juce::dsp::FFT>(order);
        
        real_buffer_.resize(size * 2); // JUCE needs double size for in-place
        complex_buffer_.resize(size);
    }
    
    void cleanup() override {
        fft_.reset();
        real_buffer_.clear();
        complex_buffer_.clear();
    }
    
    void forward(const float* in, std::complex<float>* out) override {
        std::copy(in, in + size_, real_buffer_.begin());
        std::fill(real_buffer_.begin() + size_, real_buffer_.end(), 0.0f);
        
        fft_->performRealOnlyForwardTransform(real_buffer_.data(), true);
        
        // JUCE stores results in packed format
        for (int i = 0; i < size_/2 + 1; ++i) {
            out[i] = std::complex<float>(real_buffer_[i*2], real_buffer_[i*2+1]);
        }
    }
    
    void backward(const std::complex<float>* in, float* out) override {
        // Pack complex data into JUCE format
        for (int i = 0; i < size_/2 + 1; ++i) {
            real_buffer_[i*2] = in[i].real();
            real_buffer_[i*2+1] = in[i].imag();
        }
        
        fft_->performRealOnlyInverseTransform(real_buffer_.data());
        
        std::copy(real_buffer_.begin(), real_buffer_.begin() + size_, out);
    }
    
    void forward_complex(const std::complex<float>* in, 
                        std::complex<float>* out) override {
        // JUCE's performFrequencyOnlyForwardTransform for complex
        for (int i = 0; i < size_; ++i) {
            real_buffer_[i*2] = in[i].real();
            real_buffer_[i*2+1] = in[i].imag();
        }
        
        fft_->perform(real_buffer_.data(), real_buffer_.data(), false);
        
        for (int i = 0; i < size_; ++i) {
            out[i] = std::complex<float>(real_buffer_[i*2], real_buffer_[i*2+1]);
        }
    }
    
    void backward_complex(const std::complex<float>* in, 
                         std::complex<float>* out) override {
        for (int i = 0; i < size_; ++i) {
            real_buffer_[i*2] = in[i].real();
            real_buffer_[i*2+1] = in[i].imag();
        }
        
        fft_->perform(real_buffer_.data(), real_buffer_.data(), true);
        
        for (int i = 0; i < size_; ++i) {
            out[i] = std::complex<float>(real_buffer_[i*2], real_buffer_[i*2+1]);
        }
    }
    
    std::string name() const override { return "JUCE"; }
    
    bool supports_size(int size) const override {
        // JUCE requires power-of-2 sizes
        return (size & (size - 1)) == 0;
    }
    
private:
    std::unique_ptr<juce::dsp::FFT> fft_;
    std::vector<float> real_buffer_;
    std::vector<std::complex<float>> complex_buffer_;
};
#endif