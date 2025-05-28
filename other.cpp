if(hw.switches[Terrarium::FOOTSWITCH_2].RisingEdge())
{
    if (!pausePlayback) {
        looper.TrigRecord();
        if (!looper.Recording()) {  // Turn on LED if not recording and in playback
            led2.Set(1.0f);
        }
        //led2.Set(looper.Recording() ? 0.0f : 1.0f); // Turn on LED when loop is playing but not recording
        if (!pswitches[3]) {
            looper.SetReverse(true);
        }
    }

    // Start or end double tap timer
    if (checkDoubleTap) {
        // if second press comes before 1.0 seconds, pause playback
        if (doubleTapCounter <= 1000) {
            if (looper.Recording()) {  // Ensure looper is not recording when double tapped (in case it gets double tapped while recording)
                looper.TrigRecord();
            }
            pausePlayback = !pausePlayback;
            if (pausePlayback) {        // Blink LED if paused, otherwise set to triangle wave for pulsing while recording
                led_osc2.SetWaveform(4); // WAVE_SIN = 0, WAVE_TRI = 1, WAVE_SAW = 2, WAVE_RAMP = 3, WAVE_SQUARE = 4
            } else {
                led_osc2.SetWaveform(1); 
            }
            doubleTapCounter = 0;    // reset double tap here also to prevent weird behaviour when triple clicked
            checkDoubleTap = false;
            led2.Set(1.0f);
        }
    } else {
        checkDoubleTap = true;
    }
}

if (checkDoubleTap) {
    doubleTapCounter += 1;          // Increment by 1 (48000 * 0.75)/blocksize = 1000   (blocksize is 48)
    if (doubleTapCounter > 1000) {  // If timer goes beyond 1.0 seconds, stop double tap checking
        doubleTapCounter = 0;
        checkDoubleTap = false;
    }
}