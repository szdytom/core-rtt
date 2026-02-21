#include "corertt/entity.h"
#include "corertt/tilemap.h"
#include "corertt/world.h"
#include <argparse/argparse.hpp>
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

int main(int argc, char *argv[]) {
	argparse::ArgumentParser program("corertt");

	program.add_argument("--p1-base")
		.default_value(std::string("player1_base.elf"))
		.help("path to player 1 base ELF");
	program.add_argument("--p1-unit")
		.default_value(std::string("player1_unit.elf"))
		.help("path to player 1 unit ELF");
	program.add_argument("--p2-base")
		.default_value(std::string("player2_base.elf"))
		.help("path to player 2 base ELF");
	program.add_argument("--p2-unit")
		.default_value(std::string("player2_unit.elf"))
		.help("path to player 2 unit ELF");
	program.add_argument("-n", "--ticks")
		.default_value(5)
		.scan<'i', int>()
		.help("number of ticks to simulate");
	program.add_argument("-s", "--seed")
		.default_value(std::string("test"))
		.help("seed for tilemap generation");

	try {
		program.parse_args(argc, argv);
	} catch (const std::exception &e) {
		std::cerr << e.what() << std::endl;
		std::cerr << program;
		return 1;
	}

	cr::TilemapGenerationConfig config;
	config.width = 32;
	config.height = 32;
	config.seed = cr::Seed::from_string(
		program.get<std::string>("--seed")
	); // Fixed seed for reproducibility

	cr::Tilemap tilemap = cr::Tilemap::generate(config);
	tilemap.saveAsText(std::cout); // Print generated tilemap to console

	auto world = std::make_unique<cr::World>(std::move(tilemap));
	world->setPlayerProgram(
		1, load_binary(program.get<std::string>("--p1-base")),
		load_binary(program.get<std::string>("--p1-unit"))
	);
	world->setPlayerProgram(
		2, load_binary(program.get<std::string>("--p2-base")),
		load_binary(program.get<std::string>("--p2-unit"))
	);

	int ticks = program.get<int>("--ticks");
	for (int i = 0; i < ticks; ++i) {
		std::cout << "Tick " << world->currentTick() + 1 << std::endl;
		world->step();
	}
	return 0;
}
