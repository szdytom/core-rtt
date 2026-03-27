#ifndef CORERTT_NOISE_H
#define CORERTT_NOISE_H

#include "corertt/xoroshiro.h"
#include <vector>

namespace cr {

class PerlinNoise {
public:
	/**
	 * @brief Construct a PerlinNoise generator with the given seed
	 * @param rng Random number generator for noise
	 */
	explicit PerlinNoise(Xoroshiro128PP rng);

	PerlinNoise() = default;

	/**
	 * @brief Generate 2D Perlin noise value at the given coordinates
	 * @param x X coordinate
	 * @param y Y coordinate
	 * @return Noise value between 0.0 and 1.0
	 */
	double noise(double x, double y) const noexcept;

	/**
	 * @brief Generate octave noise (multiple frequencies combined)
	 * @param x X coordinate
	 * @param y Y coordinate
	 * @param octaves Number of octaves to combine
	 * @param persistence How much each octave contributes
	 * @return Noise value between 0.0 and 1.0
	 */
	double octave_noise(double x, double y, int octaves, double persistence)
		const noexcept;

private:
	std::vector<int> _permutation;
};

/**
 * @brief A wrapper that provides uniform distribution mapping for Perlin noise
 *
 * This class samples the noise distribution and builds a CDF (Cumulative
 * Distribution Function) to map the non-uniform Perlin noise values to a
 * uniform [0,1] distribution using quantiles.
 */
class UniformPerlinNoise {
public:
	/**
	 * @brief Construct a UniformPerlinNoise generator
	 * @param seed Random seed for noise generation
	 */
	explicit UniformPerlinNoise(Xoroshiro128PP rng);

	UniformPerlinNoise() = default;

	/**
	 * @brief Calibrate the noise distribution by sampling
	 * @param scale The scale parameter that will be used for generation
	 * @param octaves Number of octaves for octave noise
	 * @param persistence Persistence for octave noise
	 * @param sample_size Number of samples to use for CDF (default: 10000)
	 */
	void calibrate(
		double scale, int octaves, double persistence, int sample_size = 10000
	);

	/**
	 * @brief Generate uniform noise value at the given coordinates
	 * @param x X coordinate
	 * @param y Y coordinate
	 * @return Uniformly distributed noise value between 0.0 and 1.0
	 * @note Must call calibrate() first
	 */
	double uniform_noise(double x, double y) const;

	/**
	 * @brief Check if the noise generator has been calibrated
	 */
	bool is_calibrated() const noexcept {
		return _is_calibrated;
	}

private:
	/**
	 * @brief Map a raw noise value to uniform distribution using the CDF
	 * @param raw_value Raw noise value from Perlin noise
	 * @return Uniformly distributed value between 0.0 and 1.0
	 */
	double _map_to_uniform(double raw_value) const noexcept;

	PerlinNoise _noise;
	Xoroshiro128PP _calibrate_rng;
	std::vector<double> _cdf_values; // Sorted noise values for CDF
	bool _is_calibrated;

	// Parameters used for calibration
	double _scale;
	int _octaves;
	double _persistence;
};

} // namespace cr

#endif
