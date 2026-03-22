#pragma once

#include <juce_dsp/juce_dsp.h>

#include <cmath>
#include <vector>

#include "fft_backend.h"

class JUCEBackend : public FFTBackend {
   public:
    JUCEBackend() = default;

    ~JUCEBackend() override {
        cleanup();
    }

    void setup(int size) override {
        cleanup();
        size_ = size;

        // JUCE FFT requires power-of-2 sizes
        int order = static_cast<int>(std::log2(size));
        fft_ = std::make_unique<juce::dsp::FFT>(order);

        // JUCE uses interleaved format [r0,i0,r1,i1,...] and needs 2*size for
        // real FFT
        interleaved_buffer_.resize(size * 2);
        temp_buffer_.resize(size * 2);
    }

    void cleanup() override {
        fft_.reset();
        interleaved_buffer_.clear();
        temp_buffer_.clear();
    }

    void forward(const float* in, std::complex<float>* out) override {
        // Copy real input to first half
        std::copy(in, in + size_, interleaved_buffer_.begin());
        // Zero the second half (imaginary parts)
        std::fill(interleaved_buffer_.begin() + size_,
                  interleaved_buffer_.end(),
                  0.0f);

        // Perform real-only forward transform
        fft_->performRealOnlyForwardTransform(interleaved_buffer_.data(), true);

        // JUCE stores result as [r0, r1, i1, r2, i2, ..., rN/2]
        // Convert to std::complex format
        out[0] = std::complex<float>(interleaved_buffer_[0], 0.0f);  // DC

        for (int i = 1; i < size_ / 2; ++i) {
            out[i] = std::complex<float>(interleaved_buffer_[i * 2],     // real
                                         interleaved_buffer_[i * 2 + 1]  // imag
            );
        }

        out[size_ / 2] =
            std::complex<float>(interleaved_buffer_[1], 0.0f);  // Nyquist
    }

    void backward(const std::complex<float>* in, float* out) override {
        // Convert from std::complex to JUCE's packed format
        // JUCE expects: [r0, rN/2, i1, r1, i2, r2, ...]
        interleaved_buffer_[0] = in[0].real();          // DC
        interleaved_buffer_[1] = in[size_ / 2].real();  // Nyquist

        for (int i = 1; i < size_ / 2; ++i) {
            interleaved_buffer_[i * 2] = in[i].real();
            interleaved_buffer_[i * 2 + 1] = in[i].imag();
        }

        // Perform inverse transform
        // Note: performRealOnlyInverseTransform already normalizes by 1/size
        fft_->performRealOnlyInverseTransform(interleaved_buffer_.data());

        std::copy(interleaved_buffer_.begin(),
                  interleaved_buffer_.begin() + size_,
                  out);
    }

    void forward_complex(const std::complex<float>* in,
                         std::complex<float>* out) override {
        // Convert std::complex to interleaved format
        for (int i = 0; i < size_; ++i) {
            interleaved_buffer_[i * 2] = in[i].real();
            interleaved_buffer_[i * 2 + 1] = in[i].imag();
        }

        // Cast to JUCE's Complex type
        auto* input_complex = reinterpret_cast<juce::dsp::Complex<float>*>(
            interleaved_buffer_.data());
        auto* output_complex =
            reinterpret_cast<juce::dsp::Complex<float>*>(temp_buffer_.data());

        // Perform complex FFT (forward = false)
        fft_->perform(input_complex, output_complex, false);

        // Convert back to std::complex
        for (int i = 0; i < size_; ++i) {
            out[i] = std::complex<float>(temp_buffer_[i * 2],
                                         temp_buffer_[i * 2 + 1]);
        }
    }

    void backward_complex(const std::complex<float>* in,
                          std::complex<float>* out) override {
        // Convert std::complex to interleaved format
        for (int i = 0; i < size_; ++i) {
            interleaved_buffer_[i * 2] = in[i].real();
            interleaved_buffer_[i * 2 + 1] = in[i].imag();
        }

        // Cast to JUCE's Complex type
        auto* input_complex = reinterpret_cast<juce::dsp::Complex<float>*>(
            interleaved_buffer_.data());
        auto* output_complex =
            reinterpret_cast<juce::dsp::Complex<float>*>(temp_buffer_.data());

        // Perform complex IFFT (inverse = true)
        fft_->perform(input_complex, output_complex, true);

        // Note: fft_->perform() does NOT normalize for C2C - divide manually
        for (int i = 0; i < size_; ++i) {
            out[i] = std::complex<float>(temp_buffer_[i * 2] / size_,
                                         temp_buffer_[i * 2 + 1] / size_);
        }
    }

    std::string name() const override {
        return "JUCE";
    }

    bool supports_size(int size) const override {
        // JUCE requires power-of-2 sizes
        return size > 0 && (size & (size - 1)) == 0;
    }

    bool supports_complex() const override {
        return true;
    }

   private:
    std::unique_ptr<juce::dsp::FFT> fft_;
    std::vector<float> interleaved_buffer_;  // [r0,i0,r1,i1,...]
    std::vector<float> temp_buffer_;         // For complex FFT output
};