# Vocal Metronome — Windows VST3 project

## Install in FL Studio on Windows

1. Close FL Studio.
2. Copy the ENTIRE `Vocal-Metronome.vst3` folder into `C:\Program Files\Common Files\VST3`. Windows may request administrator permission. Preserve the folder structure, including Contents/x86_64-win.
3. Open FL Studio, go to **Options > Manage plugins**, then **Find installed plugins**.
4. Add Vocal-Metronome as an instrument to the Channel Rack and route it to the mixer track you want to hear.
5. Leave **Sync to FL Studio / DAW** checked and counting enabled. Press FL Studio's Play button. No MIDI notes are required.

If FL Studio suspends it, disable **Smart disable** for the plugin in its wrapper. Turn counting off or mute its mixer track before exporting if you do not want the spoken metronome rendered into your music.

## Behavior

- Sync mode reads host play/stop, BPM and PPQ beat position on every audio block. Playback is scheduled on sample boundaries, without GUI timers driving sound.
- Counts repeat 1–2–3–4 on QUARTER-NOTE boundaries, anchored to project PPQ zero. This first version assumes a four-quarter-note cycle; it does not adapt the spoken count to other time signatures.
- Starting between beats waits until the next quarter note and uses the corresponding count. Seeking and loop jumps discard the previous sample voice and follow the new host position at the next processed block.
- It follows host tempo updates at audio-block boundaries. It does not interpolate tempo ramps inside a block; accuracy depends on timing information supplied by FL Studio.
- Uncheck Sync for manual BPM (20–300). Counting runs independently of the transport while FL Studio processes the instrument. The host can suspend processing while stopped.
- Pitch spans -12 to +12 semitones using sample playback rate, like the HTML version. This changes clip length as well as pitch; long clips fade before the next beat.
- Volume and four mute buttons are automatable. Plugin parameters are saved and restored with the DAW project. Muting a beat leaves the count grid running.
- It does not implement FL Studio's pre-roll/count-in system. It is an audible instrument that follows the transport.
- Embedded recordings have leading silence trimmed using the same threshold and 8 ms margin as the HTML version. Original WAV bytes are included unchanged.



## Dependencies and attribution

JUCE is fetched at build time from https://github.com/juce-framework/JUCE at tag 8.0.6. See JUCE's repository for its license terms; this archive does not grant a JUCE license. 

JUCE references:
- https://docs.juce.com/master/classjuce_1_1AudioPlayHead_1_1PositionInfo.html
- https://github.com/juce-framework/JUCE/blob/8.0.6/docs/CMake%20API.md
- https://juce.com/tutorials/tutorial_audio_processor_value_tree_state/
