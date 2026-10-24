#pragma once
#include <cmath>
#include <vector>

namespace au
{
// Small iterative radix-2 FFT (float). Deterministic across platforms for a given compiler.
class Fft
{
public:
    explicit Fft (int sizePow2) : n (sizePow2), cosT (sizePow2 / 2), sinT (sizePow2 / 2), rev (sizePow2)
    {
        for (int i = 0; i < n / 2; ++i)
        {
            const double a = -2.0 * 3.14159265358979323846 * i / n;
            cosT[(size_t) i] = (float) std::cos (a);
            sinT[(size_t) i] = (float) std::sin (a);
        }
        int bits = 0;
        while ((1 << bits) < n) ++bits;
        for (int i = 0; i < n; ++i)
        {
            int r = 0;
            for (int b = 0; b < bits; ++b)
                if (i & (1 << b)) r |= 1 << (bits - 1 - b);
            rev[(size_t) i] = r;
        }
    }

    int size() const { return n; }

    void forward (float* re, float* im) const
    {
        for (int i = 0; i < n; ++i)
        {
            const int j = rev[(size_t) i];
            if (j > i) { std::swap (re[i], re[j]); std::swap (im[i], im[j]); }
        }
        for (int len = 2; len <= n; len <<= 1)
        {
            const int half = len / 2, step = n / len;
            for (int i = 0; i < n; i += len)
                for (int k = 0; k < half; ++k)
                {
                    const float wr = cosT[(size_t) (k * step)], wi = sinT[(size_t) (k * step)];
                    const int a = i + k, b = a + half;
                    const float tr = re[b] * wr - im[b] * wi;
                    const float ti = re[b] * wi + im[b] * wr;
                    re[b] = re[a] - tr; im[b] = im[a] - ti;
                    re[a] += tr;        im[a] += ti;
                }
        }
    }

private:
    int n;
    std::vector<float> cosT, sinT;
    std::vector<int> rev;
};
}
