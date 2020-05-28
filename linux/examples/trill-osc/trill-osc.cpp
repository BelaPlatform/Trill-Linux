/*
 ____  _____ _        _
| __ )| ____| |      / \
|  _ \|  _| | |     / _ \
| |_) | |___| |___ / ___ \
|____/|_____|_____/_/   \_\
http://bela.io
*/

const char* helpText =
"Read from a Trill device and send OSC messages"
"  Usage: %s <bus>\n"
"    <bus>: the bus to scan. Default is 1 (i.e.: /dev/i2c-1)\n"
"======================\n"
"\n"
"NOTE: as this program scans several addresses on the i2c bus\n"
"it could cause non-Trill peripherals connected to it to malfunction.\n";

#include <Trill.h>
#include <vector>
#include <string>
#include <memory>
#include <unistd.h>

#include "oscpkt.hh"
#include <iostream> // needed for udp.hh
#include "udp.hh"

#include <signal.h>
int gShouldStop = 0;
void interrupt_handler(int var)
{
	gShouldStop = true;
}

std::vector<std::unique_ptr<Trill> > gTouchSensors;
oscpkt::UdpSocket gSock;

int sendOsc(const std::string& address, float* values, unsigned int size);

int main(int argc, char** argv)
{
	unsigned int i2cBus = 1;
	if(argc >= 2)
	{
		if(std::string("--help") == std::string(argv[1])) {
			printf(helpText, argv[0]);
			return 0;
		}
		i2cBus = atoi(argv[1]);
	}
	printf("Trill devices detected on bus %d\n", i2cBus);
	for(uint8_t addr = 0x20; addr <= 0x50; ++addr)
	{
		Trill::Device device = Trill::probe(i2cBus, addr);
		if(Trill::NONE != device)
		{
			gTouchSensors.push_back(std::unique_ptr<Trill>(new Trill(i2cBus, device, addr)));
			gTouchSensors.back()->printDetails();
		}
	}

	signal(SIGINT, interrupt_handler);
	gSock.connectTo("localhost", 7563);

	std::string baseAddress = "/trill/" + std::to_string(i2cBus);
	std::string address;
	while(!gShouldStop) {
		for(auto& touchSensor : gTouchSensors) {
			Trill& t = *touchSensor;
			t.readI2C();
			address = baseAddress + "/" + std::to_string(int(t.getAddress()));
			if(Trill::CENTROID == t.getMode()) {
				float values[11];
				unsigned int len = 0;
				unsigned int numTouches = 0;
				if(t.is2D()) {
					address += "/touchXY";
					float size = t.compoundTouchSize();
					float touchY = t.compoundTouchLocation();
					float touchX = t.compoundTouchHorizontalLocation();
					values[1] = touchX;
					values[2] = touchY;
					values[3] = size;
					numTouches = size > 0;
					len = 1 + numTouches * 3;
				} else {
					address += "/touches";
					numTouches = t.getNumTouches();
					for(unsigned int i = 0; i < numTouches; i++) {
						values[1 + i * 2] = t.touchLocation(i);
						values[1 + i * 2 + 1] = t.touchSize(i);
					}
					len = 1 + numTouches * 2;
				}
				values[0] = numTouches;
				sendOsc(address, values, len);
			} else {
				address += "/diff";
				sendOsc(address, t.rawData.data(), t.rawData.size());
			}
		}
		usleep(100000);
	}
	return 0;
}

int sendOsc(const std::string& address, float* values, unsigned int size)
{
	if(!size)
		return 0;
	oscpkt::Message msg(address);
	for(unsigned int n = 0; n < size; ++n)
		msg.pushFloat(values[n]);
	oscpkt::PacketWriter pw;
	pw.addMessage(msg);
	bool ok = gSock.sendPacket(pw.packetData(), pw.packetSize());
	if(!ok) {
		fprintf(stderr, "could not send\n");
		return -1;
	}
	return 0;
}
