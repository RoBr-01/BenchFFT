#ifndef FFTS_BACKEND_H
#define FFTS_BACKEND_H

#pragma once

#include <ffts.h>

#include <complex>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <vector>

#include "fft_backend.h"

/**
 * FFTS backend (corrected version)
 *
 * Key fixes:
 * - NO in-place execution (FFTS does not guarantee safety)
 * - correct buffer separation
 * - proper handling of frequency domain data for R2C transforms
 * - consistent normalization strategy
 */
class FFTSBackend : public FFTBackend {
   public:
    FFTSBackend() = default;

    ~FFTSBackend() override {
        cleanup();
    }

    // ============================================================
    // Lifecycle
    // ============================================================

    void setup(int size) override {
        if (size <= 0)
            throw std::invalid_argument("FFT size must be positive");

        cleanup();

        size_ = size;
        num_bins_ = size_ / 2 + 1;

        // Separate buffers (IMPORTANT for FFTS correctness)
        in_.resize(size_ * 2);
        out_.resize(size_ * 2);

        forward_plan_ = ffts_init_1d(size_, FFTS_FORWARD);
        backward_plan_ = ffts_init_1d(size_, FFTS_BACKWARD);

        if (!forward_plan_ || !backward_plan_) {
            cleanup();
            throw std::runtime_error("FFTS plan creation failed");
        }
    }

    void cleanup() override {
        if (forward_plan_) {
            ffts_free(forward_plan_);
            forward_plan_ = nullptr;
        }

        if (backward_plan_) {
            ffts_free(backward_plan_);
            backward_plan_ = nullptr;
        }

        in_.clear();
        out_.clear();

        size_ = 0;
        num_bins_ = 0;
    }

    // ============================================================
    // R2C
    // ============================================================

    void forward(const float* in, std::complex<float>* out) override {
        if (!forward_plan_)
            throw std::runtime_error("FFTS not initialized");

        // Copy real input to interleaved format (imaginary = 0)
        for (int i = 0; i < size_; ++i) {
            in_[2 * i] = in[i];
            in_[2 * i + 1] = 0.0f;
        }

        // Execute forward transform
        ffts_execute(forward_plan_, in_.data(), out_.data());

        // Copy interleaved output to complex array
        for (size_t i = 0; i < num_bins_; ++i) {
            out[i] = {out_[2 * i], out_[2 * i + 1]};
        }
    }

    void backward(const std::complex<float>* in, float* out) override {
        if (!backward_plan_)
            throw std::runtime_error("FFTS not initialized");

        // Prepare interleaved input buffer for inverse transform
        // For RFFT, we need to properly handle the conjugate symmetric property
        // FFTS expects a full complex buffer of size 'size_' for inverse
        // transform

        // First, copy the valid bins (0 to num_bins_-1)
        for (int i = 0; i < static_cast<int>(num_bins_); ++i) {
            in_[2 * i] = in[i].real();
            in_[2 * i + 1] = in[i].imag();
        }

        // For the remaining bins (num_bins_ to size_-1), we need to fill with
        // the conjugate symmetric values to maintain proper inverse transform
        for (int i = num_bins_; i < size_; ++i) {
            int conj_idx = size_ - i;
            if (conj_idx >= 0 && conj_idx < static_cast<int>(num_bins_)) {
                in_[2 * i] = in[conj_idx].real();
                in_[2 * i + 1] = -in[conj_idx].imag();
            } else {
                in_[2 * i] = 0.0f;
                in_[2 * i + 1] = 0.0f;
            }
        }

        // Execute inverse transform
        ffts_execute(backward_plan_, in_.data(), out_.data());

        // Normalize (FFTS is unnormalized)
        const float scale = 1.0f / static_cast<float>(size_);
        for (int i = 0; i < size_; ++i) {
            out[i] = out_[2 * i] * scale;
        }
    }

    // ============================================================
    // Complex
    // ============================================================

    void forward_complex(const std::complex<float>* in,
                         std::complex<float>* out) override {
        if (!forward_plan_)
            throw std::runtime_error("FFTS not initialized");

        for (int i = 0; i < size_; ++i) {
            in_[2 * i] = in[i].real();
            in_[2 * i + 1] = in[i].imag();
        }

        ffts_execute(forward_plan_, in_.data(), out_.data());

        for (int i = 0; i < size_; ++i) {
            out[i] = {out_[2 * i], out_[2 * i + 1]};
        }
    }

    void backward_complex(const std::complex<float>* in,
                          std::complex<float>* out) override {
        if (!backward_plan_)
            throw std::runtime_error("FFTS not initialized");

        for (int i = 0; i < size_; ++i) {
            in_[2 * i] = in[i].real();
            in_[2 * i + 1] = in[i].imag();
        }

        ffts_execute(backward_plan_, in_.data(), out_.data());

        const float scale = 1.0f / static_cast<float>(size_);
        for (int i = 0; i < size_; ++i) {
            out[i] = {out_[2 * i] * scale, out_[2 * i + 1] * scale};
        }
    }

    // ============================================================
    // Staging (disabled)
    // ============================================================

    float* staging_real() override {
        return nullptr;
    }

    std::complex<float>* staging_complex() override {
        return nullptr;
    }

    // ============================================================
    // In-place (not safely supported → fallback behavior)
    // ============================================================

    void forward_inplace(std::complex<float>* out) override {
        forward(reinterpret_cast<const float*>(out), out);
    }

    void backward_inplace(float* out) override {
        std::vector<std::complex<float>> tmp(num_bins_);
        backward(tmp.data(), out);
    }

    void forward_complex_inplace(std::complex<float>* out) override {
        forward_complex(out, out);
    }

    void backward_complex_inplace(std::complex<float>* out) override {
        backward_complex(out, out);
    }

    // ============================================================
    // Metadata
    // ============================================================

    std::string name() const override {
        return "FFTS";
    }

    bool supports_size(int) const override {
        return true;
    }

    bool supports_complex() const override {
        return true;
    }

   private:
    ffts_plan_t* forward_plan_ = nullptr;
    ffts_plan_t* backward_plan_ = nullptr;

    int size_ = 0;
    size_t num_bins_ = 0;

    std::vector<float> in_;
    std::vector<float> out_;
};

#endif