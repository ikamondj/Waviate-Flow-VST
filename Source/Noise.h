/*
  ==============================================================================

    Noise.h
    Created: 6 Sep 2025 11:44:59pm
    Author:  ikamo

  ==============================================================================
*/

#pragma once

double uniform_closed(double a, double b);

double deterministic_uniform_closed(double a, double b, double key);

double coin_flip();

double valueNoise(double x);

double voronoiNoise(double x);

double perlinNoise(double x, int octaveCount, double lacunarity, double roughness);

double ridgedMultiNoise(double x, int octaveCount, double lacunarity);
