#include <iostream>
#include <gl3w.h>
#include <GLFW/glfw3.h>

int main(int argc, char** argv){
	
	// Initialize glfw, which will create and setup an OpenGL context.
	if(!glfwInit()) {
		return 1;
	}
	
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	GLFWwindow * window = glfwCreateWindow(800, 600, "sr_webcam", NULL, NULL);
	
	if(!window) {
		glfwTerminate();
		return 2;
	}
	
	glfwMakeContextCurrent(window);
	if(gl3wInit()) {
		return 3;
	}
	if(!gl3wIsSupported(3, 2)) {
		return 4;
	}
	
	
	glfwSwapInterval(1);
	int width, height;
	glfwGetFramebufferSize(window, &width, &height);
	int w = 320; int h = 240;
	GLuint textureId;
	glBindTexture(GL_TEXTURE_2D, textureId);
	// Set proper max mipmap level.
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
	// Texture settings.
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	
	glBindTexture(GL_TEXTURE_2D, 0);
	
	while(!glfwWindowShouldClose(window)){
		glfwPollEvents();
		//glfwSetWindowShouldClose(_window, GLFW_TRUE);
		
		glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		/*
		 glBindTexture(GL_TEXTURE_2D, textureId);
		 glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_BGRA, GL_UNSIGNED_BYTE, &data[0]);
		 */
		// Blit
		GLuint srcFb;
		glGenFramebuffers(1, &srcFb);
		glBindFramebuffer(GL_READ_FRAMEBUFFER, srcFb);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
		glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureId, 0);
		glBlitFramebuffer(0, 0, w, h, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_LINEAR);
		glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
		glDeleteFramebuffers(1, &srcFb);
		
		glfwSwapBuffers(window);
	}
	glfwDestroyWindow(window);
	glfwTerminate();
	
	return 0;
}
