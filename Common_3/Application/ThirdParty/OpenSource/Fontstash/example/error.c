//
// Copyright (c) 2013 Mikko Mononen memon@inside.org
//
// This software is provided 'as-is', without any express or implied
// warranty.  In no event will the authors be held liable for any damages
// arising from the use of this software.
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.
//

#include <stdio.h>
#include <string.h>
#define FONTSTASH_IMPLEMENTATION

//#define FONS_USE_FREETYPE

#include "fontstash.h"
#include <GLFW/glfw3.h>
#define GLFONTSTASH_IMPLEMENTATION
#include "glfontstash.h"

FONScontext* fs = NULL;
int size = 90;

void dash(float dx, float dy)
{
	glBegin(GL_LINES);
	glColor4ub(0,0,0,128);
	glVertex2f(dx-5,dy);
	glVertex2f(dx-10,dy);
	glEnd();
}

void line(float sx, float sy, float ex, float ey)
{
	glBegin(GL_LINES);
	glColor4ub(0,0,0,128);
	glVertex2f(sx,sy);
	glVertex2f(ex,ey);
	glEnd();
}

static void expandAtlas(FONScontext* stash)
{
	int w = 0, h = 0;
	fonsGetAtlasSize(stash, &w, &h);
	if (w < h)
		w *= 2;
	else
		h *= 2;
	fonsExpandAtlas(stash, w, h);
	printf("expanded atlas to %d x %d\n", w, h);
}

static void resetAtlas(FONScontext* stash)
{
	fonsResetAtlas(stash, 256,256);
	printf("reset atlas to 256 x 256\n");
}

static void key(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	(void)scancode;
	(void)mods;
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		glfwSetWindowShouldClose(window, GL_TRUE);

	if (key == GLFW_KEY_E && action == GLFW_PRESS)
		expandAtlas(fs);

	if (key == GLFW_KEY_R && action == GLFW_PRESS) {
		resetAtlas(fs);
	}

	if (key == 265 && action == GLFW_PRESS) {
		size += 10;
	}
	if (key == 264 && action == GLFW_PRESS) {
		size -= 10;
		if (size < 20) size = 20;
	}
}

void stashError(void* uptr, int error, int val)
{
	(void)uptr;
	FONScontext* stash = (FONScontext*)uptr;
	switch (error) {
	case FONS_ATLAS_FULL:
		printf("atlas full\n");
		expandAtlas(stash);
		break;
	case FONS_SCRATCH_FULL:
		printf("scratch full, tried to allocate %d has %d\n", val, FONS_SCRATCH_BUF_SIZE);
		break;
	case FONS_STATES_OVERFLOW:
		printf("states overflow\n");
		break;
	case FONS_STATES_UNDERFLOW:
		printf("statels underflow\n");
		break;
	}
}

int main()
{
	int fontNormal = FONS_INVALID;
	int fontItalic = FONS_INVALID;
	int fontBold = FONS_INVALID;
	int fontJapanese = FONS_INVALID;
	GLFWwindow* window;
	const GLFWvidmode* mode;
	
	if (!glfwInit())
		return -1;

	mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
    window = glfwCreateWindow(mode->width - 40, mode->height - 80, "Font Stash", NULL, NULL);
	if (!window) {
		glfwTerminate();
		return -1;
	}

    glfwSetKeyCallback(window, key);
	glfwMakeContextCurrent(window);

	fs = glfonsCreate(256, 256, FONS_ZERO_TOPLEFT);
	if (fs == NULL) {
		printf("Could not create stash.\n");
		return -1;
	}

	fonsSetErrorCallback(fs, stashError, fs);

	fontNormal = fonsAddFont(fs, "sans", "../example/DroidSerif-Regular.ttf");
	if (fontNormal == FONS_INVALID) {
		printf("Could not add font normal.\n");
		return -1;
	}
	fontItalic = fonsAddFont(fs, "sans-italic", "../example/DroidSerif-Italic.ttf");
	if (fontItalic == FONS_INVALID) {
		printf("Could not add font italic.\n");
		return -1;
	}
	fontBold = fonsAddFont(fs, "sans-bold", "../example/DroidSerif-Bold.ttf");
	if (fontBold == FONS_INVALID) {
		printf("Could not add font bold.\n");
		return -1;
	}
	fontJapanese = fonsAddFont(fs, "sans-jp", "../example/DroidSansJapanese.ttf");
	if (fontJapanese == FONS_INVALID) {
		printf("Could not add font japanese.\n");
		return -1;
	}

	while (!glfwWindowShouldClose(window))
	{
		float sx, sy, dx, dy, lh = 0;
		int width, height;
		int atlasw, atlash;
		unsigned int white,black,brown,blue;
		char msg[64];
		glfwGetFramebufferSize(window, &width, &height);
		// Update and render
		glViewport(0, 0, width, height);
		glClearColor(0.3f, 0.3f, 0.32f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDisable(GL_TEXTURE_2D);
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(0,width,height,0,-1,1);

		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		glDisable(GL_DEPTH_TEST);
		glColor4ub(255,255,255,255);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_CULL_FACE);

		white = glfonsRGBA(255,255,255,255);
		brown = glfonsRGBA(192,128,0,128);
		blue = glfonsRGBA(0,192,255,255);
		black = glfonsRGBA(0,0,0,255);

		sx = 50; sy = 50;
		
		dx = sx; dy = sy;

		dash(dx,dy);

		fonsClearState(fs);

		fonsSetSize(fs, size);
		fonsSetFont(fs, fontNormal);
		fonsVertMetrics(fs, NULL, NULL, &lh);
		dx = sx;
		dy += lh;
		dash(dx,dy);
		
		fonsSetSize(fs, size);
		fonsSetFont(fs, fontNormal);
		fonsSetColor(fs, white);
		dx = fonsDrawText(fs, dx,dy,"The quick ",NULL);

		fonsSetSize(fs, size/2);
		fonsSetFont(fs, fontItalic);
		fonsSetColor(fs, brown);
		dx = fonsDrawText(fs, dx,dy,"brown ",NULL);

		fonsSetSize(fs, size/3);
		fonsSetFont(fs, fontNormal);
		fonsSetColor(fs, white);
		dx = fonsDrawText(fs, dx,dy,"fox ",NULL);

		fonsSetSize(fs, 14);
		fonsSetFont(fs, fontNormal);
		fonsSetColor(fs, white);
		fonsDrawText(fs, 20, height-20,"Press UP / DOWN keys to change font size and to trigger atlas full callback, R to reset atlas, E to expand atlas.",NULL);

		fonsGetAtlasSize(fs, &atlasw, &atlash);
		snprintf(msg, sizeof(msg), "Atlas: %d × %d", atlasw, atlash);
		fonsDrawText(fs, 20, height-50, msg, NULL);

		fonsDrawDebug(fs, width - atlasw - 20, 20.0);

		glEnable(GL_DEPTH_TEST);

		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	glfonsDelete(fs);

	glfwTerminate();
	return 0;
}
