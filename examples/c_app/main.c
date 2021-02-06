#include "common.h"

#include <sr_webcam.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

unsigned char* buffer = NULL;
int hasFrame		  = 0;

void callback(sr_webcam_device* device, void* data) {
	memcpy(buffer, data, sr_webcam_get_format_size(device));
	hasFrame = 1;
}

int main(int argc, char** argv) {
	(void)argc;
	(void)argv;

	// Video parameters.
	const int w	  = 320;
	const int h	  = 240;
	const int fps = 30;
	// Create window.
	GLFWwindow* window = createWindow(2 * w, 2 * h);
	int winW, winH;
	// Open device.
	sr_webcam_device* device;
	sr_webcam_create(&device, 0);
	sr_webcam_set_format(device, w, h, fps);
	sr_webcam_set_callback(device, callback);
	sr_webcam_open(device);
	// Get back video parameters.
	int vidW, vidH, vidFps;
	sr_webcam_get_dimensions(device, &vidW, &vidH);
	sr_webcam_get_framerate(device, &vidFps);
	// Allocate the buffer and texture.
	int vidSize		   = sr_webcam_get_format_size(device);
	buffer			   = (unsigned char*)calloc(vidSize, sizeof(unsigned char));
	const GLuint texId = setupTexture(vidW, vidH);
	// Start.
	sr_webcam_start(device);
	while(!glfwWindowShouldClose(window)) {
		glfwPollEvents();
		glfwGetFramebufferSize(window, &winW, &winH);
		glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		// Upload texture data.
		if(hasFrame > 0) {
			glBindTexture(GL_TEXTURE_2D, texId);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, vidW, vidH, 0, GL_RGB, GL_UNSIGNED_BYTE, buffer);
			glBindTexture(GL_TEXTURE_2D, 0);
			hasFrame = 0;
		}

		blit(texId, vidW, vidH, winW, winH, 1);

		glfwSwapBuffers(window);
	}
	sr_webcam_stop(device);
	sr_webcam_delete(device);
	free(buffer);
	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}
