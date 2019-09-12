#pragma once

#ifdef __cplusplus
extern "C" {
#endif
	
	struct _sr_webcam_device;
	typedef struct _sr_webcam_device sr_webcam_device;
	
	typedef void (*sr_webcam_callback)(sr_webcam_device * device, void * data);
	
	int sr_webcam_open_device(sr_webcam_device ** device, int deviceId);
	
	void sr_webcam_set_format(sr_webcam_device * device, int width, int height, int framerate);
	
	void sr_webcam_set_callback(sr_webcam_device * device, sr_webcam_callback callback);
	
	void sr_webcam_set_user(sr_webcam_device * device, void * user);
	
	long sr_webcam_get_format_size(sr_webcam_device * device);
	
	void * sr_webcam_get_user(sr_webcam_device * device);
	
	int sr_webcam_setup(sr_webcam_device * device);
	
	void sr_webcam_start(sr_webcam_device * device);
	
	void sr_webcam_stop(sr_webcam_device * device);
	
#ifdef __cplusplus
}
#endif

