#include <Trill.h>

#include <signal.h>
int gShouldStop;

void interrupt_handler(int var)
{
	gShouldStop = true;
}

Trill touchSensor;

int main()
{
	signal(SIGINT, interrupt_handler);
	touchSensor.setup();
	while(!gShouldStop) {
		touchSensor.readI2C();
		for(unsigned int i = 0; i < sizeof(touchSensor.rawData)/sizeof(int); i++) {
			printf("%5d ", touchSensor.rawData[i]);
		}
		printf("\n");
	}
	usleep(10000);
	return 0;
}
