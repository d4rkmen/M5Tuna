# M5Tuna: Guitar Tuner for M5 Cardputer

A versatile instrument tuner application for the M5 Cardputer that supports multiple instruments:

- Guitar
- Ukulele
- Violin
- Auto-detection mode

## Features

- **Audio Input**: Uses the M5 Cardputer's internal microphone to capture audio
- **Responsive UI**: Built with LovyanGFX for a smooth and intuitive interface
- **Accurate Pitch Detection**: Leverages the Q DSP Library for precise frequency detection
- **Advanced Filtering**:
  - Implements 1EuroFilter for stable pitch readings
  - Uses ExponentialSmoother to reduce jitter in displayed values
- **Multiple Tuning Modes**: Switch between different instrument tunings
- **Visual Feedback**: Clear visual indicators show when strings are in tune

## Technical Details

- Sample rate: 16kHz
- Frame size: 1024 samples
- A4 reference frequency: 440.0 Hz

## Setup

1. Install Visual Studio Code
2. Install the Visual Studio Code ESP-IDF extension
3. Configure the ESP-IDF extension by using the express or advanced mode. Choose **v5.4.0** as the version of ESP-IDF to use.
4. Make sure you have `git` installed
5. Clone this github project
6. From a terminal go into the project and do this:
   ```
   git submodule update --init --recursive
   ```
7. Open the `m5tuna` folder in VS Code
8. Set your ESP32 target (esp32s3)
   - Open the Command Palette (Command+Shift+P on a Mac) and select `ESP-IDF: Set Espressif Device Target`
   - Select `esp32s3`
   - Select the `ESP32-S3 chip (via ESP-PROG)` option

## License

This software is licensed under the GNU General Public License (GPL) for open-source use.

## Acknowledgments

- [q-tune](https://github.com/joulupukki/q-tune)
- [Q DSP Library](https://github.com/michidk/q-dsp)
- [LovyanGFX](https://github.com/lovyan03/LovyanGFX)
