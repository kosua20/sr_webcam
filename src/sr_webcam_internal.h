#ifndef SR_WEBCAM_INTERNAL_H
#define SR_WEBCAM_INTERNAL_H

#include "sr_webcam.h"

#ifdef __cplusplus
extern "C" {
#endif

struct _sr_webcam_device {
	int deviceId;
	int width;
	int height;
	int framerate;
	int running;

	void* stream;
	sr_webcam_callback callback;
	void* user;
};

#ifdef __cplusplus
}
#endif

#endif
