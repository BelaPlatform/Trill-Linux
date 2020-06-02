#include <Trill.h>
#include <signal.h>
const char* helpText =
"Connect to one Trill device, set it to RAW mode, change some settings and\n"
"print its readings to the console\n"
"  Usage: %s <bus> <device-name> [<address>]\n"
"         <bus> is the bus that the device is connected to (i.e.: the X in /dev/i2c-X)\n"
"         <device-name> is the name of the device (e.g.: `bar`, `square`,\n"
"	                `craft`, `hex`, ring`, ...)\n"
"          <address> (optional) is the address of the device. If this is\n"
"                    not passed, the default address for the specified device\n"
"                    type will be used instead.\n"
;

Trill touchSensor;

int gShouldStop;

void interrupt_handler(int var)
{
	gShouldStop = true;
}

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
			if(!address) // if failed, try again as hex
				address = std::stoi(argv[c], 0, 16);
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
	printf("Opening device %s on bus %d ", Trill::getNameFromDevice(device).c_str(), i2cBus);
	if(255 != address)
		printf("at address: %#4x(%d)", address, address);
	printf("\n");

	if(touchSensor.setup(i2cBus, device, address))
	{
		fprintf(stderr, "Error while initialising device\n");
		return 1;
	}
	touchSensor.printDetails();

	if(touchSensor.setMode(Trill::RAW) == 0) {
		printf("Operation mode set to %s.\n", Trill::getNameFromMode(Trill::RAW));
	} else {
		fprintf(stderr, "Communication error\n");
		return 1;
	}

	unsigned int newSpeed = 0;
	if(touchSensor.setScanSettings(newSpeed) == 0) {
		printf("Scan speed set to %d.\n", newSpeed);
	} else {
		fprintf(stderr, "Communication error\n");
		return 1;
	}

	unsigned int newPrescaler = 3;
	if(touchSensor.setPrescaler(newPrescaler) == 0) {
		printf("Prescaler set to %d.\n", newPrescaler);
	} else {
		fprintf(stderr, "Communication error\n");
		return 1;
	}

	float newThreshold = 0.01;
	if(touchSensor.setNoiseThreshold(newThreshold) == 0) {
		printf("Threshold set to %d.\n", newThreshold);
	} else {
		fprintf(stderr, "Communication error\n");
		return 1;
	}

	if(touchSensor.updateBaseline() != 0) {
		fprintf(stderr, "Communication error\n");
		return 1;
	}

	signal(SIGINT, interrupt_handler);
	while(!gShouldStop)
	{
		touchSensor.readI2C();
		printf("raw: ");
		for(unsigned int i = 0; i < touchSensor.getNumChannels(); i++)
			printf("%1.3f ", touchSensor.rawData[i]);
		printf("\n");
		usleep(100000);
	}
	return 0;
}
