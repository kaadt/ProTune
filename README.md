# ProTune

ProTune is a JUCE-based VST3 plugin that delivers transparent or creative automatic vocal tuning with ultra-low latency. It is designed for real-time use in DAWs such as FL Studio and supports MIDI control for live performance workflows.

## Features

- Real-time pitch detection and correction powered by FFT analysis and JUCE's pitch-shifting utilities
- Speed and Note Transition controls for choosing between transparent correction and hard-tuned effects
- Tolerance control to preserve natural performance nuance
- Advanced formant preservation mix for natural-sounding vocals
- Adjustable vocal range and vibrato tracking
- Key and scale selectors for chromatic, major, or minor correction modes
- Confidence-weighted note locking and dynamic retune response inspired by the Hildebrandt Auto-Tune patent
- Auto-Tune style tuning meter with live detected/target note readouts
- Optional MIDI-controlled target notes for stage-ready performances
- Responsive UI with live detected/target pitch readouts

## Getting Started

### Prerequisites

- **JUCE 7.0 or newer** with the `juce_dsp` module available. Either install the JUCE CMake package or clone JUCE locally.
- **CMake 3.15+**
- **Visual Studio 2022** with the Desktop development with C++ workload

### Configure JUCE

You can provide JUCE to the project in one of two ways:

1. **Using the JUCE CMake package** (recommended):
   ```powershell
   cmake -B build -DJUCE_DIR="C:/path/to/juce/extras/Build/JUCE"
   ```
2. **Using JUCE sources**:
   ```powershell
   cmake -B build -DJUCE_SOURCE_DIR="C:/path/to/JUCE"
   ```

### Build the Plugin

1. Generate the Visual Studio solution:
   ```powershell
   cmake -B build -S . -DJUCE_DIR="C:/path/to/juce/extras/Build/JUCE"
   ```
2. Open `build/ProTune.sln` in Visual Studio.
3. Select the `ProTune_VST3` target and build. You can also build the `ProTune_Standalone` target for a self-contained test app.
4. The resulting VST3 binary will be copied to `build/ProTune_artefacts/VST3/`.

### Quick Testing via Projucer

If you prefer to iterate inside JUCE's Projucer rather than launching a full DAW:

1. Open `ProTune.jucer` in Projucer and ensure your JUCE modules path is configured under **Projucer → Global Paths**.
2. Enable the exporter you want to use (e.g. Xcode, Visual Studio 2022, or Linux Make) and click **Save Project** to generate the platform builds.
3. If your JUCE checkout lives somewhere other than `../JUCE`, edit the module paths in `ProTune.jucer` (or set Projucer's **Global Paths**) so they point at your local `JUCE/modules` directory.
4. Click **Open in IDE** (or open the generated project manually) and build the `ProTune_Standalone` target once so the executable exists.
5. Back in Projucer, pick the same exporter/configuration and press the run button to launch the standalone instance for quick auditions without FL Studio.

The standalone build output lives under `Builds/<Exporter>/<Configuration>/ProTune_Standalone` and will recompile as you tweak code and resave the project.

### Using in FL Studio

1. Copy the generated `ProTune.vst3` to your VST3 plugins folder (e.g. `C:\Program Files\Common Files\VST3`).
2. In FL Studio, run **Options → Manage plugins** and refresh the plugin list.
3. Load ProTune from the Effects section and fine-tune the parameters to match your vocalist and desired effect.

### Dialing in the “Auto-Tune” sound

If you are not hearing an obvious correction effect, follow the [Auto-Tune Style Setup Guide](docs/AUTOTUNE_GUIDE.md). It walks through input preparation, recommended parameter ranges, and debugging steps that map directly to the DSP engine so you can confirm each stage is working as expected. For a development-oriented action plan, see [Next Steps Toward an Audible Auto-Tune Effect](docs/NEXT_STEPS.md).

## MIDI Control

Enable the **MIDI Control** toggle to drive pitch correction from incoming MIDI notes. When active, held MIDI notes determine the target pitch instead of the automatic scale snapping, enabling expressive live performance control.

## Project Structure

```
CMakeLists.txt
Source/
  ├── PitchCorrectionEngine.h/.cpp   // DSP engine for pitch detection and correction
  ├── PluginProcessor.h/.cpp         // JUCE audio processor implementation
  └── PluginEditor.h/.cpp            // UI and parameter controls
```

## License

This project is provided as-is for educational purposes.
