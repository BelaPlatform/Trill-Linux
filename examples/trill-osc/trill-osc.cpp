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
"    <bus>: the bus to scan for devices (i.e.: /dev/i2c-<bus>).\n"
"           If omitted, no device is automatically opened, but it can be\n"
"           created via OSC with `new` or `createAll`.\n"
"======================\n"
"\n"
"NOTE: as this program scans several addresses on the i2c bus\n"
"it could cause non-Trill peripherals connected to it to malfunction.\n";


#include <Trill.h>
#include <vector>
#include <string>
#include <memory>
#include <map>
#include <unistd.h>

#define OSCPKT_OSTREAM_OUTPUT
#include "oscpkt.hh"
#include <iostream> // needed for udp.hh
#include "udp.hh"

#include <signal.h>
int gShouldStop = 0;
void interrupt_handler(int var)
{
	gShouldStop = true;
}

typedef enum {
	DONT,
	ONCE,
	YES,
} ShouldRead;

struct TrillDev {
	std::unique_ptr<Trill> t;
	ShouldRead shouldRead;
};

std::map<std::string, struct TrillDev> gTouchSensors;
oscpkt::UdpSocket gSock;

int parseOsc(oscpkt::Message& msg);
int sendOsc(const std::string& address, float* values, unsigned int size);

void createAllSensorsOnBus(unsigned int i2cBus) {
	printf("Trill devices detected on bus %d\n", i2cBus);
	for(uint8_t addr = 0x20; addr <= 0x50; ++addr)
	{
		Trill::Device device = Trill::probe(i2cBus, addr);
		if(Trill::NONE != device)
		{
			std::string id = std::to_string(i2cBus) + "-" + Trill::getNameFromDevice(device) + "-" + std::to_string(addr);
			gTouchSensors[id] = {std::unique_ptr<Trill>(new Trill(i2cBus, device, addr)), YES};
			printf("Device id: %s\n", id.c_str());
			gTouchSensors[id].t->printDetails();
		}
	}
	//TODO: sendInfo
}

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
		createAllSensorsOnBus(i2cBus);
	}

	signal(SIGINT, interrupt_handler);
	gSock.connectTo("localhost", 7563);
	gSock.bindTo(7562);

	std::string baseAddress = "/trill/" + std::to_string(i2cBus);
	std::string address;
	while(!gShouldStop) {
		for(auto& touchSensor : gTouchSensors) {
			if(ShouldRead::DONT == touchSensor.second.shouldRead)
				continue;
			if(ShouldRead::ONCE == touchSensor.second.shouldRead)
				touchSensor.second.shouldRead = ShouldRead::DONT;
			Trill& t = *(touchSensor.second.t);
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
		// process incoming mesages
		while(gSock.isOk() && gSock.receiveNextPacket(1)) {// timeout
			oscpkt::PacketReader pr(gSock.packetData(), gSock.packetSize());
			oscpkt::Message *msg;
			while (pr.isOk() && (msg = pr.popMessage()) != 0) {
				std::cout << "Received " << *msg << "\n";
				parseOsc(*msg);
			}

		}
		usleep(100000);
	}
	return 0;
}

// 1.3 of https://www.fluentcpp.com/2017/04/21/how-to-split-a-string-in-c/
#include <sstream>
std::vector<std::string> split(const std::string& s, char delimiter)
{
	std::vector<std::string> tokens;
	std::string token;
	std::istringstream tokenStream(s);
	while (std::getline(tokenStream, token, delimiter))
	{
		tokens.push_back(token);
	}
	return tokens;
}

int parseOsc(oscpkt::Message& msg)
{
	oscpkt::Message::ArgReader args = msg.partialMatch("/trill/command/");
	if(!args.isOk()) {
		return -1;
	}
	std::string typeTags = msg.typeTags();
	std::cout << "Trill: received " << msg << "\n";
	std::vector<std::string> tokens = split(msg.addressPattern(), '/');
	if(tokens.size() != 4) {
		fprintf(stderr, "Unexpected address pattern %s\n", msg.addressPattern().c_str());
		return -1;
	}
	const std::string command = tokens.back();

	// tmp variables for args parsing
	float value0;
	float value1;
	std::string str0;

	// global messages
	if("list" == command) {
		printf("list\n");
		return 0;
	} else if("createAll" == command && "f" == typeTags && args.popFloat(value0)) {
		printf("createAll %f\n", value0);
		return 0;
	}

	//instance messages: they all start with an id
	// check and retrieve first argument: id
	if(typeTags[0] != 's') {
		fprintf(stderr, "Unexpected first argument type: `%c` at %s\n", typeTags[0], msg.addressPattern().c_str());
		return -1;
	}
	std::string id;
	args.popStr(id);
	typeTags = typeTags.substr(1);// peel it off

	if(gTouchSensors.find(id) == gTouchSensors.end() && "new" != command) {
		fprintf(stderr, "Unknown id: %s at %s\n", id.c_str(), msg.addressPattern().c_str());
		return -1;
	}
	if("new" == command) {
		bool ok = false;
		std::string deviceName;
		float bus;
		const float defaultI2cAddr = 255;
		float i2cAddr = defaultI2cAddr;
		// at least two arguments: bus, deviceName
		if(typeTags.size() >= 2 && 'f' == typeTags[0] && 's' == typeTags[1]) {
			args.popFloat(bus).popStr(deviceName);
			if(
				// optional third argument: address
				(typeTags.size() == 3 && 'f' == typeTags[2] && args.popFloat(i2cAddr).isOkNoMoreArgs())
				|| args.isOkNoMoreArgs()
			  )
			{
				Trill::Device device = Trill::getDeviceFromName(deviceName);
				if(Trill::UNKNOWN != device || defaultI2cAddr != i2cAddr)
					ok = true;
			}
		}
		if(ok) {
			printf("new %f %s %f\n", bus, deviceName.c_str(), i2cAddr);
			return 0;
		} else {
			std::cerr << "Unknown message " << msg << "\n";
			return 1;
		}
	} else if ("delete" == command) {
		printf("delete\n");
		return 0;
	}
	// commands below need to access a device. If we get to this line, the
	// device exists in gTouchSensors
	Trill& t = *(gTouchSensors[id].t);
	printf("id: %s - ", id.c_str());
	if ("autoScan" == command && "f" == typeTags && args.popFloat(value0).isOkNoMoreArgs()) {
		printf("autoScan: %f\n", value0);
	} // commands below simply map to the corresponding Trill class methods
	else if("updateBaseline" == command && "" == typeTags) {
		printf("UPDATEBASELINE\n");
	} else if ("setPrescaler" == command && "f" == typeTags && args.popFloat(value0).isOkNoMoreArgs()) {
		printf("setPrescaler %f\n", value0);
	} else if ("setNoiseThreshold" == command && "f" == typeTags && args.popFloat(value0).isOkNoMoreArgs()) {
		printf("setNoiseThreshold %f\n", value0);
	} else if ("setMode" == command && "s" == typeTags && args.popStr(str0).isOkNoMoreArgs()) {
		printf("setMode: %s\n", str0.c_str());
	} else {
		std::cerr << "Unknown message " << msg << "\n";
		return 1;
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
