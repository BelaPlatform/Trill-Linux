#include <Bela.h>
#include <Trill.h>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <unistd.h>

#define NUM_SENSORS 26

// Filter info
float *gFilterLastInputs;
float *gFilterLastOutputs;
unsigned long long *gFilterSampleCounts;
float gCoeffB0 = 0, gCoeffB1 = 0, gCoeffB2 = 0, gCoeffA1 = 0, gCoeffA2 = 0;

// Oscillator bank info -- for passing across threads
int gNumOscillators = 0;
int gWavetableLength = 1024;
float gAudioSampleRate = 44100.0;
unsigned long long gAudioFramesElapsed = 0;
bool gIsStdoutTty;

float *gWavetable;		// Buffer holding the precalculated sine lookup table
float *gPhases;			// Buffer holding the phase of each oscillator
float *gFrequencies;	// Buffer holding the frequencies of each oscillator
float *gAmplitudes;		// Buffer holding the amplitudes of each oscillator
float *gDFrequencies;	// Buffer holding the derivatives of frequency
float *gDAmplitudes;	// Buffer holding the derivatives of amplitude

void calculate_coeff(float sampleRate, float cutFreq);
bool initialise_oscillators(float fundamental_frequency);

extern "C" {
	// Function prototype for ARM assembly implementation of oscillator bank
	void oscillator_bank_neon(int numAudioFrames, float *audioOut,
							  int activePartialNum, int lookupTableSize,
							  float *phases, float *frequencies, float *amplitudes,
							  float *freqDerivatives, float *ampDerivatives,
							  float *lookupTable);
}

// Trill info
Trill touchSensor;

int prescalerOpts[6] = {1, 2, 4, 8, 16, 32};
int thresholdOpts[7] = {0, 10, 20, 30, 40, 50, 60};

int gSensorRange[2] = { 200, 2000 };
float gSensorReading[NUM_SENSORS] = { 0.0 };
int gTaskSleepTime = 50000;

void loop(void*)
{
	while(!gShouldStop)
	{
		if(touchSensor.isReady()) {
			touchSensor.readI2C();

			for(unsigned int i = 0; i < NUM_SENSORS; i++) {
				gSensorReading[i] = map(touchSensor.rawData[i], gSensorRange[0], gSensorRange[1], 0, 1);
				gSensorReading[i] = constrain(gSensorReading[i], 0, 1);
			}
		} else {
			printf("Sensor is not ready\n");
		}

		usleep(gTaskSleepTime);
	}
}

bool setup(BelaContext *context, void *userData)
{
	if(context->audioOutChannels != 2) {
		rt_printf("Error: this example needs stereo audio enabled\n");
		return false;
	}
	
	gAudioSampleRate = context->audioSampleRate;
	
	if(touchSensor.setup() != 0) {
		fprintf(stderr, "Unable to initialise touch sensor\n");
		return false;
	}

	Trill::Modes sensorMode = Trill::DIFF;

	if(touchSensor.setMode(sensorMode)!= 0) {
		fprintf(stderr, "Unable to set sensor mode\n");
		return false;
	} else {
		fprintf(stdout, "Mode = %d\n", sensorMode);
	}

	int newPrescaler = prescalerOpts[0];
	if(touchSensor.setPrescaler(newPrescaler) == 0) {
		fprintf(stdout, "Prescaler set to %d.\n", newPrescaler);
	} else {
		return false;
	}

	int newThreshold= thresholdOpts[6];
	if(touchSensor.setNoiseThreshold(newThreshold) == 0) {
		fprintf(stdout, "Threshold set to %d.\n", newThreshold);
	} else {
		return false;
	}
	

	if(touchSensor.updateBaseLine() != 0)
		return false;
	
	if(touchSensor.prepareForDataRead() != 0)
		return false;
		
	printf("Device type: %d\n", touchSensor.deviceType());
	printf("Firmware version: %d\n", touchSensor.firmwareVersion());
	printf("Mode: %d\n", touchSensor.getMode());

	Bela_scheduleAuxiliaryTask(Bela_createAuxiliaryTask(loop, 50, "I2C-read", NULL));
	
	// Allocate filter buffers: 2 previous samples per filter, 1 filter per input
	gFilterLastInputs = (float *)malloc(2 * NUM_SENSORS * sizeof(float));
	gFilterLastOutputs = (float *)malloc(2 * NUM_SENSORS * sizeof(float));

	if(gFilterLastInputs == 0 || gFilterLastOutputs == 0) {
		rt_printf("Unable to allocate memory buffers.\n");
		return false;
	}

	memset(gFilterLastInputs, 0, 2 * NUM_SENSORS * sizeof(float));
	memset(gFilterLastOutputs, 0, 2 * NUM_SENSORS * sizeof(float));

	// Allocate a buffer to hold sample counts (to display actual sample rate)
	gFilterSampleCounts = (unsigned long long *)malloc(NUM_SENSORS * sizeof(unsigned long long));

	if(gFilterSampleCounts == 0) {
		rt_printf("Unable to allocate memory buffers.\n");
		return false;
	}

	memset(gFilterSampleCounts, 0, NUM_SENSORS * sizeof(unsigned long long));

	// Initialise filter coefficients.
	calculate_coeff(gAudioSampleRate, 50.0);

	// Initiliase the oscillator bank in a harmonic series, one for each input
	gNumOscillators = NUM_SENSORS;
	if(!initialise_oscillators(55.0))
		return false;
		
	gIsStdoutTty = isatty(1); // Check if stdout is a terminal
	
	return true;
}

void render(BelaContext *context, void *userData)
{
	for(unsigned int n = 0; n < context->analogFrames; n++) {
		for(unsigned int i = 0; i < NUM_SENSORS; i++) {
			// Read sample
			float input = gSensorReading[i];

			// Calculate filtered output
			float output = gCoeffB0 * input
							+ gCoeffB1 * gFilterLastInputs[i * 2]
							+ gCoeffB2 * gFilterLastInputs[i * 2 + 1]
							- gCoeffA1 * gFilterLastOutputs[i * 2]
							- gCoeffA2 * gFilterLastOutputs[i * 2 + 1];

			// Save the filter history
			gFilterLastInputs[i*2 + 1] = gFilterLastInputs[i*2];
			gFilterLastInputs[i*2] = input;
			gFilterLastOutputs[i*2 + 1] = gFilterLastOutputs[i*2];
			gFilterLastOutputs[i*2] = output;

			// Increment count (for determining sample rate)
			gFilterSampleCounts[i]++;

			// Use output to control oscillator amplitude (with some headroom)
			// Square it to de-emphasise low but nonzero values
			gAmplitudes[i] = output * output / 4.f;
		}
	}
	
	// Render oscillator bank:

	// Initialise buffer to 0
	memset(context->audioOut, 0, context->audioOutChannels * context->audioFrames * sizeof(float));

	// Render audio frames
	oscillator_bank_neon(context->audioFrames, context->audioOut,
			gNumOscillators, gWavetableLength,
			gPhases, gFrequencies, gAmplitudes,
			gDFrequencies, gDAmplitudes,
			gWavetable);
	
}

void cleanup(BelaContext *context, void *userData)
{
	free(gFilterLastInputs);
	free(gFilterLastOutputs);
	free(gFilterSampleCounts);
	free(gWavetable);
	free(gPhases);
	free(gFrequencies);
	free(gAmplitudes);
	free(gDFrequencies);
	free(gDAmplitudes);
}

// Calculate the filter coefficients
// second order low pass butterworth filter

void calculate_coeff(float sampleRate, float cutFreq)
{
	// Initialise any previous state (clearing buffers etc.)
	// to prepare for calls to render()
	double f = 2*M_PI*cutFreq/sampleRate;
	double denom = 4+2*sqrt(2)*f+f*f;
	gCoeffB0 = f*f/denom;
	gCoeffB1 = 2*gCoeffB0;
	gCoeffB2 = gCoeffB0;
	gCoeffA1 = (2*f*f-8)/denom;
	gCoeffA2 = (f*f+4-2*sqrt(2)*f)/denom;
}

// Set up the oscillator bank
bool initialise_oscillators(float fundamental_frequency)
{
	// Initialise the sine wavetable
	if(posix_memalign((void **)&gWavetable, 8, (gWavetableLength + 1) * sizeof(float))) {
		rt_printf("Error allocating wavetable\n");
		return false;
	}
	for(int n = 0; n < gWavetableLength + 1; n++)
		gWavetable[n] = sinf(2.0 * M_PI * (float)n / (float)gWavetableLength);

	// Allocate the other buffers
	if(posix_memalign((void **)&gPhases, 16, gNumOscillators * sizeof(float))) {
		rt_printf("Error allocating phase buffer\n");
		return false;
	}
	if(posix_memalign((void **)&gFrequencies, 16, gNumOscillators * sizeof(float))) {
		rt_printf("Error allocating frequency buffer\n");
		return false;
	}
	if(posix_memalign((void **)&gAmplitudes, 16, gNumOscillators * sizeof(float))) {
		rt_printf("Error allocating amplitude buffer\n");
		return false;
	}
	if(posix_memalign((void **)&gDFrequencies, 16, gNumOscillators * sizeof(float))) {
		rt_printf("Error allocating frequency derivative buffer\n");
		return false;
	}
	if(posix_memalign((void **)&gDAmplitudes, 16, gNumOscillators * sizeof(float))) {
		rt_printf("Error allocating amplitude derivative buffer\n");
		return false;
	}

	for(int n = 0; n < gNumOscillators; n++) {
		// Randomise phases so they don't all line up as a high-amplitude pulse
		gPhases[n] = (float)random() / (float)RAND_MAX;

		// For efficiency, frequency is expressed in change in wavetable position per sample, not Hz or radians
		// Stretch the partials a little bit to sound more interesting and less synthetic
		gFrequencies[n] = fundamental_frequency * powf(1.0 + n, 1.002) * (float)gWavetableLength / 44100.0;

		// Oscillators start silent
		gAmplitudes[n] = 0.0;
		gDFrequencies[n] = gDAmplitudes[n] = 0.0;
	}

	gAmplitudes[3] = 0.5;

	return true;
}
