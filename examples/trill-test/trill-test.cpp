#include <Trill.h>

#include <signal.h>
#include <vector>

#define DIFF_THRESHOLD 200

const char* helpText =
"Connect to one Trill device and print its readings to the console\n"
"  Usage: %s <bus> <device-name> [<address>]\n"
"         <bus> is the bus that the device is connected to (i.e.: the X in /dev/i2c-X)\n"
"         <device-name> is the name of the device (e.g.: `bar`, `square`,\n"
"	                `craft`, `hex`, ring`, ...)\n"
"          <address> (optional) is the address of the device. If this is\n"
"                    not passed, the default address for the specified device\n"
"                    type will be used instead.\n"
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
			numPads = std::stoi(argv[c]);
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
	// Set differential mode
	if(touchSensor.setMode(Trill::DIFF) != 0)
	{
		fprintf(stderr, "Couldn't set device in differential mode\n");
		return 1;
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
		touchSensor.readI2C();

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
				bool padPassed = ( diffRange[i] > (float)(DIFF_THRESHOLD * 0.001) );

				testPassed = testPassed && padPassed;

				if(padPassed)
					printf("OK\t");
				else
					printf("%1.3f\t", reading);
			}
		}
		printf("\n");

		if(testPassed)
		{
			fprintf(stdout, "The sensor has passed the test, all %d pads seem to be working.\n", numPads);
			gShouldStop = true;
		}

		usleep(100000);
	}
	return 0;
}
