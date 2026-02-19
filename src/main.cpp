#include "corertt/entity.h"
#include "corertt/tilemap.h"
#include "corertt/world.h"
#include <fstream>
#include <iostream>
#include <memory>

std::vector<std::uint8_t> load_binary(std::string_view path) {
	std::ifstream file(path.data(), std::ios::binary);
	if (!file) {
		throw std::runtime_error("Failed to open file");
	}
	return std::vector<std::uint8_t>(
		(std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>()
	);
}

int main() {
	cr::TilemapGenerationConfig config;
	config.width = 32;
	config.height = 32;
	config.seed = cr::Seed::from_string(
		"test"
	); // Fixed seed for reproducibility

	cr::Tilemap tilemap = cr::Tilemap::generate(config);

	auto world = std::make_unique<cr::World>(std::move(tilemap));
	world->setPlayerProgram(
		1, load_binary("player1_base.elf"), load_binary("player1_unit.elf")
	);
	world->setPlayerProgram(
		2, load_binary("player2_base.elf"), load_binary("player2_unit.elf")
	);

	for (int i = 0; i < 5; ++i) {
		std::cout << "Tick " << world->currentTick() + 1 << std::endl;
		world->step();
	}
	return 0;
}
