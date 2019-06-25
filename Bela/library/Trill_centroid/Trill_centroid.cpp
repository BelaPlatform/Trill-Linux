/***** Trill_centroid.cpp *****/

#include "Trill_centroid.h"

int Trill_centroid::readI2C()
{
	// Read the I2C data
	int result = 0;
	if((result = Trill::readI2C()) != 0) {
		numTouches_ = 0;
		return result;
	}
	
	// Go through the values and pick out the centroids
	int touchNum = 0;
	int peakVal = 0;
	int troughDepth = 0;
	float weighted = 0;
	float unweighted = 0;
	float touchVals[MAX_TOUCHES] = {0};
	
	for(unsigned int j = 0; j < 26; j++) {
		unweighted += rawData[j];
		weighted += rawData[j]*(j+1);
		if(rawData[j] > peakVal)
			peakVal = rawData[j];
		if(peakVal - rawData[j] > troughDepth)
			troughDepth = peakVal - rawData[j];
		if(j > 1) {
			if((troughDepth > 400) && (rawData[j] - rawData[j - 1] > 100)) {
				touchVals[touchNum] = weighted / unweighted;
				touchNum++;
				weighted = 0;
				unweighted = 0;
				peakVal = 0;
				troughDepth = 0;
				if(touchNum >= 3)
					break;
			}
		}
	}
	if(touchNum < MAX_TOUCHES && unweighted != 0) {
		touchVals[touchNum] = weighted / unweighted;
		touchNum++;
	}	
	
	// Copy to instances variables
	numTouches_ = touchNum;
	for(int j = 0; j < touchNum; j++) {
		if(j >= MAX_TOUCHES)
			break;
		touchLocations_[j] = touchVals[j];
	}
	
	return 0;
}
