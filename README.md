# sr_webcam

This is a simple wrapper around live camera video capture libraries on Windows, macOS and Linux. It respectively interacts with:

* the MediaFoundation library on Windows
* the AVFoundation framework on macOS
* the video4linux2 system library on Linux

Basic examples in C and C++ (including synchronization details) can be found in the `examples` directory. The examples connect to the first available device and display the capture in an OpenGL window. 

Common files: `include/sr_webcam.h`, `src/sr_webcam_internal.h`, `src/sr_webcam.c`

## Dependencies

Each platform implementation can be found in a separate file.

* On Windows, this should link against `mfplat`, `mf`, `mfuuid`, `Mfreadwrite`and `Shlwapi`. Implementation is in `src/sr_webcam_win.cpp` (C++).
* On macOS, this should link against `AVFoundation.framework`, `CoreMedia.framework` and `Accelerate.framework`. Implementation is in `src/sr_webcam_mac.m` (Objective-C). **You additionally need** to request the capability for your app to access camera devices by either:
    - placing `examples/Info.plist` in the same directory as your binary.
    - embedding it in the binary, in a `__TEXT` section named `__info_plist` (see `examples/premake5.lua` for an example). 
* On Linux, this should link against `pthread`. Implementation is in `src/sr_webcam_lin.c` (C).

## Usage

    struct sr_webcam_device;
    typedef void (*sr_webcam_callback)(sr_webcam_device * device, void * data);

### Selecting a device and configuring it

    int sr_webcam_create(sr_webcam_device ** device, int deviceId);
    void sr_webcam_set_format(sr_webcam_device * device, int width, int height, int framerate);
    void sr_webcam_set_callback(sr_webcam_device * device, sr_webcam_callback callback);
    void sr_webcam_set_user(sr_webcam_device * device, void * user);

### Starting video capture

    int sr_webcam_open(sr_webcam_device * device);
    void sr_webcam_start(sr_webcam_device * device);

### Ending video capture

    void sr_webcam_stop(sr_webcam_device * device);
    void sr_webcam_delete(sr_webcam_device * device);

### Querying device informations

    long sr_webcam_get_format_size(sr_webcam_device * device);
    void * sr_webcam_get_user(sr_webcam_device * device);
    void sr_webcam_get_dimensions(sr_webcam_device * device, int * width, int * height);
    void sr_webcam_get_framerate(sr_webcam_device * device, int * fps);

## Limitations

This utility library assumes that the device you are capturing can provide frames in an R8G8B8 or similar, and doesn't support conversion from more complex format such as YUV420. The library only allows you to select a device by ID, and doesn't expose querying for a device name.


