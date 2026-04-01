#include "corelib.h"

struct SensorData data[25];
char output[80];

int main(void) {
	read_sensor(data);
	// Log the sensor data into a map of characters
	// . for not a unit, @ for unit
	char *ptr = output;
	strcpy(ptr, "Sensor data:\n");
	ptr += strlen(ptr);
	for (int i = 0; i < 25; i++) {
		if (!data[i].visible) {
			*ptr++ = '?';
		} else if (data[i].has_unit) {
			*ptr++ = '@';
		} else {
			*ptr++ = '.';
		}
		*ptr++ = " \n"[i % 5 == 4];
	}
	log(output, ptr - output);
	log_str("Test complete");
	while (true) {}
}
