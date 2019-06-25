/***** Trill_centroid.h *****/
// Quick and dirty class to handle parsing touch location
// from raw values. Really this should be done the right way
// by getting it from the sensor itself in the appropriate mode.

#include <Trill.h>

#define MAX_TOUCHES 	3

class Trill_centroid : public Trill {
public:
	Trill_centroid() : Trill(), numTouches_(0) {}
	Trill_centroid(int i2c_bus, int i2c_address) : Trill(i2c_bus, i2c_address), numTouches_(0) {}

	// Override readI2C() method to update the touch locations
	// calculated from raw values
	int readI2C();
	
	int numTouches() { return numTouches_; }
	float touchLocation(int index) {
		if(index < 0 || index >= MAX_TOUCHES)
			return -1.0;
		if(index >= numTouches_)
			return -1.0;
		return touchLocations_[index];
	}

private:
	int numTouches_;
	float touchLocations_[MAX_TOUCHES];
};
