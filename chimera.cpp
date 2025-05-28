#include <stdio.h>
#include <string.h>
#include <unordered_map>
#include "daisy_seed.h"
#include "daisysp.h"

using namespace daisy;
using namespace daisysp;

#define MAX_DELAY static_cast<size_t>(48000 * 1.0f)
#define MAX_SIZE (48000 * 60 * 5) // 5 minutes of floats at 48 khz
float DSY_SDRAM_BSS buf[MAX_SIZE];

#define CHORUS 0      // 	DONE
#define OVERDRIVE 1   //    DONE, kinda finicky though
#define DELAY 2       // needs complete redo!
#define TREMOLO 3     // 	DONE, maybe could be tuned though
#define FLANGER 4     // 	DONE
#define AUTOWAH 5     // 	DONE
#define EQUALIZER 6   // test this?
#define DRUMS 7       // test tone ane metro

DaisySeed hw;
Led led1;
Led led2;
Switch fs1;
Switch fs2;
Switch sw1;
Switch sw2;
Switch sw3;
Switch sw4;

Looper looper;
Chorus chorus;
Overdrive overdrive;
DelayLine<float, MAX_DELAY> delay;
Tremolo tremolo;
Flanger flanger;
Autowah autowah;
Svf eq1;
Svf eq2;
Svf eq3;
Svf eq4;
Metro metro;
AnalogBassDrum drum;

float p1, p2, p3, p4, p5, p6;
float q1, q2, q3, q4, q5, q6;
float pq_wiggle = 0.05f;
float eq1_boost = 1.0f;
float eq2_boost = 1.0f;
float eq3_boost = 1.0f;
float eq4_boost = 1.0f;

std::unordered_map<int, std::pair<bool, float>> effects;
int activeEffect;
int oldActiveEffect = 0;

bool checkDoubleTap = false;
int doubleTapCounter = 0;

void updateLed1()
{
	if (effects[activeEffect].first)
		led1.Set(1.0f);
	else
		led1.Set(0.0f);
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

// TODO! this needs to match all the other stuff
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
			if (p3 < 0.25f) tremolo.SetWaveform(0); // SINE
			else if (p3 < 0.5f) tremolo.SetWaveform(4); // SQUARE
			else if (p3 < 0.75f) tremolo.SetWaveform(3); // RAMP
			else tremolo.SetWaveform(2); // SAW
			break;
	}
}

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size)
{
	// check the switches (1-3) so we know what effect is selected
	updateActiveEffect();

	// check switch #4 to adjust the looper mode
	if (sw4.RawState()) // up
		looper.SetMode(Looper::Mode::ONETIME_DUB);
	else // down
		looper.SetMode(Looper::Mode::NORMAL);

	// get current pot values
	p1 = hw.adc.GetFloat(0); // effect param 1
	p2 = hw.adc.GetFloat(1); // effect param 2
	p3 = hw.adc.GetFloat(2); // effect param 3
	p4 = hw.adc.GetFloat(3); // gain for the effects (usually)
	p5 = hw.adc.GetFloat(4); // metro tempo (used for DRUMS)
	p6 = hw.adc.GetFloat(5); // looper gain

	fs1.Debounce();
	fs2.Debounce();

	// toggle the active effect when pressed
	if (fs1.RisingEdge())
    {
		effects[activeEffect].first = !effects[activeEffect].first;
		updateLed1();
	}

	// looper logic
	if (fs2.RisingEdge())
    {
		looper.TrigRecord();
		if (!looper.Recording()) // Turn on LED if not recording and in playback
		{
			led2.Set(1.0f);
			led2.Update();
		}

		if (checkDoubleTap) {
			if (doubleTapCounter <= 1000) {
				if (looper.Recording()) {
					looper.TrigRecord();
				}
				doubleTapCounter = 0;
				checkDoubleTap = false;
			}
		} else {
			checkDoubleTap = true;
		}
    }

	if (checkDoubleTap) {
		doubleTapCounter += 1;
		if (doubleTapCounter > 1000) {
			doubleTapCounter = 0;
			checkDoubleTap = false;
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
			if (fabs(q1 - p1) > pq_wiggle) chorus.SetLfoDepth(p1);
			if (fabs(q2 - p2) > pq_wiggle) chorus.SetLfoFreq(p2 * 2.0f);
			if (fabs(q3 - p3) > pq_wiggle) chorus.SetDelay(p3);
			if (fabs(q4 - p4) > pq_wiggle) effects[CHORUS].second = (p4 * 1.5f) + 0.5f;
			break;

		case OVERDRIVE:
			if (fabs(q1 - p1) > pq_wiggle) overdrive.SetDrive(p1);
			if (fabs(q4 - p4) > pq_wiggle) effects[OVERDRIVE].second = (p4 * 1.5f) + 0.5f;
			break;

		case FLANGER:
			if (fabs(q1 - p1) > pq_wiggle) flanger.SetLfoDepth(p1);
			if (fabs(q2 - p2) > pq_wiggle) flanger.SetLfoFreq(p2);
			if (fabs(q4 - p4) > pq_wiggle) effects[FLANGER].second = (p4 * 1.5f) + 0.5f;
			break;

		case DELAY:
			delay.SetDelay(p1);
			break;

		case AUTOWAH:
			if (fabs(q1 - p1) > pq_wiggle) autowah.SetWah(p1);
			if (fabs(q2 - p2) > pq_wiggle) autowah.SetDryWet(p2 * 100.0f);
			if (fabs(q3 - p3) > pq_wiggle) autowah.SetLevel(p3);
			if (fabs(q4 - p4) > pq_wiggle) effects[AUTOWAH].second = (p4 * 1.5f) + 0.5f;
			break;

		case TREMOLO:
			if (fabs(q1 - p1) > pq_wiggle) tremolo.SetFreq(p1 * 5.0f);
			if (fabs(q2 - p2) > pq_wiggle) tremolo.SetDepth(p2);
			if (fabs(q3 - p3) > pq_wiggle) {
				if (p3 < 0.33f) tremolo.SetWaveform(0); // SINE
				else if (p3 < 0.67) tremolo.SetWaveform(4); // SQUARE
				else tremolo.SetWaveform(2); // SAW
			}
			if (fabs(q4 - p4) > pq_wiggle) effects[TREMOLO].second = (p4 * 1.5f) + 0.5f;
			break;

		case EQUALIZER:
			if (fabs(q1 - p1) > pq_wiggle) eq1_boost = p1 + 0.5f;
			if (fabs(q2 - p2) > pq_wiggle) eq2_boost = p2 + 0.5f;
			if (fabs(q3 - p3) > pq_wiggle) eq3_boost = p3 + 0.5f;
			if (fabs(q4 - p4) > pq_wiggle) eq4_boost = p4 + 0.5f;
			break;

		case DRUMS:
			if (fabs(q1 - p1) > pq_wiggle) drum.SetFreq((75.0f * p1) + 25.0f);
			if (fabs(q2 - p2) > pq_wiggle) drum.SetTone(p2);
			if (fabs(q3 - p3) > pq_wiggle) drum.SetDecay(p3);
			if (fabs(q4 - p4) > pq_wiggle) effects[DRUMS].second = (p4 * 1.5f) + 0.5f;
			break;
		
	}

	// metro tempo can be changed at anytime
	// (p5 is between 0-1, which maps to 1-3hz, which is equals to 60-180bpm)
	metro.SetFreq((2.0f * p5) + 1.0f);

	bool metro_tick;
	float sig, delay_out, sig_out, feedback;
	for (size_t i = 0; i < size; i++)
	{
		sig = in[0][i];

		if (effects[CHORUS].first)
		{
			chorus.Process(sig *= effects[CHORUS].second);
			sig = chorus.GetLeft();
		}

		if (effects[OVERDRIVE].first)
		{
			sig = overdrive.Process(sig  *= effects[OVERDRIVE].second);
		}

		if (effects[DELAY].first)
		{
			delay_out = delay.Read();
			sig_out = sig + delay_out;
			feedback = (delay_out * 0.1) + sig;
			delay.Write(feedback);
			sig = sig_out;
		}
		
		if (effects[FLANGER].first)
		{
			sig = flanger.Process(sig *= effects[FLANGER].second);
		}

		if (effects[AUTOWAH].first)
		{
			sig = autowah.Process(sig);
		}

		if (effects[TREMOLO].first)
		{
			sig = tremolo.Process(sig);
			sig *= effects[TREMOLO].second;
		}

		if (effects[EQUALIZER].first)
		{
			eq1.Process(sig);
			eq2.Process(sig);
			eq3.Process(sig);
			eq4.Process(sig);
			sig = (eq1.Band() * eq1_boost) + (eq2.Band() * eq2_boost) + (eq3.Band() * eq3_boost) + (eq4.Band() * eq4_boost);
		}

		if (effects[DRUMS].first)
		{
			metro_tick = metro.Process();
			sig += (drum.Process(metro_tick) * effects[DRUMS].second);
		}
		
		float loop_out = 0.0;
		loop_out = looper.Process(sig);
		
		out[0][i] = (loop_out * (p6 + 0.5f)) + sig;
	}
}

int main(void)
{
	hw.Init();
	hw.SetAudioBlockSize(4);
	hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);
	hw.SetLed(false); // this doesn't seem to work?
	float sample_rate = hw.AudioSampleRate();

	// TODO switch up the language, active means turned on,
	// need some other word for selected effect

	// {KEY: {is_active?, fx_gain}}
	effects = {
		{CHORUS, {false, 1.0f}},
		{OVERDRIVE, {false, 1.0f}},
		{DELAY, {false, 1.0f}},
		{TREMOLO, {false, 1.0f}},
		{FLANGER, {false, 1.0f}},
		{AUTOWAH, {false, 1.0f}},
		{EQUALIZER, {false, 1.0f}},
		{DRUMS, {false, 1.0f}},
	};

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
    // chorus.SetLfoFreq(0.0f);
    // chorus.SetLfoDepth(0.f);
    // chorus.SetDelay(0.f);
	
	overdrive.Init();
	// overdrive.SetDrive(0.0f);
	
	delay.SetDelay(sample_rate * 1.0f);
	
	tremolo.Init(sample_rate);
	flanger.Init(sample_rate);
	autowah.Init(sample_rate);
	
	eq1.Init(sample_rate);
	eq2.Init(sample_rate);
	eq3.Init(sample_rate);
	eq4.Init(sample_rate);
	eq1.SetFreq(200.0f);
	eq2.SetFreq(500.0f);
	eq3.SetFreq(2000.0f);
	eq4.SetFreq(5000.0f);

	metro.Init(1, sample_rate);
	drum.Init(sample_rate);
    // drum.SetFreq(50.f);
	// drum.SetTone(0.0f);
	// drum.SetDecay(0.0f);

    looper.Init(buf, MAX_SIZE);
	if (sw4.RawState()) // up
		looper.SetMode(Looper::Mode::ONETIME_DUB);
	else // down
		looper.SetMode(Looper::Mode::NORMAL);

    hw.adc.Init(adcConfig, 6);
    hw.adc.Start();

	// get starting values for the pots
	p1 = hw.adc.GetFloat(0); // effect param 1
	p2 = hw.adc.GetFloat(1); // effect param 2
	p3 = hw.adc.GetFloat(2); // effect param 3
	p4 = hw.adc.GetFloat(3); // gain for some effects OR effect param 4
	p5 = hw.adc.GetFloat(4); // metro tempo (used for DRUMS)
	p6 = hw.adc.GetFloat(5); // looper gain

	// find the active effect
	updateActiveEffect();

	// update the parameters for the active effect
	initFirstEffect();

	// start the main loop
	hw.StartAudio(AudioCallback);
}
