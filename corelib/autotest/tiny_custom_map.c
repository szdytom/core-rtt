#include "corelib.h"

int main() {
	struct GameInfo info;
	assert(meta(&info) == EC_OK);
	assert(info.map_width == 6);
	assert(info.map_height == 4);
	assert(info.base_size == 2);
	while (true) {}
}
