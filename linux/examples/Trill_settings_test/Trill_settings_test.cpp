#include <Trill.h>
#include <signal.h>

Trill touchSensor;

int speedOpts[4] = {0, 1, 2, 3};
int prescalerOpts[6] = {1, 2, 4, 8, 16, 32};
int thresholdOpts[7] = {0, 10, 20, 30, 40, 50, 60};
int bitResolution = 12;
int gShouldStop;

void interrupt_handler(int var)
{
	gShouldStop = true;
}

int main()
{
	if(touchSensor.setup(1, Trill::BAR) != 0) {
		fprintf(stderr, "Unable to initialise touch sensor\n");
		return false;
	}

	unsigned int newSpeed = speedOpts[0];
	if(touchSensor.setScanSettings(newSpeed) == 0) {
		fprintf(stderr, "Scan speed set to %d.\n", newSpeed);
	} else {
		return false;
	}

	unsigned int newPrescaler = 3;
	if(touchSensor.setPrescaler(newPrescaler) == 0) {
		fprintf(stderr, "Prescaler set to %d.\n", newPrescaler);
	} else {
		return false;
	}

	float newThreshold = 0.01;
	if(touchSensor.setNoiseThreshold(newThreshold) == 0) {
		fprintf(stderr, "Threshold set to %d.\n", newThreshold);
	} else {
		return false;
	}

	if(touchSensor.updateBaseline() != 0)
		return false;

	signal(SIGINT, interrupt_handler);
	while(!gShouldStop)
	{
		touchSensor.readI2C();
		for(unsigned int i = 0; i < touchSensor.rawData.size(); i++) {
			printf("%.3f ", touchSensor.rawData[i]);
		}	
		printf("\n");
		usleep(10000);
	}
	return 0;
}
