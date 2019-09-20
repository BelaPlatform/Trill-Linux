#include <Bela.h>
#include <Trill.h>
#include <cmath>
#include <libraries/OnePole/OnePole.h>
#include <Oscillator.h>


Trill touchSensor;

int gPrescalerOpts[6] = {1, 2, 4, 8, 16, 32};
int gThresholdOpts[7] = {0, 10, 20, 30, 40, 50, 60};

// One Pole filter object declaration
OnePole freqFilt, ampFilt, detuneFilt;


float gTouchPosition[2] = { 0.0 , 0.0 };
float gTouchSize = 0.0;
int gTouchSizeRange[2] = { 500, 6000 };

int gTaskSleepTime = 5000;

Oscillator osc[2];

float gFreqRange[2] = { 200.0, 1500.0 };
float gAmplitudeRange[2] = { 0.0, 1.0 } ;
float gDetuneRange[2] = { 0.0, 50.0 };

void loop(void*)
{
	while(!gShouldStop)
	{
		touchSensor.readLocations(); 

		int avgLocation = 0;
		int avgSize = 0;
		int numTouches = 0;
		for(int i = 0; i < touchSensor.numberOfTouches(); i++) {
	        if(touchSensor.touchLocation(i) != 0) {
	        	avgLocation += touchSensor.touchLocation(i);
				avgSize += touchSensor.touchSize(i);
	        	numTouches += 1;
	        }
	    }
	    avgLocation = floor(1.0f * avgLocation / numTouches);
	    avgSize = floor(1.0f * avgSize / numTouches);
	    gTouchSize = map(avgSize, gTouchSizeRange[0], gTouchSizeRange[1], 0, 1);
	    gTouchSize = constrain(gTouchSize, 0, 1);
	    gTouchPosition[1] = map(avgLocation, 0, 1792, 0, 1);
	    gTouchPosition[1] = constrain(gTouchPosition[1], 0, 1);

		int avgHorizontalLocation = 0;
		int numHorizontalTouches = 0;
	    for(int i = 0; i < touchSensor.numberOfHorizontalTouches(); i++) {
	        if(touchSensor.touchHorizontalLocation(i) != 0) {
	        	avgHorizontalLocation += touchSensor.touchHorizontalLocation(i);
	        	numHorizontalTouches += 1;
	        }
	    }
		avgHorizontalLocation = floor(1.0f * avgHorizontalLocation / numHorizontalTouches);
		
		gTouchPosition[0] = map(avgHorizontalLocation, 0, 1792, 0, 1);
	    gTouchPosition[0] = constrain(gTouchPosition[0], 0, 1);

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

	if(touchSensor.deviceType() != Trill::TWOD) {
		fprintf(stderr, "This example is supposed to work only with the Trill SQUARE. \n You may have to adapt it to make it work with other Trill devices.\n");
		return false;
	}

	Bela_scheduleAuxiliaryTask(Bela_createAuxiliaryTask(loop, 50, "I2C-read", NULL));	
		
	// Setup low pass filters for smoothing frequency and amplitude
	freqFilt.setup(1, context->audioSampleRate); // Cut-off frequency = 1Hz
	ampFilt.setup(1, context->audioSampleRate); // Cut-off frequency = 1Hz
	detuneFilt.setup(1, context->audioSampleRate); // Cut-off frequency = 1Hz
	
	osc[0].setup(context->audioSampleRate, gFreqRange[0], Oscillator::square);
	osc[1].setup(context->audioSampleRate, gFreqRange[0], Oscillator::square);

	return true;
}

void render(BelaContext *context, void *userData)
{
	for(unsigned int n = 0; n < context->audioFrames; n++) {
	
		float frequency;
		// Map Y-axis to a frequency range	
		frequency = map(gTouchPosition[1], 0, 1, gFreqRange[0], gFreqRange[1]);
		// Smooth frequency using low-pass filter
		frequency = freqFilt.process(frequency);
		// Map touch size to a frequency detuning range
		float detuning;
		detuning = map(gTouchSize, 0, 1, gDetuneRange[0], gDetuneRange[1]);
		
		osc[0].setFrequency(frequency);
		osc[1].setFrequency(frequency+ detuning);


		
		float amplitude = ampFilt.process(gTouchPosition[0]);
		
		float out = amplitude * 0.5 * (osc[0].process() + osc[1].process());
		
		// Write sinewave to left and right channels
		for(unsigned int channel = 0; channel < context->audioOutChannels; channel++) {
			audioWrite(context, n, channel, .2f * out);
		}
	}
}

void cleanup(BelaContext *context, void *userData)
{
	touchSensor.cleanup();
}
