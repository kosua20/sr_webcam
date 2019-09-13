#include "common.h"

#include <iostream>
#include <vector>

int main(int argc, char** argv){
	
	// Video parameters.
	const int vidW = 320;
	const int vidH = 240;
	const int fps = 30;

	GLFWwindow * window = createWindow(2 * vidW, 2 * vidH);
	int winW, winH;

	const GLuint texId = setupTexture(vidW, vidH);

	// Dummy data for now.
	std::vector<unsigned char> data(vidW * vidH * 4);
	for (int y = 0; y < vidH; ++y) {
		for (int x = 0; x < vidW; ++x) {
			const int bid = 4 * (y * vidW + x);
			data[bid + 0] = 0;
			data[bid + 1] = ((x / 20) % 2 == 1) ? 128 : 0;
			data[bid + 2] = ((y / 20) % 2 == 1) ? 128 : 255;
			data[bid + 3] = 255;
		}
	}

	while (!glfwWindowShouldClose(window)) {

		glfwPollEvents();
		glfwGetFramebufferSize(window, &winW, &winH);
		glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		glBindTexture(GL_TEXTURE_2D, texId);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, vidW, vidH, 0, GL_BGRA, GL_UNSIGNED_BYTE, &data[0]);
		glBindTexture(GL_TEXTURE_2D, 0);

		blit(texId, vidW, vidH, winW, winH, 0);

		glfwSwapBuffers(window);
	}

	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}
