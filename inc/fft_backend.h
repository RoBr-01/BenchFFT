#pragma once

#include <complex>
#include <string>
#include <vector>

// Abstract interface for FFT backends.
//
// STAGING BUFFER INTERFACE (optional optimisation)
// ─────────────────────────────────────────────────
// Some libraries (PFFFT, FFTW) require their own aligned internal buffers and
// unavoidably copy the caller's input into them before transforming.  To let
// the benchmark eliminate this copy, backends can override the staging-buffer
// accessors below.  The benchmark will then write test data directly into the
// backend's own memory and call forward_inplace() / backward_inplace() instead
// of the copy-in variants.
//
// Backends that operate directly on caller pointers (KissFFT, Signalsmith)
// simply leave the defaults in place — the benchmark falls back to the normal
// forward()/backward() path for them.

class FFTBackend {
   public:
    virtual ~FFTBackend() = default;

    // ── Lifecycle ────────────────────────────────────────────────────────────
    virtual void setup(int size) = 0;
    virtual void cleanup() = 0;

    // ── Standard transforms (copy-in / copy-out) ─────────────────────────────
    // Forward FFT: real time-domain → complex frequency-domain (size/2+1 bins)
    virtual void forward(const float* in, std::complex<float>* out) = 0;

    // Backward FFT: complex frequency-domain (size/2+1 bins) → real time-domain
    // Implementations must normalise by 1/N so that ifft(fft(x)) == x.
    virtual void backward(const std::complex<float>* in, float* out) = 0;

    // Complex-to-complex (not all backends support this efficiently)
    virtual void forward_complex(const std::complex<float>* in,
                                 std::complex<float>* out) = 0;
    virtual void backward_complex(const std::complex<float>* in,
                                  std::complex<float>* out) = 0;

    // ── Staging-buffer interface (optional, default = nullptr / no-op)
    // ──────── Returns a pointer to the backend's own aligned input staging
    // buffer, or nullptr if the backend has no such buffer (copy path will be
    // used).
    virtual float* staging_real() {
        return nullptr;
    }
    virtual std::complex<float>* staging_complex() {
        return nullptr;
    }

    // Transform using data already written into the staging buffer.
    // Only called by the benchmark when staging_real()/staging_complex() !=
    // nullptr.
    virtual void forward_inplace(std::complex<float>* out) {
        (void)out;
    }
    virtual void backward_inplace(float* out) {
        (void)out;
    }
    virtual void forward_complex_inplace(std::complex<float>* out) {
        (void)out;
    }
    virtual void backward_complex_inplace(std::complex<float>* out) {
        (void)out;
    }

    // ── Metadata ─────────────────────────────────────────────────────────────
    virtual std::string name() const = 0;
    virtual bool supports_size(int /*size*/) const {
        return true;
    }
    virtual bool supports_complex() const {
        return true;
    }

    int get_size() const {
        return size_;
    }

   protected:
    int size_ = 0;
};