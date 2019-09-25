#include <Bela.h>
#include <Trill.h>
#include <cmath>
#include <libraries/Gui/Gui.h>

Gui gui;

#define NUM_OSC 5

Trill touchSensor;

int gPrescalerOpts[6] = {1, 2, 4, 8, 16, 32};
int gThresholdOpts[7] = {0, 10, 20, 30, 40, 50, 60};

float gTouchLocation[NUM_OSC] = { 0.0, 0.0, 0.0, 0.0, 0.0 };

float gTouchSize[NUM_OSC] = { 0.0, 0.0, 0.0, 0.0, 0.0 };

int gNumActiveTouches = 0;

int gTouchSizeRange[2] = { 100, 6000 };

int gTaskSleepTime = 100;

// Time period (in seconds) after which data will be sent to the GUI
float gTimePeriod = 0.005;

void loop(void*)
{
	while(!gShouldStop)
	{
		touchSensor.readLocations(); 
	    for(int i = 0; i <  touchSensor.numberOfTouches(); i++) {
	    	gTouchLocation[i] = map(touchSensor.touchLocation(i), 0, 3200, 0, 1);
			gTouchLocation[i] = constrain(gTouchLocation[i], 0, 1);
			gTouchSize[i] = map(touchSensor.touchSize(i), gTouchSizeRange[0], gTouchSizeRange[1], 0, 1);
	    	gTouchSize[i] = constrain(gTouchSize[i], 0, 1);
	    }
	    gNumActiveTouches = touchSensor.numberOfTouches();
		
		for(int i = gNumActiveTouches; i <  NUM_OSC; i++) {
			gTouchLocation[i] = 0.0;
			gTouchSize[i] = 0.0;
		}
		

	    
		usleep(gTaskSleepTime);
	}
}

bool setup(BelaContext *context, void *userData)
{
	
	if(touchSensor.setup(1, 0x18, Trill::NORMAL, gThresholdOpts[6], gPrescalerOpts[0]) != 0) {
		fprintf(stderr, "Unable to initialise touch sensor\n");
		return false;
	}
	
	Trill::Modes sensorMode = Trill::NORMAL;

	if(touchSensor.setMode(sensorMode)!= 0) {
		fprintf(stderr, "Unable to set sensor mode\n");
		return false;
	} else {
		fprintf(stdout, "Mode: %d\n", sensorMode);
	}

	if(touchSensor.setPrescaler(gPrescalerOpts[0]) == 0) {
		fprintf(stdout, "Prescaler set to %d.\n", gPrescalerOpts[0]);
	} else {
		return false;
	}

	if(touchSensor.setNoiseThreshold(gThresholdOpts[6]) == 0) {
		fprintf(stdout, "Threshold set to %d.\n", gThresholdOpts[6]);
	} else {
		return false;
	}
	
	if(touchSensor.updateBaseLine() != 0)
		return false;
	
	if(touchSensor.prepareForDataRead() != 0)
		return false;

	printf("Device type: %d\n", touchSensor.deviceType());
	printf("Firmware version: %d\n", touchSensor.firmwareVersion());

	if(touchSensor.deviceType() != Trill::ONED) {
		fprintf(stderr, "This example is supposed to work only with the Trill BAR. \n You may have to adapt it to make it work with other Trill devices.\n");
		return false;
	}

	Bela_scheduleAuxiliaryTask(Bela_createAuxiliaryTask(loop, 50, "I2C-read", NULL));	
	
	gui.setup(context->projectName);
	return true;
}

void render(BelaContext *context, void *userData)
{
	static unsigned int count = 0;
	for(unsigned int n = 0; n < context->audioFrames; n++) {
		if(count >= gTimePeriod*context->audioSampleRate) 
		{

			int numTouches[1] = { gNumActiveTouches };
			gui.sendBuffer(0, numTouches);
			gui.sendBuffer(1, gTouchLocation);
			gui.sendBuffer(2, gTouchSize);

			count = 0;
		}
		count++;
	}
}

void cleanup(BelaContext *context, void *userData)
{
	touchSensor.cleanup();
}
