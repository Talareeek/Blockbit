#ifndef PERLIN_NOISE_HPP
#define PERLIN_NOISE_HPP

#include <vector>
#include <numeric>
#include <random>
#include <cmath>
#include <algorithm>

class PerlinNoise
{
private:

    std::vector<int> p;

    static double fade(double t)
    {
        return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
    }

    static double lerp(double a, double b, double t)
    {
        return a + t * (b - a);
    }

    static double grad(int hash, double x, double y)
    {
        switch (hash & 3)
        {
            case 0: return  x + y;
            case 1: return -x + y;
            case 2: return  x - y;
            case 3: return -x - y;
            default: return 0.0;
        }
    }

public:

    PerlinNoise(unsigned int seed)
    {
        p.resize(256);
        std::iota(p.begin(), p.end(), 0);

        std::mt19937 engine(seed);
        std::shuffle(p.begin(), p.end(), engine);

        p.insert(p.end(), p.begin(), p.end());
    }

    double noise(double x, double y) const
    {
        const int xi = static_cast<int>(std::floor(x)) & 255;
        const int yi = static_cast<int>(std::floor(y)) & 255;

        const double xf = x - std::floor(x);
        const double yf = y - std::floor(y);

        const double u = fade(xf);
        const double v = fade(yf);

        const int aa = p[p[xi] + yi];
        const int ab = p[p[xi] + yi + 1];
        const int ba = p[p[xi + 1] + yi];
        const int bb = p[p[xi + 1] + yi + 1];

        const double x1 = lerp(
            grad(aa, xf, yf),
            grad(ba, xf - 1.0, yf),
            u
        );

        const double x2 = lerp(
            grad(ab, xf, yf - 1.0),
            grad(bb, xf - 1.0, yf - 1.0),
            u
        );

        return (lerp(x1, x2, v) + 1.0) / 2.0;
    }
};

#include <vector>
#include <numeric>
#include <random>
#include <cmath>
#include <algorithm>

class PerlinNoise3D
{
private:
    std::vector<int> p;

    static double fade(double t)
    {
        return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
    }

    static double lerp(double a, double b, double t)
    {
        return a + t * (b - a);
    }

    static double grad(int hash, double x, double y, double z)
    {
        const int h = hash & 15;

        const double u = h < 8 ? x : y;
        const double v = h < 4 ? y :
                         (h == 12 || h == 14 ? x : z);

        return ((h & 1) ? -u : u) +
               ((h & 2) ? -v : v);
    }

public:
    explicit PerlinNoise3D(unsigned int seed)
    {
        p.resize(256);
        std::iota(p.begin(), p.end(), 0);

        std::mt19937 engine(seed);
        std::shuffle(p.begin(), p.end(), engine);

        const auto original = p;
        p.insert(p.end(), original.begin(), original.end());
    }

    double noise(double x, double y, double z) const
    {
        const int xi = static_cast<int>(std::floor(x)) & 255;
        const int yi = static_cast<int>(std::floor(y)) & 255;
        const int zi = static_cast<int>(std::floor(z)) & 255;

        const double xf = x - std::floor(x);
        const double yf = y - std::floor(y);
        const double zf = z - std::floor(z);

        const double u = fade(xf);
        const double v = fade(yf);
        const double w = fade(zf);

        const int aaa = p[p[p[xi]     + yi]     + zi];
        const int aba = p[p[p[xi]     + yi + 1] + zi];
        const int aab = p[p[p[xi]     + yi]     + zi + 1];
        const int abb = p[p[p[xi]     + yi + 1] + zi + 1];

        const int baa = p[p[p[xi + 1] + yi]     + zi];
        const int bba = p[p[p[xi + 1] + yi + 1] + zi];
        const int bab = p[p[p[xi + 1] + yi]     + zi + 1];
        const int bbb = p[p[p[xi + 1] + yi + 1] + zi + 1];

        const double x00 = lerp(
            grad(aaa, xf,     yf,     zf),
            grad(baa, xf - 1, yf,     zf),
            u
        );

        const double x10 = lerp(
            grad(aba, xf,     yf - 1, zf),
            grad(bba, xf - 1, yf - 1, zf),
            u
        );

        const double x01 = lerp(
            grad(aab, xf,     yf,     zf - 1),
            grad(bab, xf - 1, yf,     zf - 1),
            u
        );

        const double x11 = lerp(
            grad(abb, xf,     yf - 1, zf - 1),
            grad(bbb, xf - 1, yf - 1, zf - 1),
            u
        );

        const double y0 = lerp(x00, x10, v);
        const double y1 = lerp(x01, x11, v);

        const double result = lerp(y0, y1, w);

        // Przybliżona normalizacja do [0, 1].
        return std::clamp((result + 1.0) / 2.0, 0.0, 1.0);
    }
};

#endif // PERLIN_NOISE_HPP