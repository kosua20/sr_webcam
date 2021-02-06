#include "common.h"
#include <stdlib.h>

GLFWwindow* createWindow(int w, int h) {

	// Initialize glfw, which will create and setup an OpenGL context.
	if(!glfwInit()) {
		return NULL;
	}

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	GLFWwindow* window = glfwCreateWindow(w, h, "sr_webcam", NULL, NULL);

	if(!window) {
		glfwTerminate();
		return NULL;
	}

	glfwMakeContextCurrent(window);

	if(gl3wInit()) {
		return NULL;
	}
	if(!gl3wIsSupported(3, 2)) {
		return NULL;
	}

	glfwSwapInterval(1);
	return window;
}

GLuint setupTexture(int w, int h) {
	GLuint textureId = 0;
	glGenTextures(1, &textureId);
	glBindTexture(GL_TEXTURE_2D, textureId);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glBindTexture(GL_TEXTURE_2D, 0);
	return textureId;
}

void blit(GLuint texId, int srcW, int srcH, int dstW, int dstH, int flip) {
	GLuint srcFb;
	glGenFramebuffers(1, &srcFb);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, srcFb);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texId, 0);
	glBlitFramebuffer(0, 0, srcW, srcH, 0, flip * dstH, dstW, (1 - flip) * dstH, GL_COLOR_BUFFER_BIT, GL_LINEAR);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	glDeleteFramebuffers(1, &srcFb);
}