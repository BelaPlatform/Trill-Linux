var dgram = require('dgram');
var osc = require('osc-min');
// Default IPs and ports
var localPort = 7563;
var remoteIp = '127.0.0.1';
var remotePort = 7562;

// hack to detect if we are running in the REPL or not
// https://stackoverflow.com/questions/56403255/determine-if-javascript-nodejs-code-is-running-in-a-repl
// not 100% accurate, e.g: for `node -i $($< ./FILE.js )`, it retunrs false
var repl = false;
try {
	__dirname;
} catch (e) {
	repl = true;
}

var args = process.argv;
if(args.length > 2){
	if(args[2] == 'help'){
		console.log([
		"Usage: ",
		"`node trill-osc <remoteIp> <remotePort> <localPort>`",
		"All parameters are optional, defaults to:",
		"`node trill-osc "+remoteIp+" "+remotePort+" "+localPort+"`"
		].join("\n"));
		exit();
	}
	remoteIp = args[2];
	if(args.length > 3)
		remotePort = args[3];
	if(args.length > 4)
		localPort = args[4];
}
function help()
{
//"HEREDOC" https://stackoverflow.com/a/56031250/2958741
var str = (function(){/**
A utility to talk to the trill-osc example.
Send commands with
	sendTrillCommand("COMMAND", args ...)

All commands are sent to "/trill/commands/<command>" with 0 or more arguments.

Global commands:
list all enabled devices:
	sendTrillCommand("listAll")
discover and create all Trill devices on the specified `i2cBus`:
	sendTrillCommand("createAll", i2cBus)
delete all active Trill devices:
	sendTrillCommand("deleteAll")
set all devices to read (and send) new data automatically:
	sendTrillCommand("autoReadAll)
disable automatic reading for all devices:
	sendTrillCommand("stopReadAll")
change the scanning rate so that there is a `ms` sleep in between reads (and sends):
	sendTrillCommand("loopSleep", ms)

Instance commands: they all start with a string id

create Trill device on the specified I2C `busNumber`, of the specified
`deviceType`, at the specified `i2cAddress` (optional).
`deviceType` is a string representing the name of the device (e.g.: "bar", "square", etc).
Use `deviceType = "unknown"` for accepting any type, but then you have to
specify a valid value for `i2cAddress`.
	sendTrillCommand("new", id, i2cBus, deviceType, i2cAddress)
delete exising Trill device
	sendTrillCommand("delete", id)
set device to read (and send) new data automatically (if `shouldDo`),
or not (if 0 == shouldDo)
	sendTrillCommand("autoRead", id, shouldDo)
ask the device to read (and send) data once
	sendTrillCommand("readI2C", id)

More instance commands, which map directly to the C++ API http://docs.bela.io/classTrill.html
	sendTrillCommand("updateBaseline", id)
	sendTrillCommand("setPrescaler", id, value)
	sendTrillCommand("setNoiseThreshold", id, value)
	sendTrillCommand("setMode", id, mode) // mode is a string: "centroid", "raw", "baseline",  or "diff"

Incoming messages will be at:
/trill/commandreply << for replies to commands

Readings:
1D devices in centroid mode:
	/trill/readings/<id>/touches <num-touches> <loc0> <pos0> <loc1> <pos1> ...
2D devices in centroid mode (compoundTouch):
	/trill/readings/<id>/touchXY <num-touch> <loc0> <pos0>
Devices in raw/baseline/diff mode:
	/trill/readings/<id>/raw <numChannels> <c0> <c1> ...
	/trill/readings/<id>/baseline <numChannels> <c0> <c1> ...
	/trill/readings/<id>/diff <numChannels> <c0> <c1> ...

Disable print readings:
set the global `logReadings` variable to true (default)  or false if you want to
	disable/enable printing the readings to the node console.

use help() to print this message again

**/}).toString().slice(15,-5);
	console.log(str);
}

console.log("send to: "+remoteIp+":"+remotePort+", receive on: :"+localPort)

// socket to send and receive OSC messages from bela
var socket = dgram.createSocket('udp4');
socket.bind(localPort);
		
socket.on('message', (message, info) => {

	var msg = osc.fromBuffer(message);
	
	if (msg.oscType === 'message'){
		parseMessage(msg);
	} else if (msg.elements){
		for (let element of msg.elements){
			parseMessage(element);
		}
	}
	
});

var logReadings = 1;
var readingsAddressReg = /^\/trill\/readings.*/
function parseMessage(msg){

	if(msg.address.match(readingsAddressReg) && !logReadings)
		return;
	var address = msg.address.split('/');
	if (!address || !address.length || address.length <2){
		console.log('bad OSC address', address);
		return;
	}
	
	console.log('address:', msg.address);
	for (let arg of msg.args){
		console.log(arg);
		if (arg.type === 'blob'){
			floats = [];
			for (let i = 0; i< arg.value.length; i += 4){
				floats.push(arg.value.readFloatLE(i).toPrecision(5));
			}
			console.log(floats);
		}
	}
}

function sendTrillCommand(command /*, arguments */){
	var msg = ({
		address : '/trill/command/' + command,
		args 	: [ ],
	});
	var args = arguments;
	// start from n = 1 : skip first argument `command`
	for (let n = 1; n < args.length; ++n) {
		let type;
		let arg = args[n];
		switch (typeof(arg))
		{
			case "string":
				type = 'string';
				break;
			case "number":
				type = 'float';
				break;
			default:
				console.log("Unknown type", typeof(arg));
		}
		if(type)
			msg.args.push({type: type, value: arg});
	}
	//console.log("sending: ", msg);
	var buffer = osc.toBuffer(msg);
	socket.send(buffer, 0, buffer.length, remotePort, remoteIp, function(err) {
		if(err)
			console.log("Error wil sending ", buffer);
	});
}


// command line interface
help();
if(repl) {

} else {
	cli();
}

function cli() {
	const readline = require('readline');

	const rl = readline.createInterface({
		input: process.stdin,
		output: process.stdout
	});
	console.log("This is an interactive console where your js is evaluated, but not a full REPL.\n"+
		"You may consider running this script interactively in the node REPL (e.g.: running:\n"+
		"`.load ./trill-osc.js` inside node.");

	function lineCb(line) {
		var str = line.trim();
		if(str === "help")
			help();
		else {
			try {
				eval(str);
			} catch (e) {
				console.log(e);
			}
		}
		process.stdout.write(">> ");
		rl.question('>> ', lineCb);
	}
	rl.question('>> ', lineCb);
}
