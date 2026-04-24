#ifndef FFTS_BACKEND_H
#define FFTS_BACKEND_H

#pragma once

#include <complex>
#include <cstdlib>  // aligned_alloc / free
#include <stdexcept>
#include <string>

#include "fft_backend.h"
#include <ffts.h>

/**
 * @brief FFTS backend adapter (header-only binding layer).
 *
 * Notes:
 * - FFTS is complex-to-complex only.
 * - Real transforms are emulated via packing/unpacking.
 * - Uses a single aligned interleaved buffer.
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

    inline void setup(int size) override {
        if (size <= 0)
            throw std::invalid_argument("FFT size must be positive.");

        cleanup();

        size_ = size;
        num_bins_ = size_ / 2 + 1;

        // Allocate interleaved complex buffer (real + imag)
        buffer_ =
            static_cast<float*>(aligned_alloc(32, sizeof(float) * 2 * size_));

        if (!buffer_) {
            throw std::bad_alloc();
        }

        forward_plan_ = ffts_init_1d(size_, FFTS_FORWARD);
        backward_plan_ = ffts_init_1d(size_, FFTS_BACKWARD);

        if (!forward_plan_ || !backward_plan_) {
            cleanup();
            throw std::runtime_error("FFTS plan creation failed");
        }
    }

    inline void cleanup() override {
        if (forward_plan_) {
            ffts_free(forward_plan_);
            forward_plan_ = nullptr;
        }

        if (backward_plan_) {
            ffts_free(backward_plan_);
            backward_plan_ = nullptr;
        }

        if (buffer_) {
            free(buffer_);
            buffer_ = nullptr;
        }

        size_ = 0;
        num_bins_ = 0;
    }

    // ============================================================
    // Standard Transforms
    // ============================================================

    inline void forward(const float* in, std::complex<float>* out) override {
        if (!forward_plan_)
            throw std::runtime_error("FFTS not initialized");

        // Pack real → interleaved complex
        for (int i = 0; i < size_; ++i) {
            buffer_[2 * i] = in[i];
            buffer_[2 * i + 1] = 0.0f;
        }

        // Execute FFT
        ffts_execute(forward_plan_, buffer_, buffer_);

        // Extract first N/2+1 bins
        for (size_t i = 0; i < num_bins_; ++i) {
            out[i] = {buffer_[2 * i], buffer_[2 * i + 1]};
        }
    }

    inline void backward(const std::complex<float>* in, float* out) override {
        if (!backward_plan_)
            throw std::runtime_error("FFTS not initialized");

        // Pack complex input → interleaved
        for (int i = 0; i < size_; ++i) {
            if (i < static_cast<int>(num_bins_)) {
                buffer_[2 * i] = in[i].real();
                buffer_[2 * i + 1] = in[i].imag();
            } else {
                buffer_[2 * i] = 0.0f;
                buffer_[2 * i + 1] = 0.0f;
            }
        }

        // Execute inverse FFT
        ffts_execute(backward_plan_, buffer_, buffer_);

        // Extract real part + normalize
        const float scale = 1.0f / static_cast<float>(size_);
        for (int i = 0; i < size_; ++i) {
            out[i] = buffer_[2 * i] * scale;
        }
    }

    // ============================================================
    // Complex-to-Complex
    // ============================================================

    inline void forward_complex(const std::complex<float>* in,
                                std::complex<float>* out) override {
        if (!forward_plan_)
            throw std::runtime_error("FFTS not initialized");

        for (int i = 0; i < size_; ++i) {
            buffer_[2 * i] = in[i].real();
            buffer_[2 * i + 1] = in[i].imag();
        }

        ffts_execute(forward_plan_, buffer_, buffer_);

        for (int i = 0; i < size_; ++i) {
            out[i] = {buffer_[2 * i], buffer_[2 * i + 1]};
        }
    }

    inline void backward_complex(const std::complex<float>* in,
                                 std::complex<float>* out) override {
        if (!backward_plan_)
            throw std::runtime_error("FFTS not initialized");

        for (int i = 0; i < size_; ++i) {
            buffer_[2 * i] = in[i].real();
            buffer_[2 * i + 1] = in[i].imag();
        }

        ffts_execute(backward_plan_, buffer_, buffer_);

        const float scale = 1.0f / static_cast<float>(size_);
        for (int i = 0; i < size_; ++i) {
            out[i] = {buffer_[2 * i] * scale, buffer_[2 * i + 1] * scale};
        }
    }

    // ============================================================
    // Staging Interface (minimal compliance)
    // ============================================================

    inline float* staging_real() override {
        return nullptr;
    }

    inline std::complex<float>* staging_complex() override {
        return nullptr;
    }

    // ============================================================
    // In-place (true in-place using buffer)
    // ============================================================

    inline void forward_inplace(std::complex<float>* out) override {
        if (!forward_plan_)
            throw std::runtime_error("FFTS not initialized");

        ffts_execute(forward_plan_, buffer_, buffer_);

        for (size_t i = 0; i < num_bins_; ++i) {
            out[i] = {buffer_[2 * i], buffer_[2 * i + 1]};
        }
    }

    inline void backward_inplace(float* out) override {
        if (!backward_plan_)
            throw std::runtime_error("FFTS not initialized");

        ffts_execute(backward_plan_, buffer_, buffer_);

        const float scale = 1.0f / static_cast<float>(size_);
        for (int i = 0; i < size_; ++i) {
            out[i] = buffer_[2 * i] * scale;
        }
    }

    inline void forward_complex_inplace(std::complex<float>* out) override {
        if (!forward_plan_)
            throw std::runtime_error("FFTS not initialized");

        ffts_execute(forward_plan_, buffer_, buffer_);

        for (int i = 0; i < size_; ++i) {
            out[i] = {buffer_[2 * i], buffer_[2 * i + 1]};
        }
    }

    inline void backward_complex_inplace(std::complex<float>* out) override {
        if (!backward_plan_)
            throw std::runtime_error("FFTS not initialized");

        ffts_execute(backward_plan_, buffer_, buffer_);

        const float scale = 1.0f / static_cast<float>(size_);
        for (int i = 0; i < size_; ++i) {
            out[i] = {buffer_[2 * i] * scale, buffer_[2 * i + 1] * scale};
        }
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
    int num_bins_{};

    float* buffer_ = nullptr;  // interleaved [real, imag, real, imag...]
};

#endif  // FFTS_BACKEND_H