#include <stdio.h>
#include <string.h>
#include <unordered_map>
#include "daisy_seed.h"
#include "daisysp.h"

using namespace daisy;
using namespace daisysp;

#define MAX_DELAY static_cast<size_t>(48000 * 1.0f)
#define MAX_SIZE (48000 * 60 * 5) // 5 minutes of floats at 48 khz

#define CHORUS 0
#define OVERDRIVE 1
#define EQ_QUESTION_MARK 2
#define FLANGER 3 // maybe too redundant
#define DELAY 4
#define AUTOWAH 5
#define TREMOLO 6
#define DRUMS 7


float DSY_SDRAM_BSS buf[MAX_SIZE];

DaisySeed hw;
Looper looper;
Chorus chorus;
Overdrive overdrive;
Flanger flanger;
Autowah autowah; // done!
DelayLine<float, MAX_DELAY> delay;
Svf low_pass; // ?
Metro metro;
Tremolo tremolo; // add waves
AnalogBassDrum drum;

Oscillator      led_osc2; // For pulsing the led when in effects-only mode
float           ledBrightness2;

Led led1;
Led led2;
Switch sw1;
Switch sw2;
Switch sw3;
Switch sw4;
Switch fs1;
Switch fs2;

float p1, p2, p3, p4, p5, p6;
float q1, q2, q3, q4, q5, q6;

std::unordered_map<int, bool> effects;
int activeEffect;
int oldActiveEffect = 0;

void updateLed1()
{
	if (effects[activeEffect])
	{
		led1.Set(1.0f);
	}
	else
	{
		led1.Set(0.0f);
	}
	led1.Update();
}

void updateActiveEffect()
{
	// my switches are backwards, so I'm swapping the true/false to get the orientation i want
	activeEffect = (!sw1.RawState() << 2) | (!sw2.RawState() << 1) | !sw3.RawState();
	
	// check if the effect has changed, if so we need to "freeze the pots"
	if (oldActiveEffect != activeEffect) {
		q1 = p1;
		q2 = p2;
		q3 = p3;
		q4 = p4;
		q5 = p5;
		q6 = p6;
		oldActiveEffect = activeEffect;
	}
	
	updateLed1();
}

void initFirstEffect()
{
	switch (activeEffect) {
		case CHORUS:
			chorus.SetLfoDepth(p1);
			chorus.SetLfoFreq(p2 * 2.0f);
			chorus.SetDelay(p3);
			break;
		
		case OVERDRIVE:
			overdrive.SetDrive(p1);
			break;

		case FLANGER:
			flanger.SetLfoDepth(p1);
			flanger.SetLfoFreq(p2);
			break;

		case DELAY:
			delay.SetDelay(p1);
			break;

		case AUTOWAH:
			autowah.SetWah(p1);
			autowah.SetDryWet(p2 * 100.0f);
			autowah.SetLevel(p3);
			break;

		case TREMOLO:
			tremolo.SetFreq(p1 * 5.0f);
			tremolo.SetDepth(p2);
			break;
	}
}

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size)
{
	// check the switches (1-3) so we know what effect is selected
	updateActiveEffect();

	// check switch #4 to adjust the looper mode
	if (sw4.RawState()) { // up
		looper.SetMode(Looper::Mode::ONETIME_DUB);
	} else { // down
		looper.SetMode(Looper::Mode::NORMAL);
	}

	// get current pot values
	p1 = hw.adc.GetFloat(0);
	p2 = hw.adc.GetFloat(1);
	p3 = hw.adc.GetFloat(2);
	p4 = hw.adc.GetFloat(3);
	p5 = hw.adc.GetFloat(4);
	p6 = hw.adc.GetFloat(5); // looper gain

	fs1.Debounce();
	fs2.Debounce();

	// toggle the active effect when pressed
	if (fs1.RisingEdge())
    {
		effects[activeEffect] = !effects[activeEffect];
		updateLed1();
	}

	if (fs2.RisingEdge())
    {
		looper.TrigRecord();
		if (!looper.Recording()) // Turn on LED if not recording and in playback
		{
			led2.Set(1.0f);
			led2.Update();
		}
    }

	if (fs2.TimeHeldMs() >= 1000)
	{
		looper.Clear();
		led2.Set(0.0f);
		led2.Update();
	}

	// update effect parameters by checking the values of the pots
	switch (activeEffect) {
		case CHORUS:
			if (q1 != p1) chorus.SetLfoDepth(p1);
			if (q2 != p2) chorus.SetLfoFreq(p2 * 2.0f);
			if (q3 != p3) chorus.SetDelay(p3);
			break;
		
		case OVERDRIVE:
			if (q1 != p1) overdrive.SetDrive(p1);
			break;

		case FLANGER:
			flanger.SetLfoDepth(p1);
			flanger.SetLfoFreq(p2);
			break;

		case DELAY:
			delay.SetDelay(p1);
			break;

		case AUTOWAH:
			autowah.SetWah(p1);
			autowah.SetDryWet(p2 * 100.0f);
			autowah.SetLevel(p3);
			break;

		case TREMOLO:
			tremolo.SetFreq(p1 * 5.0f);
			tremolo.SetDepth(p2);
			break;
	}

	float sig, delay_out, sig_out, feedback;
	for (size_t i = 0; i < size; i++)
	{
        ledBrightness2 = led_osc2.Process();
		sig = in[0][i];

		if (effects[CHORUS])
		{
			chorus.Process(sig);
			sig = chorus.GetLeft();
		}

		if (effects[OVERDRIVE])
		{
			sig = overdrive.Process(sig);
		}

		if (effects[FLANGER])
		{
			sig = flanger.Process(sig);
		}

		if (effects[AUTOWAH])
		{
			sig = autowah.Process(sig);
		}

		if (effects[DELAY])
		{
			delay_out = delay.Read();
			sig_out = sig + delay_out;
			feedback = (delay_out * 0.1) + sig;
			delay.Write(feedback);
			sig = sig_out;
		}

		if (effects[TREMOLO])
		{
			sig = tremolo.Process(sig);
		}
		
		float loop_out = 0.0;
		loop_out = looper.Process(sig);
		
		out[0][i] = (loop_out * p6) + sig;
	}

	if (looper.Recording())
	{
        led2.Set(ledBrightness2 * 0.5 + 0.1);
		led2.Update();
    } 
}

// p1 - fx1
// p2 - fx2
// p3 - fx3
// p4 - fx gain?
// p5 - 
// p6 - looper gain

// EQ? like a bass boost?

// 	// metro.SetFreq(p6 * 3.0f);
// 	// drum.SetTone(p4);
// 	// drum.SetDecay(p5);
// 	// bool t = metro.Process();
// 	// sig += drum.Process(t);

int main(void)
{
	hw.Init();
	hw.SetAudioBlockSize(4);
	hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);
	hw.SetLed(false); // turn off onboard LED
	float sample_rate = hw.AudioSampleRate();

	effects = {
		{CHORUS, false},
		{OVERDRIVE, false},
		{2, false},
		{FLANGER, false},
		{DELAY, false},
		{AUTOWAH, false},
		{TREMOLO, false},
		{7, false},
	};

	led_osc2.Init(sample_rate);
    led_osc2.SetFreq(1.5);
    led_osc2.SetWaveform(1);
    ledBrightness2 = 0.0;

	AdcChannelConfig adcConfig[6];
	adcConfig[0].InitSingle(hw.GetPin(16)); // physical pin 23
	adcConfig[1].InitSingle(hw.GetPin(17)); // physical pin 24
	adcConfig[2].InitSingle(hw.GetPin(18)); // physical pin 25
	adcConfig[3].InitSingle(hw.GetPin(19)); // physical pin 26
	adcConfig[4].InitSingle(hw.GetPin(20)); // physical pin 27
	adcConfig[5].InitSingle(hw.GetPin(21)); // physical pin 28
	led1.Init(hw.GetPin(22), false); // physical pin 29
	led2.Init(hw.GetPin(23), false); // physical pin 30
	fs1.Init(hw.GetPin(25), 0.f); // physical pin 32
	fs2.Init(hw.GetPin(26), 0.f); // physical pin 33
	sw1.Init(hw.GetPin(10), 0.f, daisy::Switch::TYPE_TOGGLE, daisy::Switch::POLARITY_NORMAL, daisy::Switch::PULL_UP); // physical pin 11
	sw2.Init(hw.GetPin(9), 0.f, daisy::Switch::TYPE_TOGGLE, daisy::Switch::POLARITY_NORMAL, daisy::Switch::PULL_UP); // physical pin 10
	sw3.Init(hw.GetPin(8), 0.f, daisy::Switch::TYPE_TOGGLE, daisy::Switch::POLARITY_NORMAL, daisy::Switch::PULL_UP); // physical pin 9
	sw4.Init(hw.GetPin(7), 0.f, daisy::Switch::TYPE_TOGGLE, daisy::Switch::POLARITY_NORMAL, daisy::Switch::PULL_UP); // physical pin 8

	chorus.Init(sample_rate);
    chorus.SetLfoFreq(0.0f);
    chorus.SetLfoDepth(0.f);
    chorus.SetDelay(0.f);

	flanger.Init(sample_rate);

	overdrive.Init();
	overdrive.SetDrive(0.0f);

	delay.SetDelay(sample_rate * 1.0f);

	metro.Init(1, sample_rate);

	tremolo.Init(sample_rate);

	autowah.Init(sample_rate);

	drum.Init(sample_rate);
    drum.SetFreq(75.f);
	drum.SetTone(0.5f);
	drum.SetDecay(0.5f);

    looper.Init(buf, MAX_SIZE);
    looper.SetMode(Looper::Mode::NORMAL);

    hw.adc.Init(adcConfig, 6);
    hw.adc.Start();

	// get starting values for the pots
	p1 = hw.adc.GetFloat(0);
	p2 = hw.adc.GetFloat(1);
	p3 = hw.adc.GetFloat(2);
	p4 = hw.adc.GetFloat(3);
	p5 = hw.adc.GetFloat(4);
	p6 = hw.adc.GetFloat(5); // looper gain

	// find the active effect
	updateActiveEffect();

	// update the parameters for the active effect
	initFirstEffect();

	hw.StartAudio(AudioCallback);
}

/*

TODO
 - turn off onboard LED
 - effects
 - looper
 - storage?
 - save params between effects (how to?)

*/