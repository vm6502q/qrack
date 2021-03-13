//////////////////////////////////////////////////////////////////////////////////////
//
// (C) Daniel Strano and the Qrack contributors 2017-2021. All rights reserved.
//
// This is a fill-in over the std::complex<__fp16> type, for Qrack.
//
// Licensed under the GNU Lesser General Public License V3.
// See LICENSE.md in the project root or https://www.gnu.org/licenses/lgpl-3.0.en.html
// for details.

#pragma once

#include <complex>

namespace Qrack {

struct Complex2 {
    __fp16 real;
    __fp16 imag;

    Complex2() {}

    Complex2(float r, float i)
        : real(r)
        , imag(i)
    {
    }

    Complex2(std::complex<float> o)
        : real(std::real(o))
        , imag(std::imag(o))
    {
    }

    Complex2(float r)
        : real(r)
        , imag(0)
    {
    }

    bool operator==(const Complex2& rhs) const { return (real == rhs.real) && (imag == rhs.imag); }

    bool operator!=(const Complex2& rhs) const { return (real != rhs.real) || (imag != rhs.imag); }

    Complex2 operator-() const { return Complex2(-real, -imag); }

    inline Complex2 operator+(const Complex2& rhs) const { return Complex2(real + rhs.real, imag + rhs.imag); }

    inline Complex2 operator-(const Complex2& rhs) const { return Complex2(real - rhs.real, imag - rhs.imag); }

    inline Complex2 operator*(const Complex2& rhs) const
    {
        return Complex2(real * rhs.real - imag * rhs.imag, real * rhs.imag + imag * rhs.real);
    }

    inline Complex2 operator*(const float& rhs) const { return Complex2(real * rhs, imag * rhs); }

    inline Complex2 operator*=(const float& rhs) { return Complex2(real *= rhs, imag *= rhs); }

    inline Complex2 operator*=(const Complex2& rhs)
    {
        Complex2 temp(real * rhs.real - imag * rhs.imag, real * rhs.imag + imag * rhs.real);
        real = temp.real;
        imag = temp.imag;
        return temp;
    }

    inline Complex2 operator/(const Complex2& rhs) const
    {
        return (Complex2(real, imag) * rhs) / (rhs.real * rhs.real + rhs.imag * rhs.imag);
    }

    inline Complex2 operator/(const float& rhs) const { return Complex2(real / rhs, imag / rhs); }

    inline Complex2 operator/=(const float& rhs) { return Complex2(real /= rhs, imag /= rhs); }

    inline Complex2 operator/=(const Complex2& rhs)
    {
        Complex2 temp = (Complex2(real, imag) * rhs) / (rhs.real * rhs.real + rhs.imag * rhs.imag);
        real = temp.real;
        imag = temp.imag;
        return temp;
    }
};

} // namespace Qrack

float real(const Qrack::Complex2& c);

float imag(const Qrack::Complex2& c);

float abs(const Qrack::Complex2& c);

float arg(const Qrack::Complex2& c);

float norm(const Qrack::Complex2& c);

Qrack::Complex2 sqrt(const Qrack::Complex2& c);

Qrack::Complex2 exp(const Qrack::Complex2& c);

Qrack::Complex2 pow(const Qrack::Complex2& b, const Qrack::Complex2& p);

Qrack::Complex2 conj(const Qrack::Complex2& c);

namespace Qrack {

inline Complex2 operator*(const float& lhs, const Complex2& rhs) { return Complex2(lhs * rhs.real, lhs * rhs.imag); }

inline Complex2 operator/(const float& lhs, const Complex2& rhs) { return (lhs * rhs) / norm(rhs); }

} // namespace Qrack
