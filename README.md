# M5Tuna: Guitar Tuner for M5 Cardputer

A versatile instrument tuner application for the M5 Cardputer (including Cardputer ADV with ES8311 codec) that supports multiple instruments:

- Guitar
- Ukulele
- Violin
- Auto-detection mode

## Features

- **Real-time Pitch Detection**: Leverages the Q DSP Library with autocorrelation-based pitch detection
- **Custom HAL**: Standalone hardware abstraction layer using LovyanGFX, with dedicated drivers for display, microphone (I2S), speaker, keyboard, battery, LED, and ES8311 codec
- **Signal Processing Pipeline**:
  - Frequency range validation (27.5 Hz – 4200 Hz)
  - Median filter for outlier spike rejection
  - Dual 1-Euro adaptive filters for low-latency jitter reduction
  - Moving average and exponential smoothing
  - Consecutive same-note gating to suppress transient false detections
- **Smooth Visual Feedback**:
  - Pitch indicator with exponential smoothing and color interpolation (red → yellow → green)
  - In-tune crosshair with temporal stability and hysteresis
  - Real-time waveform visualizer with filled bars and contour edges
  - Animated hint text via hl_text utility
- **RGB LED Indicator**: Green when in tune, off when silent, with hysteresis to prevent flickering
- **Multiple Tuning Modes**: Switch between different instrument tunings
- **Continuous Audio Capture**: Triple-buffer rotation with asynchronous I2S recording for zero-gap audio

## Technical Details

- Sample rate: 16 kHz
- Frame size: 1024 samples
- A4 reference frequency: 440.0 Hz
- Display: LovyanGFX with double-buffered sprite rendering
- Platform: ESP-IDF v5.5

## Setup

1. Install Visual Studio Code
2. Install the Visual Studio Code ESP-IDF extension
3. Configure the ESP-IDF extension using express or advanced mode. Choose **v5.5** as the version of ESP-IDF.
4. Make sure you have `git` installed
5. Clone this repository
6. From a terminal, inside the project directory:
   ```
   git submodule update --init --recursive
   ```
7. Open the project folder in VS Code
8. Set your ESP32 target (esp32s3):
   - Open the Command Palette (Ctrl+Shift+P) and select `ESP-IDF: Set Espressif Device Target`
   - Select `esp32s3`
   - Select the `ESP32-S3 chip (via ESP-PROG)` option

## License

This software is licensed under the GNU General Public License (GPL) for open-source use.

## Acknowledgments

- [q-tune](https://github.com/joulupukki/q-tune)
- [Q DSP Library](https://github.com/michidk/q-dsp)
- [LovyanGFX](https://github.com/lovyan03/LovyanGFX)
