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

#endif // PERLIN_NOISE_HPP