#pragma once

#include "sr_webcam.h"

#ifdef __cplusplus
extern "C" {
#endif
	
	struct _sr_webcam_device {
		int deviceId;
		int width;
		int height;
		int framerate;
		
		void * stream;
		sr_webcam_callback callback;
		void * user;
	};

#ifdef __cplusplus
}
#endif
