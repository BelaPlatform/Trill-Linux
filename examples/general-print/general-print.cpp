#include <Trill.h>
#include <iostream>

#include <signal.h>
const char* helpText =
"Connect to one Trill device and print its readings to the console\n"
"  Usage: %s <bus> <device-name> [<address>]\n"
"         <bus> is the bus that the device is connected to (i.e.: the X in /dev/i2c-X)\n"
"         <device-name> is the name of the device (e.g.: `bar`, `square`,\n"
"	                `craft`, `hex`, ring`, ...)\n"
"          <address> (optional) is the address of the device. If this is\n"
"                    not passed, the default address for the specified device\n"
"                    type will be used instead.\n"
;
int gShouldStop;

void interrupt_handler(int var)
{
	gShouldStop = true;
}

Trill touchSensor;

int main(int argc, char** argv)
{
	std::string deviceName;
	int i2cBus = -1;
	uint8_t address = 255;
	if(2 > argc) {
		printf(helpText, argv[0]);
		return 1;
	}
	for(unsigned int c = 1; c < argc; ++c)
	{
		if(std::string("--help") == std::string(argv[c])) {
			printf(helpText, argv[0]);
			return 0;
		}
		if(1 == c) {
			i2cBus = std::stoi(argv[c]);
		} else if(2 == c) {
			deviceName = argv[c];
		} else if(3 == c) {
			address = std::stoi(argv[c]);
		}
	}
	if(i2cBus < 0) {
		fprintf(stderr, "No or invalid bus specified\n");
		return 1;
	}
	Trill::Device device = Trill::getDeviceFromName(deviceName);
	if(Trill::UNKNOWN == device) {
		fprintf(stderr, "No or invalid device name specified: `%s`\n", deviceName.c_str());
		return 1;
	}
	std::cout << "Opening device " << Trill::getNameFromDevice(device) << " on bus " << i2cBus;
	if(255 != address)
		std::cout << "at address: " << address;
	std::cout << "\n";

	signal(SIGINT, interrupt_handler);
	if(touchSensor.setup(1, device, address))
	{
		fprintf(stderr, "Error while initialising device\n");
		return 1;
	}
	touchSensor.printDetails();
	while(!gShouldStop) {
		touchSensor.readI2C();
		// we print the sensor readings depending on the device mode,
		// so that if you change the device type above, you get meaningful
		// values for your device
		if(Trill::CENTROID == touchSensor.getMode()) {
			printf("Touches: %d:", touchSensor.getNumTouches());
			for(unsigned int i = 0; i < touchSensor.getNumTouches(); i++) {
				printf("%1.3f ", touchSensor.touchLocation(i));
				if(touchSensor.is2D())
					printf("%1.3f ", touchSensor.touchHorizontalLocation(i));
			}
		}
		else {
			for(unsigned int i = 0; i < touchSensor.getNumChannels(); i++)
				printf("%1.3f ", touchSensor.rawData[i]);
		}
		printf("\n");
		usleep(100000);
	}
	return 0;
}
