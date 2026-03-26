#include "corelib.h"

int main(void) {
	log_str("first message\n");
	log_str("second message\n");
	log("third message", 13);
	log_str("[PASS] log + log_str simple test\n");

	while (true) {}
	return 0;
}
