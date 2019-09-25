#include <Bela.h>
#include <Trill.h>
#include <cmath>

#define NUM_TOUCH 5
#define NUM_LED 12

Trill touchSensor;

int gPrescalerOpts[6] = {1, 2, 4, 8, 16, 32};
int gThresholdOpts[7] = {0, 10, 20, 30, 40, 50, 60};

float gTouchLocation[NUM_TOUCH] = { 0.0, 0.0, 0.0, 0.0, 0.0 };

float gTouchSize[NUM_TOUCH] = { 0.0, 0.0, 0.0, 0.0, 0.0 };

int gNumActiveTouches = 0;

int gTouchSizeRange[2] = { 100, 6000 };

int gTaskSleepTime = 100;

float gTimePeriod = 0.01;

unsigned int gLedPins[NUM_LED] = { 0, 1, 2, 3, 4, 5, 8, 9, 11, 12, 13, 14 };
bool gLedStatus[NUM_LED] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

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
		
		for(int i = gNumActiveTouches; i <  NUM_TOUCH; i++) {
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

	for(unsigned int l = 0; l < NUM_LED; l++)
		pinMode(context, 0, gLedPins[l], OUTPUT); 

	return true;
}

void render(BelaContext *context, void *userData)
{
	bool activeSections[NUM_LED] = { false };
	static unsigned int count = 0;
	for(unsigned int n = 0; n < context->audioFrames; n++) {
		for(unsigned int t = 0; t < gNumActiveTouches; t++) {
			int section = floor( NUM_LED * gTouchLocation[t] );
			activeSections[section] = activeSections[section] || 1;
		}
		for(unsigned int l = 0; l < NUM_LED; l++) {
			gLedStatus[l] = activeSections[l];
			digitalWrite(context, n, gLedPins[l], gLedStatus[l]);	
		}
		
		if(count >= gTimePeriod*context->audioSampleRate) 
		{
			for(unsigned int l = 0; l < NUM_LED; l++)
				rt_printf("%d ",gLedStatus[l]);
			rt_printf("\n");
			count = 0;
		}
		count++;
	}
}

void cleanup(BelaContext *context, void *userData)
{
	touchSensor.cleanup();
}
