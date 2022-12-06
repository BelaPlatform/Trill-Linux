/*
 ____  _____ _        _
| __ )| ____| |      / \
|  _ \|  _| | |     / _ \
| |_) | |___| |___ / ___ \
|____/|_____|_____/_/   \_\
http://bela.io
*/

const char* helpText =
"An OSC client to manage Trill devices."
"  Usage: %s [--port <inPort>] [[--auto <bus> ] <remote>]\n"
"  --port <inPort> :  set the port where to listen for OSC messages\n"
"\n"
"  `--auto <bus> <remote> : this is useful for debugging: automatically detect\n"
"                          all the Trill devices on <bus> (corresponding to /dev/i2c-<bus>)\n"
"                          and start reaading from them\n"
"                          All messages read will be sent to the <remote> IP:port (default: %s)\n"
"======================\n"
"\n"
"NOTE: when `--auto` is used, or a `createAll` command is received, the program\n"
"scans several addresses on the i2c bus, which could cause non-Trill\n"
"peripherals connected to it to malfunction.\n"
"\n"
"Command reference:\n"
"Send commands to `/trill/commands/<command>`, with 0 or more arguments.\n"
"\n"
"Global commands:\n"
"list all enabled devices:\n"
"	/trill/commands/listAll\n"
"discover and create all Trill devices on the specified `i2cBus`:\n"
"	/trill/commands/createAll <float>i2cBus\n"
"delete all active Trill devices:\n"
"	/trill/commands/deleteAll\n"
"set whether all devices `should` read (and send) new data automatically or not:\n"
"	/trill/commands/autoReadAll <float>should \n"
"change the scanning rate so that there is a `ms` sleep in between reads (and sends):\n"
"	/trill/commands/loopSleep, ms\n"
"\n"
"Instance commands: they all start with a string id\n"
"\n"
"create Trill device on the specified I2C `busNumber`, of the specified\n"
"`deviceType`, at the specified `i2cAddress` (optional).\n"
"`deviceType` is a string representing the name of the device (e.g.: bar, square, etc).\n"
"Use `deviceType = unknown` for accepting any type, but then you have to\n"
"specify a valid value for `i2cAddress`.\n"
"	/trill/commands/new, <string>id <float>i2cBus <string>deviceType <float>i2cAddress\n"
"delete exising Trill device\n"
"	/trill/commands/delete <string>id\n"
"set whether device `should` read (and send) new data automatically (or not)\n"
"	/trill/commands/autoRead <string>id <float>should\n"
"ask the device to read (and send) data once\n"
"	/trill/commands/readI2C, <string>id\n"
"\n"
"More instance commands, which map directly to the C++ API http://docs.bela.io/classTrill.html\n"
"	/trill/commands/updateBaseline <string>id\n"
"	/trill/commands/setMode <string>id <string>mode // mode is a string: centroid, raw, baseline,  or diff\n"
"	/trill/commands/setScanSettings <string>id <float>speed <float>num_bits\n"
"	/trill/commands/setPrescaler <string>id <float>value\n"
"	/trill/commands/setNoiseThreshold <string>id <float>value\n"
"\n"
"Outbound messages:\n"
"messages in response to commands will be sent to:\n"
"	/trill/commandreply\n"
"\n"
"Readings:\n"
"1D devices in centroid mode:\n"
"	/trill/readings/<id>/touches <num-touches> <loc0> <pos0> <loc1> <pos1> ...\n"
"2D devices in centroid mode (compoundTouch):\n"
"	/trill/readings/<id>/touchXY <num-touch> <loc0> <pos0>\n"
"Devices in raw/baseline/diff mode:\n"
"	/trill/readings/<id>/raw <numChannels> <c0> <c1> ...\n"
"	/trill/readings/<id>/baseline <numChannels> <c0> <c1> ...\n"
"	/trill/readings/<id>/diff <numChannels> <c0> <c1> ...\n"
;

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
bool gAutoReadAll = 0;
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
int sendOscTrillDev(const std::string& command, const std::string& id, const TrillDev& trillDev);
int sendOscReply(const std::string& command, const std::string& id, int ret);
std::vector<std::string> split(const std::string& s, char delimiter);

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
	sendOscTrillDev("new", id, gTouchSensors[id]);
	return 0;
}

void createAllDevicesOnBus(unsigned int i2cBus, bool autoRead) {
	printf("Trill devices detected on bus %d\n", i2cBus);
	for(uint8_t addr = 0x20; addr <= 0x50; ++addr)
	{
		Trill::Device device = Trill::probe(i2cBus, addr);
		if(Trill::NONE != device)
		{
			std::string id = std::to_string(i2cBus) + "-" + std::to_string(addr) + "-" + Trill::getNameFromDevice(device);
			ShouldRead shouldRead = autoRead ? ALWAYS : DONT;
			newTrillDev(id, i2cBus, device, addr, shouldRead);
		}
	}
}

static std::string baseAddress = "/trill/";
int main(int argc, char** argv)
{
	int i2cBus = -1;
	unsigned int inPort = 7562;
	std::string remote = "localhost:7563";

	int c = 1;
	while(c < argc)
	{
		if(std::string("--help") == std::string(argv[c])) {
			printf(helpText, argv[0], remote.c_str());
			return 0;
		}
		if(std::string("--port") == std::string(argv[c])) {
			++c;
			if(c < argc) {
				inPort = atoi(argv[c]);
				if(!inPort) {
					fprintf(stderr, "Invalid port: %s\n", argv[c]);
				}
			}
		}
		if(std::string("--auto") == std::string(argv[c])) {
			++c;
			if(c < argc) {
				i2cBus = atoi(argv[c]);
				++c;
			}
			if(c < argc) {
				remote = argv[c];
				++c;
			}
			std::vector<std::string> spl = split(remote, ':');
			if(2 != spl.size()) {
				fprintf(stderr, "Wrong or unparseable `IP:port` argument: %s\n", remote.c_str());
				return 1;
			}
			gSock.connectTo(spl[0], std::stoi(spl[1]));
			std::cout << "Detecting all devices on bus " << i2cBus << ", sending to " << remote << "\n(remote address will be reset when the first inbound message is received)\n";
			gAutoReadAll = true;
			createAllDevicesOnBus(i2cBus, gAutoReadAll);
		}
		++c;
	}
	std::cout << "Listening on port " << inPort << "\n";

	signal(SIGINT, interrupt_handler);
	gSock.bindTo(inPort);

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
			static bool connected = false;
			if(!connected) {
				std::vector<std::string> origin = split(gSock.packetOrigin().asString(), ':');
				if(origin.size() != 2) {
					fprintf(stderr, "Something wrong with the address we received from\n");
					continue;
				}
				std::cout << "Connecting to " << origin[0] << ":" << origin[1] << "\n";
				gSock.connectTo(origin[0], origin[1]);
				gSock.bindTo(inPort);
				connected = true;
			}

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
	oscpkt::Message::ArgReader args = msg.partialMatch(baseAddress + "command/");
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
		createAllDevicesOnBus(value0, gAutoReadAll);
		return 0;
	} else if ("deleteAll" == command && args.isOkNoMoreArgs()) {
		printf("deleteAll\n");
		gTouchSensors.clear();
		return 0;
	} else if ("autoReadAll" == command && "f" == typeTags && args.popFloat(value0).isOkNoMoreArgs()) {
		printf("autoReadAll %f\n", value0);
		gAutoReadAll = value0;
		for(auto& t : gTouchSensors)
			t.second.shouldRead = gAutoReadAll ? ALWAYS : DONT;
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
			newTrillDev(id, bus, Trill::getDeviceFromName(deviceName), i2cAddr, gAutoReadAll ? ALWAYS : DONT);
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
	} else if ("setScanSettings" == command && "ff" == typeTags && args.popFloat(value0).popFloat(value1).isOkNoMoreArgs()) {
		printf("setScanSettings %f %f\n", value0, value1);
		ret = t.setScanSettings(value0, value1);
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

std::string commandReplyAddress = baseAddress + "commandreply/";
int sendOscList()
{
	for(auto& d : gTouchSensors)
		sendOscTrillDev("list", d.first, d.second);
	return 0;
}

int sendOscTrillDev(const std::string& command, const std::string& id, const TrillDev& trillDev)
{
	oscpkt::Message msg(commandReplyAddress + "/" + command);
	msg.pushStr(id);
	Trill& t = *trillDev.t;
	msg.pushStr(Trill::getNameFromDevice(t.deviceType()));
	msg.pushFloat(t.getAddress());
	msg.pushStr(Trill::getNameFromMode(t.getMode()));
	return sendOsc(msg);
}

int sendOscReply(const std::string& command, const std::string& id, int ret)
{
	oscpkt::Message msg(commandReplyAddress + command);
	msg.pushStr(id);
	msg.pushFloat(ret);
	return sendOsc(msg);
}
