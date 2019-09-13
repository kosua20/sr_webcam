#include "sr_webcam_internal.h"
#include <Dshow.h>

int sr_webcam_setup(sr_webcam_device * device) {
	
	/*device->stream = stream;
	device->width = stream->_width;
	device->height = stream->_height;
	device->deviceId = stream->_id;*/
	return 0;
}


void sr_webcam_start(sr_webcam_device * device) {
	if (device->stream) {
	}
}

void sr_webcam_stop(sr_webcam_device * device) {
	if (device->stream) {
		
	}
}
