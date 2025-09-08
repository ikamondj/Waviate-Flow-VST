/*
  ==============================================================================

    Noise.cpp
    Created: 6 Sep 2025 11:44:59pm
    Author:  ikamo

  ==============================================================================
*/

#include "Noise.h"
#include <random>
#include <limits>
#include <cassert>
#include <cmath>
#include <bit>
#include <cstdint>
#include <noise/noise.h>
noise::module::Voronoi voronoi;
noise::module::Perlin perlin;
noise::module::RidgedMulti ridgedMulti;
inline std::mt19937_64& rng() {
    thread_local std::mt19937_64 eng([] {
        std::random_device rd;
        std::seed_seq s{ rd(), rd(), rd(), rd(), rd(), rd(), rd(), rd() };
        return std::mt19937_64{ s };
        }());
    return eng;
}

double uniform_closed(double a, double b) {
    assert(a <= b);
    if (a == b) return a;
    const double hi = std::nextafter(b, std::numeric_limits<double>::infinity());
    std::uniform_real_distribution<double> dist(a, hi); // includes b
    return dist(rng());
}

// --- 64-bit mix (splitmix64) ---
inline uint64_t splitmix64(uint64_t x) {
    x += 0x9E3779B97F4A7C15ull;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
    return x ^ (x >> 31);
}

// Canonicalize double -> 64b (fold -0.0, collapse NaNs)
inline uint64_t canonical_double_bits(double d) {
    if (std::isnan(d)) return 0x7ff8000000000000ull; // canonical qNaN
    if (d == 0.0) d = 0.0;                           // fold -0.0 to +0.0
    return std::bit_cast<uint64_t>(d);
}

// Uniform U in [0,1) using top 53 bits
inline double uniform01(uint64_t key) {
    uint64_t mant53 = splitmix64(key) >> 11; // 64-53
    return mant53 * (1.0 / (1ull << 53));
}

// Deterministic uniform CLOSED [a,b], no asserts, stable fallbacks
double deterministic_uniform_closed(double a, double b, double key) {
    // Sanitize bounds (stable defaults)
    if (!std::isfinite(a)) a = 0.0;
    if (!std::isfinite(b)) b = 1.0;
    if (a > b) std::swap(a, b);
    if (a == b) return a;

    // Map key -> U[0,1)
    const uint64_t k = canonical_double_bits(key);
    const double u = uniform01(k);              // [0,1)
    const double x = a + (b - a) * u;           // [a,b)

    // Make it closed without throwing off uniformity (only measure-zero change)
    return (x < b) ? x : b;
}

double coin_flip() {
    static thread_local std::mt19937 gen(std::random_device{}());
    static thread_local std::bernoulli_distribution dist(0.5);
    return dist(gen) ? 1.0 : 0.0;
}

double valueNoise(double x) {
    return noise::ValueCoherentNoise3D(x, 0, 0, 0, noise::NoiseQuality::QUALITY_STD);
}

double voronoiNoise(double x) {
    return voronoi.GetValue(x, 0, 0);
}

double perlinNoise(double x, int octaveCount, double lacunarity, double roughness) {
    perlin.SetOctaveCount(octaveCount);
    perlin.SetLacunarity(lacunarity);
    perlin.SetPersistence(roughness);
    return perlin.GetValue(x,0,0);
}

double ridgedMultiNoise(double x, int octaveCount, double lacunarity) {
    ridgedMulti.SetOctaveCount(octaveCount);
    ridgedMulti.SetLacunarity(lacunarity);
    return ridgedMulti.GetValue(x, 0, 0);
}
