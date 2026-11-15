# Vocal Metronome — Windows VST3 project

A voice metronome using your four updated recordings. METRONOME title, dark charcoal surfaces, soft teal accents, and light text. Audio is embedded in the plugin. The original host-sync audio engine is preserved.

## Status

This archive contains the complete implementation and build automation, NOT a compiled VST3. It was prepared in a Linux environment; a Windows build has not been performed. It has not been compiled, loaded, or audio-tested in FL Studio. The Windows build must complete successfully before it can be used. Do not rename a source file to .vst3.

## Easiest build without installing developer tools: GitHub

1. Create an empty private GitHub repository.
2. Put the CONTENTS of this Vocal-Metronome folder in its root, including `.github/workflows/windows-vst3.yml`. The CMakeLists.txt file must be at the repository root. Using Git to upload preserves the hidden .github folder.
3. Open the repository's Actions tab, choose **Build Windows VST3**, and click **Run workflow**. If prompted, enable Actions for the repository.
4. After a successful run, open the run and download **Vocal-Metronome-Windows-x64-VST3** under Artifacts. Extract the downloaded ZIP.
5. Install the complete Vocal-Metronome.vst3 folder as below.

This runs compilation on a GitHub Windows machine. No account connection or upload has been performed for you. Your account's Actions availability and usage limits apply. If a build fails, its log identifies the error; no compiled release is included here.

## Local Windows build

Install Visual Studio 2022 with **Desktop development with C++**, including its Windows SDK and C++ CMake tools. Install Git for Windows with command-line support. Extract the entire project first, then double-click **Build-Windows.cmd**. Internet is needed on the first build to download JUCE 8.0.6 and its build dependencies.

The script uses Visual Studio 2022 Community's bundled CMake if `cmake` is not on PATH. For another Visual Studio edition, use its Developer PowerShell and run:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target VocalMetronome_VST3 --parallel
```

Output: `build/VocalMetronome_artefacts/Release/VST3/Vocal-Metronome.vst3`.

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

## Validation checklist after building

This is the remaining runtime acceptance test, not a claim of completed testing:

- Scan and open the plugin in FL Studio; check UI layout and all controls.
- In a 4/4 project, play at 60, 100 and 180 BPM alongside FL Studio's metronome. Check that all four counts align.
- Change and automate BPM while playing; check for drift.
- Stop/restart on a beat, start between beats, seek, and loop a four-bar range.
- Try both small and large audio buffers (64 and 1024 samples).
- Check each mute, volume, and pitch; save/reopen the FL project and verify settings restore.
- Close the plugin editor during playback; counts should continue.
- Test offline export with counting enabled and disabled.

## Dependencies and attribution

JUCE is fetched at build time from https://github.com/juce-framework/JUCE at tag 8.0.6. See JUCE's repository for its license terms; this archive does not grant a JUCE license. 

JUCE references:
- https://docs.juce.com/master/classjuce_1_1AudioPlayHead_1_1PositionInfo.html
- https://github.com/juce-framework/JUCE/blob/8.0.6/docs/CMake%20API.md
- https://juce.com/tutorials/tutorial_audio_processor_value_tree_state/
