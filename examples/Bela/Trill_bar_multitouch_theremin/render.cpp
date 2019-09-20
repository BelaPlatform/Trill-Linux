#include <Bela.h>
#include <Trill.h>
#include <cmath>
#include <libraries/OnePole/OnePole.h>
#include <Oscillator.h>

#define NUM_OSC 5

Trill touchSensor;

int gPrescalerOpts[6] = {1, 2, 4, 8, 16, 32};
int gThresholdOpts[7] = {0, 10, 20, 30, 40, 50, 60};

// One Pole filter object declaration
OnePole freqFilt[NUM_OSC], ampFilt[NUM_OSC];

float gCutOffFreq = 5, gCutOffAmp = 15;

float gTouchLocation[NUM_OSC] = { 0.0, 0.0, 0.0, 0.0, 0.0 };

float gTouchSize[NUM_OSC] = { 0.0, 0.0, 0.0, 0.0, 0.0 };

int gNumActiveTouches = 0;

int gTouchSizeRange[2] = { 1500, 6000 };

int gTaskSleepTime = 50000;

Oscillator osc[NUM_OSC];

float gFreqRange[2] = { 200.0, 1500.0 };
float gAmplitudeRange[2] = { 0.0, 1.0 } ;

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
		
	for(unsigned int i = 0; i < NUM_OSC; i++) {
		osc[i].setup(context->audioSampleRate, gFreqRange[0], Oscillator::sine);
		// Setup low pass filters for smoothing frequency and amplitude
		freqFilt[i].setup(gCutOffFreq, context->audioSampleRate);
		ampFilt[i].setup(gCutOffAmp, context->audioSampleRate);
	}
		
	return true;
}

void render(BelaContext *context, void *userData)
{
	for(unsigned int n = 0; n < context->audioFrames; n++) {
		
		float out = 0.0;
		for(unsigned int i = 0; i < NUM_OSC; i++) {
			float frequency, amplitude;
			frequency = map(gTouchLocation[i], 0, 1, gFreqRange[0], gFreqRange[1]);
			frequency = freqFilt[i].process(frequency);
			osc[i].setFrequency(frequency);
			amplitude = map(gTouchSize[i], 0, 1, gAmplitudeRange[0], gAmplitudeRange[1]);
			amplitude = ampFilt[i].process(amplitude);
			
			
			out += (1.f/NUM_OSC) * amplitude * osc[i].process();
		}
	
		for(unsigned int channel = 0; channel < context->audioOutChannels; channel++) {
			audioWrite(context, n, channel, out);
		}
		
	}
}

void cleanup(BelaContext *context, void *userData)
{
	touchSensor.cleanup();
}
