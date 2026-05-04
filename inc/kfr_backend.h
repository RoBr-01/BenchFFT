#pragma once

#include <kfr/dft.hpp>
#include <kfr/math.hpp>
#include <vector>

#include "fft_backend.h"

class KFRBackend : public FFTBackend {
   public:
    void setup(int size) override {
        size_ = size;

        // Create DFT plans
        plan_real_ = std::make_unique<kfr::dft_plan_real<float>>(size);
        plan_complex_ = std::make_unique<kfr::dft_plan<float>>(size);

        // Allocate buffers
        real_buffer_.resize(size);
        complex_buffer_.resize(size);
        temp_complex_.resize(size);
        temp_buffer_.resize(plan_real_->temp_size);
        temp_buffer_complex_.resize(plan_complex_->temp_size);
    }

    void cleanup() override {
        plan_real_.reset();
        plan_complex_.reset();
        real_buffer_.clear();
        complex_buffer_.clear();
        temp_complex_.clear();
    }

    void forward(const float* in, std::complex<float>* out) override {
        // Copy input to KFR buffer
        std::copy(in, in + size_, real_buffer_.begin());

        // Execute real-to-complex FFT
        kfr::univector<kfr::complex<float>> kfr_out(size_ / 2 + 1);
        plan_real_->execute(kfr_out,
                            kfr::make_univector(real_buffer_.data(), size_),
                            temp_buffer_.data());

        // Copy to output
        std::copy(kfr_out.begin(), kfr_out.end(), out);
    }

    void backward(const std::complex<float>* in, float* out) override {
        // Copy input to KFR buffer
        kfr::univector<kfr::complex<float>> kfr_in(size_ / 2 + 1);
        for (int i = 0; i < size_ / 2 + 1; ++i) {
            kfr_in[i] = kfr::complex<float>(in[i].real(), in[i].imag());
        }

        // Execute complex-to-real inverse FFT
        kfr::univector<float> kfr_out(size_);
        plan_real_->execute(kfr_out, kfr_in, temp_buffer_.data());

        // KFR normalizes automatically
        std::copy(kfr_out.begin(), kfr_out.end(), out);

        // Normalize
        const float scale = 1.0f / static_cast<float>(size_);
        for (int i = 0; i < size_; ++i) {
            out[i] *= scale;
        }
    }

    void forward_complex(const std::complex<float>* in,
                         std::complex<float>* out) override {
        // Copy to KFR format
        kfr::univector<kfr::complex<float>> kfr_in(size_);
        for (int i = 0; i < size_; ++i) {
            kfr_in[i] = kfr::complex<float>(in[i].real(), in[i].imag());
        }

        // Execute complex-to-complex FFT
        kfr::univector<kfr::complex<float>> kfr_out(size_);
        plan_complex_->execute(
            kfr_out, kfr_in, temp_buffer_complex_.data(), kfr::cfalse);

        // Copy output
        for (int i = 0; i < size_; ++i) {
            out[i] = std::complex<float>(kfr_out[i].real(), kfr_out[i].imag());
        }
    }

    void backward_complex(const std::complex<float>* in,
                          std::complex<float>* out) override {
        // Copy to KFR format
        kfr::univector<kfr::complex<float>> kfr_in(size_);
        for (int i = 0; i < size_; ++i) {
            kfr_in[i] = kfr::complex<float>(in[i].real(), in[i].imag());
        }

        // Execute inverse complex-to-complex FFT
        kfr::univector<kfr::complex<float>> kfr_out(size_);
        plan_complex_->execute(
            kfr_out, kfr_in, temp_buffer_complex_.data(), kfr::ctrue);

        // Copy output (KFR normalizes automatically)
        for (int i = 0; i < size_; ++i) {
            out[i] = std::complex<float>(kfr_out[i].real(), kfr_out[i].imag());
        }

        // Normalize
        const float scale = 1.0f / static_cast<float>(size_);
        for (int i = 0; i < size_; ++i) {
            out[i] *= scale;
        }
    }

    std::string name() const override {
        return "KFR";
    }

   private:
    std::unique_ptr<kfr::dft_plan_real<float>> plan_real_;
    std::unique_ptr<kfr::dft_plan<float>> plan_complex_;

    std::vector<float> real_buffer_;
    std::vector<std::complex<float>> complex_buffer_;
    std::vector<kfr::complex<float>> temp_complex_;
    std::vector<uint8_t> temp_buffer_;
    std::vector<uint8_t> temp_buffer_complex_;
};