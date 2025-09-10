/*
  ==============================================================================

    Noise.h
    Created: 6 Sep 2025 11:44:59pm
    Author:  ikamo

  ==============================================================================
*/

#pragma once
#include <stdint.h>
double uniform_closed(double a, double b);

double deterministic_uniform_closed(double a, double b, double key);

int64_t coin_flip();

double valueNoise(double x);

double voronoiNoise(double x);

double perlinNoise(double x, int64_t octaveCount, double lacunarity, double roughness);

double ridgedMultiNoise(double x, int64_t octaveCount, double lacunarity);
