/*
 ____  _____ _        _
| __ )| ____| |      / \
|  _ \|  _| | |     / _ \
| |_) | |___| |___ / ___ \
|____/|_____|_____/_/   \_\
http://bela.io
*/

const char* helpText =
"Scan the I2C bus for Trill devices\n"
"  Usage: %s <bus>\n"
"    <bus>: the bus to scan. Default is 1 (i.e.: /dev/i2c-1)\n"
"======================\n"
"\n"
"This project is a handy utility which will identify all connected\n"
"Trill devices and print information on them to the console\n"
"\n"
"This is particularly useful if you are unsure of the address of the sensor after\n"
"changing it via the solder bridges on the back. This example will also give\n"
"you a total count of the amount of Trill sensors currently connected on the\n"
"specified bus.\n"
"\n"
"NOTE: as this program scans several addresses on the i2c bus\n"
"it could cause non-Trill peripherals connected to it to malfunction.\n";

#include <Trill.h>
#include <string>

int main(int argc, char** argv)
{
	unsigned int i2cBus = 1;
	if(argc >= 2)
	{
		if(std::string("--help") == std::string(argv[1])) {
			printf(helpText, argv[0]);
			return 0;
		}
		i2cBus = atoi(argv[1]);
	}
	printf("Trill devices detected on bus %d\n", i2cBus);
	printf("Address    | Type\n");
	unsigned int total = 0;
	for(uint8_t n = 0x20; n <= 0x50; ++n) {
		Trill::Device device = Trill::probe(i2cBus, n);
		if(device != Trill::NONE) {
			printf("%#4x (%3d) | %s\n", n, n, Trill::getNameFromDevice(device).c_str());
			++total;
		}
	}
	printf("\nTotal: %d devices\n", total);
	return 0;
}
