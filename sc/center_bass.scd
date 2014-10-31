s.reboot;

(  // lib init
SynthDef("pnoise", { | outbus = 0, mul = 0.4 |
	Out.ar(outbus, PinkNoise.ar(mul));
}).add;
SynthDef("adev", { | outbus = 0, ch = 1, mul = 1 |
	Out.ar(outbus, AudioIn.ar(channel: ch, mul: mul));
}).add;
SynthDef("HiPan2", { |inbus = 0, outbus = 0|
	Out.ar(outbus, Mix.new([
		Pan2.ar(HPF.ar(In.ar(inbus), freq: 200), FSinOsc.kr(1),0.1),
		Pan2.ar(LPF.ar(In.ar(inbus), freq: 200), 0)
	]))
}).add;
a = Bus.audio(s);
// a.scope;
)

a.scope;

p = Synth.new("HiPan2", [\inbus, a]);
n = Synth.before(p, "pnoise", [\outbus, a]);
n.free;
m = Synth.before(p, "adev", [\outbus, a])
m.set(\mul, 2);
m.free;

// (
// SynthDef("help-Pan2", { Out.ar(0, Pan2.ar(HPF.ar(PinkNoise.ar(0.4), freq: 200), FSinOsc.kr(1), 0.3)) }).play;
// SynthDef("LowPan2", {Out.ar(0, Pan2.ar(LPF.ar(PinkNoise.ar(0.4), freq: 200), 0, 1))}).play;
// )
//
// SynthDef("help-Pan2", { Out.ar(0, Pan2.ar(PinkNoise.ar(0.4), FSinOsc.kr(1), 0.3)) }).play;
//

{Out.ar(0, Dust.ar(MouseX.kr(1,4000)))}.play


{ WhiteNoise.ar(mul:0.01) }.play

{ AllpassC.ar(Decay.ar(Dust.ar(1,0.5), 0.2, WhiteNoise.ar), 0.2, 0.2, 3) }.play;









