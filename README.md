# sr_webcam

This is a simple wrapper around live camera video capture libraries on Windows, macOS and Linux. It respectively interacts with:

* the TODO on Windows
* the TODO on macOS
* the TODO on Linux

Basic examples in C and C++ (including synchronization details) can be found in the `examples` directory.

## Dependencies

Each platform implementation can be found in a separate file.

* On Windows, this should link against `TODO`. Implementation is in `sr_webcam_win.cpp` (C++).
* On macOS, this should link against `TODO`. Implementation is in `sr_webcam_mac.m` (Objective-C).
* On Linux, this should link against `TODO`. Implementation is in `sr_webcam_lin.c` (C).

## Usage

### Selecting a device and configuring it

### Starting video capture

### Ending video capture

### Querying device informations

### Helpers

## Limitations

This utility library assumes that the device you are capturing can provide frames in an R8G8B8 or similar, and doesn't support conversion from more complex format such as YUV420. The library only allows you to select a device by ID, and doesn't expose querying for a device name.


