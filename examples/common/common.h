#pragma once

#include <gl3w.h>
#include <GLFW/glfw3.h>

#ifdef __cplusplus
extern "C" {
#endif

GLFWwindow* createWindow(int w, int h);

GLuint setupTexture(int w, int h);

void blit(GLuint texId, int srcW, int srcH, int dstW, int dstH, int flip);

#ifdef __cplusplus
}
#endif