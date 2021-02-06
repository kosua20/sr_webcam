#include "common.h"
#include <sr_webcam.h>

#include <iostream>
#include <vector>
#include <mutex>

class VideoStream {
public:
	VideoStream(int w, int h, int fps) {
		sr_webcam_create(&device, 0);
		sr_webcam_set_format(device, w, h, fps);
		sr_webcam_set_callback(device, &VideoStream::callback);
		sr_webcam_set_user(device, this);
		const int res1 = sr_webcam_open(device);
		if(res1 != 0) {
			std::cout << "Unable to open device." << std::endl;
			return;
		}
		buffer.resize(sr_webcam_get_format_size(device));
		sr_webcam_get_dimensions(device, &_w, &_h);
		sr_webcam_get_framerate(device, &_fps);
	}

	~VideoStream() {
		sr_webcam_delete(device);
	}

	void start() {
		sr_webcam_start(device);
	}

	void stop() {
		sr_webcam_stop(device);
	}

	static void callback(sr_webcam_device* device, void* data) {
		VideoStream* stream = static_cast<VideoStream*>(sr_webcam_get_user(device));
		stream->dataReceived(data, sr_webcam_get_format_size(device));
	}

	void dataReceived(void* data, size_t size) {
		std::lock_guard<std::mutex> guard(mutex);
		uint8_t* rgb = static_cast<uint8_t*>(data);
		std::copy(rgb, rgb + size, buffer.begin());
		newFrame = true;
	}

	bool getRGB(std::vector<uint8_t>& dst) {
		std::lock_guard<std::mutex> guard(mutex);
		if(!newFrame) {
			return false;
		}
		dst.swap(buffer);
		newFrame = false;
		return true;
	}

	int w() const { return _w; }

	int h() const { return _h; }

	int fps() const { return _fps; }

private:
	sr_webcam_device* device;
	std::vector<uint8_t> buffer;
	std::mutex mutex;
	bool newFrame = false;
	int _w = 0, _h = 0, _fps = 0;
};

int main(int, char**) {

	// Video parameters.
	const int w	  = 320;
	const int h	  = 240;
	const int fps = 30;

	GLFWwindow* window = createWindow(2 * w, 2 * h);
	int winW, winH;

	// Open device.
	VideoStream webcam(w, h, fps);
	int vidW = webcam.w();
	int vidH = webcam.h();

	std::vector<unsigned char> buffer(vidW * vidH * 3);

	// Setup texture.
	const GLuint texId = setupTexture(vidW, vidH);

	std::cout << "Webcam created with size " << vidW << "x" << vidH << "@" << webcam.fps() << "fps." << std::endl;
	webcam.start();
	while(!glfwWindowShouldClose(window)) {

		glfwPollEvents();

		if(webcam.getRGB(buffer)) {
			glBindTexture(GL_TEXTURE_2D, texId);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, vidW, vidH, 0, GL_RGB, GL_UNSIGNED_BYTE, &buffer[0]);
			glBindTexture(GL_TEXTURE_2D, 0);
		}

		glfwGetFramebufferSize(window, &winW, &winH);
		glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		blit(texId, vidW, vidH, winW, winH, 1);

		glfwSwapBuffers(window);
	}

	webcam.stop();
	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}
