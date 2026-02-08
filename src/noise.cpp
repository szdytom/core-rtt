#include "corertt/noise.h"
#include <algorithm>
#include <cmath>
#include <cpptrace/cpptrace.hpp>
#include <format>
#include <iostream>
#include <random>

namespace cr {

namespace {

double fade(double t) noexcept {
	// Fade function: 6t^5 - 15t^4 + 10t^3
	return t * t * t * (t * (t * 6 - 15) + 10);
}

double lerp(double t, double a, double b) noexcept {
	return a + t * (b - a);
}

double grad(int hash, double x, double y) noexcept {
	// Convert low 4 bits of hash code into 12 gradient directions
	int h = hash & 15;
	double u = h < 8 ? x : y;
	double v = h < 4 ? y : h == 12 || h == 14 ? x : 0;
	return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
}

} // namespace

PerlinNoise::PerlinNoise(Xoroshiro128PP rng) {
	// Initialize permutation array with values 0-255
	_permutation.resize(256);
	std::iota(_permutation.begin(), _permutation.end(), 0);

	// Shuffle using the provided seed
	std::shuffle(_permutation.begin(), _permutation.end(), rng);

	// Duplicate the permutation to avoid overflow
	_permutation.insert(
		_permutation.end(), _permutation.begin(), _permutation.end()
	);
}

double PerlinNoise::noise(double x, double y) const noexcept {
	// Find unit grid cell containing point
	int X = static_cast<int>(std::floor(x)) & 255;
	int Y = static_cast<int>(std::floor(y)) & 255;

	// Get relative xy coordinates of point within that cell
	x -= std::floor(x);
	y -= std::floor(y);

	// Compute fade curves for each coordinate
	double u = fade(x);
	double v = fade(y);

	// Hash coordinates of the 4 cube corners
	int A = _permutation[X] + Y;
	int AA = _permutation[A];
	int AB = _permutation[A + 1];
	int B = _permutation[X + 1] + Y;
	int BA = _permutation[B];
	int BB = _permutation[B + 1];

	// Add blended results from 4 corners of the square
	double result = lerp(
		v,
		lerp(u, grad(_permutation[AA], x, y), grad(_permutation[BA], x - 1, y)),
		lerp(
			u, grad(_permutation[AB], x, y - 1),
			grad(_permutation[BB], x - 1, y - 1)
		)
	);

	// Convert from [-1,1] to [0,1]
	return (result + 1.0) * 0.5;
}

double PerlinNoise::octave_noise(
	double x, double y, int octaves, double persistence
) const noexcept {
	double value = 0.0;
	double amplitude = 1.0;
	double frequency = 1.0;
	double max_value = 0.0;

	for (int i = 0; i < octaves; i++) {
		value += noise(x * frequency, y * frequency) * amplitude;
		max_value += amplitude;
		amplitude *= persistence;
		frequency *= 2.0;
	}

	return value / max_value;
}

UniformPerlinNoise::UniformPerlinNoise(Xoroshiro128PP rng)
	: _noise(rng), _calibrate_rng(rng), _is_calibrated(false) {}

void UniformPerlinNoise::calibrate(
	double scale, int octaves, double persistence, int sample_size
) {
	_scale = scale;
	_octaves = octaves;
	_persistence = persistence;

	// Clear previous calibration
	_cdf_values.clear();

	// Sample noise values across a reasonable range
	Xoroshiro128PP rng = _calibrate_rng;
	std::uniform_real_distribution<double> coord_dist(0.0, 1000.0);

	// Collect samples
	_cdf_values.reserve(sample_size);
	for (int i = 0; i < sample_size; ++i) {
		double x = coord_dist(rng);
		double y = coord_dist(rng);

		double noise_value;
		if (_octaves == 1) {
			noise_value = _noise.noise(x * _scale, y * _scale);
		} else {
			noise_value = _noise.octave_noise(
				x * _scale, y * _scale, _octaves, _persistence
			);
		}

		_cdf_values.push_back(noise_value);
	}

	// Sort values to create CDF
	std::sort(_cdf_values.begin(), _cdf_values.end());

	_is_calibrated = true;
}

double UniformPerlinNoise::uniform_noise(double x, double y) const {
#ifndef NDEBUG
	if (!_is_calibrated) {
		std::cerr << std::format(
			"UniformPerlinNoise::uniform_noise() called before calibration.\n"
		);
		cpptrace::generate_trace().print(std::cerr);
		std::abort();
	}
#endif

	// Generate raw noise value
	double raw_value;
	if (_octaves == 1) {
		raw_value = _noise.noise(x * _scale, y * _scale);
	} else {
		raw_value = _noise.octave_noise(
			x * _scale, y * _scale, _octaves, _persistence
		);
	}

	// Map to uniform distribution
	return _map_to_uniform(raw_value);
}

double UniformPerlinNoise::_map_to_uniform(double raw_value) const noexcept {
	// Find position in CDF using binary search
	auto it = std::lower_bound(
		_cdf_values.begin(), _cdf_values.end(), raw_value
	);

	// Calculate quantile (position in CDF as fraction)
	size_t position = std::distance(_cdf_values.begin(), it);
	double quantile = static_cast<double>(position) / _cdf_values.size();

	// Clamp to [0,1] range
	return std::max(0.0, std::min(1.0, quantile));
}

} // namespace cr
