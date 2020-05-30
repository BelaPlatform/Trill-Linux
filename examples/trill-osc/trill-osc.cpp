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
unsigned int gLoopSleep = 20;

void interrupt_handler(int var)
{
	gShouldStop = true;
}

typedef enum {
	DONT,
	ONCE,
	ALWAYS,
} ShouldRead;

struct TrillDev {
	std::unique_ptr<Trill> t;
	ShouldRead shouldRead;
};

std::map<std::string, struct TrillDev> gTouchSensors;
oscpkt::UdpSocket gSock;

int parseOsc(oscpkt::Message& msg);
int sendOscFloats(const std::string& address, float* values, unsigned int size);
int sendOscTrillDev(const std::string& id, const TrillDev& trillDev);
int sendOscReply(const std::string& command, const std::string& id, int ret);

int newTrillDev(const std::string& id, unsigned int i2cBus, Trill::Device device, uint8_t i2cAddr, ShouldRead shouldRead)
{
	gTouchSensors[id] = {std::unique_ptr<Trill>(new Trill(i2cBus, device, i2cAddr)), shouldRead};
	Trill& t = *gTouchSensors[id].t;
	if(Trill::NONE == t.deviceType()) {
		gTouchSensors.erase(id);
		return -1;
	}
	// ensure the sensor scans continuously even though we read it only
	// occasionally
	t.setAutoScanInterval(1);
	printf("Device id: %s\n", id.c_str());
	t.printDetails();
	sendOscTrillDev(id, gTouchSensors[id]);
	return 0;
}

void createAllSensorsOnBus(unsigned int i2cBus, bool autoRead) {
	printf("Trill devices detected on bus %d\n", i2cBus);
	for(uint8_t addr = 0x20; addr <= 0x50; ++addr)
	{
		Trill::Device device = Trill::probe(i2cBus, addr);
		if(Trill::NONE != device)
		{
			std::string id = std::to_string(i2cBus) + "-" + Trill::getNameFromDevice(device) + "-" + std::to_string(addr);
			ShouldRead shouldRead = autoRead ? ALWAYS : DONT;
			newTrillDev(id, i2cBus, device, addr, shouldRead);
		}
	}
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
		createAllSensorsOnBus(i2cBus, true);
	}

	signal(SIGINT, interrupt_handler);
	gSock.connectTo("localhost", 7563);
	gSock.bindTo(7562);

	std::string baseAddress = "/trill/";
	std::string address;
	while(!gShouldStop) {
		for(auto& touchSensor : gTouchSensors) {
			if(ShouldRead::DONT == touchSensor.second.shouldRead)
				continue;
			if(ShouldRead::ONCE == touchSensor.second.shouldRead)
				touchSensor.second.shouldRead = ShouldRead::DONT;
			Trill& t = *(touchSensor.second.t);
			t.readI2C();
			address = baseAddress + "readings/" + touchSensor.first;
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
				sendOscFloats(address, values, len);
			} else {
				address += "/diff";
				sendOscFloats(address, t.rawData.data(), t.rawData.size());
			}
		}
		// process incoming mesages
		while(!gShouldStop && gSock.isOk() && gSock.receiveNextPacket(1)) {// timeout
			oscpkt::PacketReader pr(gSock.packetData(), gSock.packetSize());
			oscpkt::Message *msg;
			while (!gShouldStop && pr.isOk() && (msg = pr.popMessage()) != 0) {
				std::cout << "Received " << *msg << "\n";
				parseOsc(*msg);
			}

		}
		int sleep = gLoopSleep;
		// sleep overall about gLoopSleep ms, but if it's too large, do it
		// in smaller chunks to be reactive to incoming messages
		int chunk = 10; // chunks of 10ms
		int slept = 0;
		while(!gShouldStop && sleep > 0) {
			if(sleep >= chunk) {
				sleep -= chunk;
				slept += chunk;
				usleep(chunk * 1000);
			} else {
				slept += sleep;
				usleep(sleep * 1000);
				sleep = 0;
			}
		}
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

int sendOscList();

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
	printf("Command: %s\n", command.c_str());

	// tmp variables for args parsing
	float value0;
	float value1;
	std::string str0;

	// global commands
	if("listAll" == command && args.isOkNoMoreArgs()) {
		printf("listAll\n");
		sendOscList();
		return 0;
	} else if("createAll" == command && "f" == typeTags && args.popFloat(value0).isOkNoMoreArgs())
	{
		printf("createAll %f\n", value0);
		createAllSensorsOnBus(value0, false);
		return 0;
	} else if ("deleteAll" == command && args.isOkNoMoreArgs()) {
		printf("deleteAll\n");
		gTouchSensors.clear();
		return 0;
	} else if ("autoReadAll" == command && args.isOkNoMoreArgs()) {
		printf("autoReadAll\n");
		for(auto& t : gTouchSensors)
			t.second.shouldRead = ALWAYS;
		return 0;
	} else if ("stopReadAll" == command && args.isOkNoMoreArgs()) {
		printf("stopReadAll");
		for(auto& t : gTouchSensors)
			t.second.shouldRead = DONT;
		return 0;
	} else if ("loopSleep" == command && "f" == typeTags && args.popFloat(value0).isOkNoMoreArgs()) {
		printf("loopSleep %f\n", value0);
		gLoopSleep = value0;
		return 0;
	}

	//instance commands: they all start with an id
	// check and retrieve first argument: id
	if(typeTags[0] != 's') {
		std::cerr << "Unknown message or wrong argument list " << msg << "\n";
		return -1;
	}
	std::string id;
	args.popStr(id);
	typeTags = typeTags.substr(1);// peel it off

	// only "new" can use a non-existing id
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
			printf("new %s %f %s %f\n", id.c_str(), bus, deviceName.c_str(), i2cAddr);
			newTrillDev(id, bus, Trill::getDeviceFromName(deviceName), i2cAddr, DONT);
			return 0;
		} else {
			std::cerr << "Unknown message or wrong argument list " << msg << "\n";
			return 1;
		}
	} else if ("delete" == command) {
		gTouchSensors.erase(id);
		printf("delete\n");
		return 0;
	}
	// commands below need to access a device. If we get to this line, the
	// device exists in gTouchSensors
	Trill& t = *(gTouchSensors[id].t);
	int ret;
	printf("id: %s - ", id.c_str());
	if ("autoRead" == command && "f" == typeTags && args.popFloat(value0).isOkNoMoreArgs()) {
		printf("autoRead: %f\n", value0);
		gTouchSensors[id].shouldRead = value0 ? ALWAYS : DONT;
	} else if ("readI2C" == command && args.isOkNoMoreArgs()) {
		printf("readI2C\n");
		gTouchSensors[id].shouldRead = ONCE;
	} // commands below simply map to the corresponding methods of the Trill class
	else if("updateBaseline" == command && args.isOkNoMoreArgs()) {
		printf("updateBaseline\n");
		ret = t.updateBaseline();
		sendOscReply(command, id, ret);
	} else if ("setPrescaler" == command && "f" == typeTags && args.popFloat(value0).isOkNoMoreArgs()) {
		printf("setPrescaler %f\n", value0);
		ret = t.setPrescaler(value0);
		sendOscReply(command, id, ret);
	} else if ("setNoiseThreshold" == command && "f" == typeTags && args.popFloat(value0).isOkNoMoreArgs()) {
		printf("setNoiseThreshold %f\n", value0);
		ret = t.setNoiseThreshold(value0);
		sendOscReply(command, id, ret);
	} else if ("setMode" == command && "s" == typeTags && args.popStr(str0).isOkNoMoreArgs()) {
		printf("setMode: %s\n", str0.c_str());
		ret = t.setMode(Trill::getModeFromName(str0));
		sendOscReply(command, id, ret);
	} else {
		std::cerr << "Unknown message or wrong argument list " << msg << "\n";
		return 1;
	}
	return 0;
}

int sendOsc(const oscpkt::Message& msg)
{
	oscpkt::PacketWriter pw;
	pw.addMessage(msg);
	bool ok = gSock.sendPacket(pw.packetData(), pw.packetSize());
	if(!ok) {
		fprintf(stderr, "could not send\n");
		return -1;
	}
	return 0;
}

int sendOscFloats(const std::string& address, float* values, unsigned int size)
{
	if(!size)
		return 0;
	oscpkt::Message msg(address);
	for(unsigned int n = 0; n < size; ++n)
		msg.pushFloat(values[n]);
	return sendOsc(msg);
}

std::string commandReplyAddress = "/trill/commandreply/";
int sendOscList()
{
	for(auto& d : gTouchSensors)
		sendOscTrillDev(d.first, d.second);
	return 0;
}

int sendOscTrillDev(const std::string& id, const TrillDev& trillDev)
{
	oscpkt::Message msg(commandReplyAddress);
	msg.pushStr(id);
	Trill& t = *trillDev.t;
	msg.pushStr(Trill::getNameFromDevice(t.deviceType()));
	msg.pushFloat(t.getAddress());
	msg.pushStr(Trill::getNameFromMode(t.getMode()));
	return sendOsc(msg);
}

int sendOscReply(const std::string& command, const std::string& id, int ret)
{
	oscpkt::Message msg(commandReplyAddress);
	msg.pushStr(id);
	return sendOsc(msg);
}
