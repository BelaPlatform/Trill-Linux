#include <Trill.h>

#include <signal.h>
#include <vector>

const char* helpText =
"Connect to one Trill device and print its readings to the console\n"
"  Usage: %s <bus> <device-name> [[[[<address>] <threshold>] <prescaler>] <num-pads>]\n"
"         <bus> is the bus that the device is connected to (i.e.: the X in /dev/i2c-X)\n"
"         <device-name> is the name of the device (e.g.: `bar`, `square`,\n"
"	                `craft`, `hex`, ring`, ...)\n"
"          <address> (optional) is the address of the device. If this is\n"
"                    not passed, the default address for the specified device\n"
"                    type will be used instead.\n"
"          <threshold> (optional) the threshold at which a pad is considered `OK`\n"
"          <prescaler> (optional) the device prescaler\n"
"          <num-pads> (optional) is the number of pads to test. If this is\n"
"                    not passed, the default number of pads for the specified\n"
"                    device type will be used insted.\n"
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
	int numPads = -1; // Number of pads
	float threshold = 0.1;
	int prescaler = -1;
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
		} else if(4 == c) {
			threshold = std::stof(argv[c]);
		} else if(5 == c) {
			prescaler = std::stoi(argv[c]);
		} else if(6 == c) {
			numPads = std::stoi(argv[c]);
		}
	}
	printf("Settings:\n");
	printf("i2cBus: %d\n", i2cBus);
	printf("deviceName: %s\n", deviceName.c_str());
	printf("address: %#x\n", address);
	printf("threshold: %.3f\n", threshold);
	printf("prescaler: %d\n", prescaler);
	printf("numPads: %d\n", numPads);
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
	// Set differential mode
	if(touchSensor.setMode(Trill::DIFF) != 0)
	{
		fprintf(stderr, "Couldn't set device in differential mode\n");
		return 1;
	}
	if(-1 != prescaler)
	{
		if(touchSensor.setPrescaler(prescaler))
		{
			fprintf(stderr, "Couldn't set device prescaler to %d\n", prescaler);
			return 1;
		}
	}

	// Get num channels
	int nChannels = touchSensor.getNumChannels();
	if(numPads == -1)
	{
		numPads = nChannels;
	}
	else if(numPads > nChannels)
	{
		fprintf(stdout, "The number of specified pads (%d) exceeds the number of available channels (%d). Using %d pads for testing.\n", numPads, nChannels, nChannels);
	}

	// Diff range
	std::vector<float> diffRange (numPads, 0.0);

	touchSensor.printDetails();
	signal(SIGINT, interrupt_handler);

	while(!gShouldStop) {
		int ret = touchSensor.readI2C();
		if(ret) {
			fprintf(stderr, "Reading failed. Check your connections\n");
			return 1;
		}

		bool testPassed = true;

		if(Trill::DIFF == touchSensor.getMode()) {
			for(unsigned int i = 0; i < numPads; i++)
			{
				// Read raw data
				float reading = touchSensor.rawData[i];
				// Check if diff reading is greater than previous and update
				if(reading > diffRange[i])
					diffRange[i] = reading;

				// Check if pad passes test
				bool padPassed = ( diffRange[i] > threshold );

				testPassed = testPassed && padPassed;

				if(padPassed)
					printf("%4s ", "OK");
				else
					printf("%4.2f ", reading);
			}
		}
		printf("\n");

		if(testPassed)
		{
			fprintf(stdout, "The sensor has passed the test, all %d pads seem to be working.\n", numPads);
			return 0;
		}

		usleep(100000);
	}
	return 2;
}
