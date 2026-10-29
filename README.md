# AtmosRiser (VST3)

A reverse-reverb and riser plugin. Put it on a vocal or instrument track, press play: it
listens to the track, picks the first phrase, and renders a reverse-reverb swell that rises
into it. On the next playback the swell plays in sync with your timeline. You can also drag the
result onto a track as a WAV, or drop any audio file into the plugin instead.

## The track workflow

1. Insert AtmosRiser on the track. It listens automatically: there is no button to press.
2. Press play in your DAW (space bar or the play button) over the part you want. The waveform
   shows "Capturing the track" while it records what the track plays (up to the last 90 s).
   The track itself passes through unchanged.
3. Press stop. The take is kept, the first phrase is selected, and the swell is rendered.
4. Audition it with **LISTEN** / **LOOP** inside the plugin. These never re-record.
5. Drag the EDIT waveform onto a track. The info line shows the bar where the swell starts
   and the file name includes it.

**Every DAW playback captures again and replaces the take.** To keep the one you have, click
**LOCK** (top right of the waveform): playback then leaves the take alone. Exports and bounces never
capture. A playback where the track is silent keeps the previous take. **RESET** clears the
recording inside the plugin and releases the lock.

Dragged and exported clips start exactly on a bar line (padded with silence), and the file name
says which bar to drop them on, e.g. `track_drop-on-bar-16_atmosriser.wav`. With grid snapping on,
the swell lands sample-accurately in front of the phrase.

Adjust the phrase in **RAW** view (drag to select, drag the handles). Takes are saved in
`%APPDATA%\AtmosRiser\Captures` (one file per instance; older takes from the same session are
deleted), so reopening the project restores the last one.

Automatic sibilance detection is always on. It finds "s"/"sh" sounds in the selection and turns
their highs down by about 18 dB in the signal that feeds the reverb, so they don't echo
backwards and smear the start of the swell. Your original audio is not changed.

## Run the interface in a browser (no plugin needed)

```bash
cd ui
```

```bash
npm install
```

```bash
npm run dev
```

Open the printed `http://localhost:5173` URL. The browser preview uses a mock backend: every
control works and the waveform follows the parameters, but it makes no sound and the
drag-into-DAW and export actions only show a message.

## Build the plugin (Windows)

Requirements: CMake 3.22+, Visual Studio 2022 (Build Tools are enough), Node.js 18+ and npm.
The Microsoft Edge WebView2 Runtime must be present on the machine that runs the plugin
(it ships with Windows 11).

```bash
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
```

```bash
cmake --build build --config Release --target AtmosRiser_VST3 AtmosRiser_Standalone -- -m
```

The CMake build runs `npm install` on first configure and `npm run build` whenever a UI source
file changes, then embeds `ui/dist/index.html` into the plugin binary.

JUCE is picked up from `./JUCE`, then `../EchoBox/JUCE`, otherwise fetched from GitHub (8.0.4).
The WebView2 SDK (NuGet package `Microsoft.Web.WebView2` 1.0.1901.177) is downloaded from
nuget.org into `third_party/webview2` on the first configure and linked statically, so there is
no extra DLL to ship.

Outputs:

- `build/AtmosRiser_artefacts/Release/VST3/AtmosRiser.vst3`
- `build/AtmosRiser_artefacts/Release/Standalone/AtmosRiser.exe`

Install by copying the `.vst3` folder into `C:\Program Files\Common Files\VST3\` and
rescanning plugins in your DAW.

## Using it

| Control | What it does |
| --- | --- |
| Drop a file anywhere on the plugin | Loads it (WAV, AIFF, FLAC, MP3, OGG). The menu also has **Load audio file…**. A demo pluck is loaded until you do. |
| **RAW** view | Shows the source. Drag to select the segment, drag the cyan handles to adjust, double-click to select everything. |
| **EDIT** view | Shows the rendered clip. Drag the waveform onto a DAW track to drop the WAV there. |
| **LISTEN / STOP** | Plays the selected source in RAW view, or the processed clip in EDIT view. Space bar and any MIDI note also trigger it. |
| **LOOP** | Repeats the preview. |
| **RESET** | Clears the captured or loaded audio. The next DAW playback captures the track again. Knob settings are kept. |
| **Phone / Chorus / Tremolo** | Independent on/off switches for the swell; any combination can be on, and an effect that is on glows. Hover one for its settings: Phone has Mix and Tone, Chorus has Mix and Rate, Tremolo has Mix and Timing (1/4 to 1/32, triplets) locked to the tempo. |
| **Size** (left of the orb) | Room size of the reverb, 10–150 %: small and tight to a long wash. |
| **Low Cut** (left, below Size) | High-pass on the swell, Off to 1 kHz, to keep it from muddying the mix. |
| **Fade** (right of the orb) | How the swell starts: 0 % begins with the reverb already audible, 100 % rises smoothly from silence. |
| **Level** (right, below Fade) | Output level of the swell, -24 to +6 dB. |
| **LOCK** | Keeps the current take when the DAW plays. |
| **Reverse orb** | Musical length of the swell: 1/8 bar to 8 bars, following the host tempo and time signature (120 BPM in the standalone app). |
| Presets | Seven factory presets. **Save** stores your own in `%APPDATA%\AtmosRiser\Presets`. |
| Menu | Load audio, Export WAV, open the exports folder, and **Include original after the swell** (off by default: the clip is the swell alone and ends where the phrase begins). |

Knobs and the orb respond to vertical drag (Shift for fine), the mouse wheel, arrow keys and
Page Up/Down. Double-click resets a control.

Exports and drag-outs are written to `Documents\AtmosRiser Exports`. Dropped files are copied
to `%APPDATA%\AtmosRiser\Imported Audio` so the DAW project can reload them later.

## Testing against a simulated DAW

`tests/HostCheck.cpp` loads the built VST3 the way a DAW does. It plays a phrase through it with
the transport running, stops, plays the timeline again with the track silent, and checks that the
swell is silent beforehand, rises, and peaks where the phrase sits. It also checks the sibilance
detector.

```bash
cmake --build build --config Release --target AtmosRiserHostCheck
```

```bash
build/AtmosRiserHostCheck_artefacts/Release/AtmosRiserHostCheck.exe build/AtmosRiser_artefacts/Release/VST3/AtmosRiser.vst3
```

## How the effect is rendered

1. The selection is cut out with 5 ms edge fades and resampled to the host rate.
2. Sibilant passages are detected (5 ms frames whose energy is mostly above 4.5 kHz) and tamed
   in a copy that feeds the reverb.
3. That copy is reversed, passed through the diffuser, and then through a 100 % wet reverb with a
   silent tail as long as the chosen musical length.
4. Only the reverb tail is kept: what rings on after the reversed phrase has finished. The part
   recorded while the phrase was still playing would contain the phrase itself, so it is dropped.
5. The tail is matched to the original's peak and reversed, so it swells into the moment the
   phrase begins and stops there. The selected mode colours it and the fade-in is applied.
   With "Include original" on, the phrase follows after the pre-delay.

The clip is peak-limited to -0.1 dBFS. Rendering happens off the audio thread, about 80 ms
after the last change.

## Project layout

```
Source/                     C++ plugin
  ReverseReverbRenderer.*   offline DSP: segment -> reverse -> diffuse -> reverb -> reverse -> colour
  PluginProcessor.*         parameters, presets, tempo sync, playback, state, UI snapshots
  PluginEditor.*            WebView2 host and the JS <-> C++ bridge
ui/                         web interface (React + TypeScript + Vite)
  src/bridge/               backend contract, JUCE implementation, browser mock
  src/components/           PluginHeader, WaveformDisplay, ReverseOrb, RotaryKnob,
                            ParameterPanel, EffectModeSelector, IconButton, icons
  src/styles/               tokens.css, layout.css, components.css
third_party/webview2/       WebView2 SDK headers and static loader
```

Pre-delay (20 ms) and diffusion (70 %) are fixed inside the engine: with a swell-only clip the
pre-delay only moves its placement, and diffusion barely changes the sound.
