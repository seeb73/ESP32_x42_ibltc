libLTC
------

Linear (or Longitudinal) Timecode (LTC) is an encoding of SMPTE timecode data
as a Manchester-Biphase encoded audio signal.
The audio signal is commonly recorded on a VTR track or other storage media.

libltc provides functionality to encode and decode LTC audio from/to
SMPTE or EBU timecode, including SMPTE date support.

libltc is the successor of [libltcsmpte](https://sourceforge.net/projects/ltcsmpte/).
For more information, please see the FAQ in the documentation.

Documentation
-------------

The API reference, examples, as well as introduction can be found at

https://x42.github.io/libltc/

This site is part or the source-code in the doc/ folder.

# ESP32 Port of x42-libltc

This repository is a fork of the original `x42-libltc` library, ported to work with the **ESP32** microcontroller within the Arduino environment.

## Changes in this Fork

The goal of this fork is to make the library easily usable for Arduino IDE users working with ESP32.

- The original library code has been preserved.
- **Arduino IDE compatibility files** (`library.properties`, `keywords.txt`) have been added to the root directory.
- **Wrapper files** (`ESP32_x42_libltc.cpp` and `ESP32_x42_libltc.h`) have been added to the `src` directory to bridge the original library with the Arduino framework.
- The library has been tested and confirmed to work with **Arduino IDE v2.3.x**.

## File Structure

The key files added for ESP32/Arduino compatibility are:
```
ESP32_x42_libltc/
├── library.properties
├── keywords.txt
└── src/
    ├── ESP32_x42_libltc.cpp
    └── ESP32_x42_libltc.h
    └── (original library files...)
```

## Installation

To install this library for use with the Arduino IDE, follow these steps:

1.  Click the `Code` button on this repository page and select `Download ZIP`.
2.  Unzip the downloaded file. You will have a folder named something like `ESP32_x42_libltc-main`. Rename it to `ESP32_x42_libltc`.
3.  Move the `ESP32_x42_libltc` folder to your Arduino libraries directory. The default location is:
    - **Windows:** `Documents\Arduino\libraries\`
    - **macOS:** `~/Documents/Arduino/libraries/`
    - **Linux:** `~/Arduino/libraries/`

    The final path should look like this: `...\Documents\Arduino\libraries\ESP32_x42_libltc\`

4.  Restart the Arduino IDE.

The library will now be available under `Sketch > Include Library > ESP32_x42_libltc`.

## Usage

Once installed, you can include the library in your ESP32 projects:

```cpp
#include <ESP32_x42_libltc.h>

void setup() {
  // Your setup code here
}

void loop() {
  // Your main code here
}
