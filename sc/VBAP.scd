//Server.default = s = Server.internal;
//s.reboot;

(
// init
s.waitForBoot{
	var speakernum = 4;
	// 2D ring of speakers
	a = VBAPSpeakerArray.new(2, Array.fill(speakernum, { arg i; i * 360/speakernum - 180}));
	a.speakers[1].dump;
	b = a.loadToBuffer;
	// VBAP on first audio input
	x = { |azi = 0, ele = 0, spr = 20, vol = 1|
		VBAP.ar(4, AudioIn.ar(1, mul:vol), b.bufnum, azi, ele, spr);
	}.play;
	// OSC azimuth
	OSCFunc.newMatching(
		{|msg, time, addr, recvPort| msg[1].postln; x.set(\azi, msg[1].asFloat);},
		'/azi',
		recvPort: 3030);
	// OSC spread
	OSCFunc.newMatching(
		{|msg, time, addr, recvPort| msg[1].postln; x.set(\spr, msg[1].asFloat);},
		'/spr',
		recvPort: 3030);
	// OSC volume
	OSCFunc.newMatching(
		{|msg, time, addr, recvPort| msg[1].postln; x.set(\vol, msg[1].asFloat);},
		'/dist',  // from 0 to 100
		recvPort: 3030);
};
)


// for i in `seq 180`; do oscsend localhost 3030 /azi i $i; sleep 1; done
// m = NetAddr("127.0.0.1", 3030); // loopback
// m.sendMsg("/azi", "180");
// m.sendMsg("/azi", "90");
// m.sendMsg("/vol", "0.2");
// m.sendMsg("/vol", "1");

