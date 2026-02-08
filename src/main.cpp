#include "corertt/tilemap.h"
#include <iostream>

int main() {
	cr::TilemapGenerationConfig config;
	config.width = 32;
	config.height = 32;
	config.seed = cr::Seed::from_string(
		"test"
	); // Fixed seed for reproducibility

	cr::Tilemap tilemap = cr::Tilemap::generate(config);
	tilemap.saveAsText(std::cout);

	return 0;
}
