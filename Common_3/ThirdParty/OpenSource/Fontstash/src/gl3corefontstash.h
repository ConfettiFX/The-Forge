//
// Copyright (c) 2009-2013 Mikko Mononen memon@inside.org
//
// This software is provided 'as-is', without any express or implied
// warranty.  In no event will the authors be held liable for any damages
// arising from the use of this software.
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
// 1. The origin of this software must not be misrepresented; you must not
//	claim that you wrote the original software. If you use this software
//	in a product, an acknowledgment in the product documentation would be
//	appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//	misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.
//
#ifndef GL3COREFONTSTASH_H
#define GL3COREFONTSTASH_H

#ifdef __cplusplus
extern "C" {
#endif

FONS_DEF FONScontext* glfonsCreate(int width, int height, int flags);
FONS_DEF void glfonsDelete(FONScontext* ctx);

FONS_DEF unsigned int glfonsRGBA(unsigned char r, unsigned char g, unsigned char b, unsigned char a);

#ifdef __cplusplus
}
#endif

#endif // GL3COREFONTSTASH_H

#ifdef GLFONTSTASH_IMPLEMENTATION

#ifndef GLFONS_VERTEX_ATTRIB
#	define GLFONS_VERTEX_ATTRIB 0
#endif

#ifndef GLFONS_TCOORD_ATTRIB
#	define GLFONS_TCOORD_ATTRIB 1
#endif

#ifndef GLFONS_COLOR_ATTRIB
#	define GLFONS_COLOR_ATTRIB 2
#endif

struct GLFONScontext {
	GLuint tex;
	int width, height;
	GLuint vertexArray;
	GLuint vertexBuffer;
	GLuint tcoordBuffer;
	GLuint colorBuffer;
};
typedef struct GLFONScontext GLFONScontext;

static int glfons__renderCreate(void* userPtr, int width, int height)
{
	GLFONScontext* gl = (GLFONScontext*)userPtr;

	// Create may be called multiple times, delete existing texture.
	if (gl->tex != 0) {
		glDeleteTextures(1, &gl->tex);
		gl->tex = 0;
	}

	glGenTextures(1, &gl->tex);
	if (!gl->tex) return 0;

	if (!gl->vertexArray) glGenVertexArrays(1, &gl->vertexArray);
	if (!gl->vertexArray) return 0;

	glBindVertexArray(gl->vertexArray);

	if (!gl->vertexBuffer) glGenBuffers(1, &gl->vertexBuffer);
	if (!gl->vertexBuffer) return 0;

	if (!gl->tcoordBuffer) glGenBuffers(1, &gl->tcoordBuffer);
	if (!gl->tcoordBuffer) return 0;

	if (!gl->colorBuffer) glGenBuffers(1, &gl->colorBuffer);
	if (!gl->colorBuffer) return 0;

	gl->width = width;
	gl->height = height;
	glBindTexture(GL_TEXTURE_2D, gl->tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, gl->width, gl->height, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    static GLint swizzleRgbaParams[4] = {GL_ONE, GL_ONE, GL_ONE, GL_RED};
    glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzleRgbaParams);

	return 1;
}

static int glfons__renderResize(void* userPtr, int width, int height)
{
	// Reuse create to resize too.
	return glfons__renderCreate(userPtr, width, height);
}

static void glfons__renderUpdate(void* userPtr, int* rect, const unsigned char* data)
{
	GLFONScontext* gl = (GLFONScontext*)userPtr;
	int w = rect[2] - rect[0];
	int h = rect[3] - rect[1];

	if (gl->tex == 0) return;

	// Push old values
	GLint alignment, rowLength, skipPixels, skipRows;
	glGetIntegerv(GL_UNPACK_ALIGNMENT, &alignment);
	glGetIntegerv(GL_UNPACK_ROW_LENGTH, &rowLength);
	glGetIntegerv(GL_UNPACK_SKIP_PIXELS, &skipPixels);
	glGetIntegerv(GL_UNPACK_SKIP_ROWS, &skipRows);

	glBindTexture(GL_TEXTURE_2D, gl->tex);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, gl->width);
	glPixelStorei(GL_UNPACK_SKIP_PIXELS, rect[0]);
	glPixelStorei(GL_UNPACK_SKIP_ROWS, rect[1]);

	glTexSubImage2D(GL_TEXTURE_2D, 0, rect[0], rect[1], w, h, GL_RED, GL_UNSIGNED_BYTE, data);

	// Pop old values
	glPixelStorei(GL_UNPACK_ALIGNMENT, alignment);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, rowLength);
	glPixelStorei(GL_UNPACK_SKIP_PIXELS, skipPixels);
	glPixelStorei(GL_UNPACK_SKIP_ROWS, skipRows);
}

static void glfons__renderDraw(void* userPtr, const float* verts, const float* tcoords, const unsigned int* colors, int nverts)
{
	GLFONScontext* gl = (GLFONScontext*)userPtr;
	if (gl->tex == 0 || gl->vertexArray == 0) return;

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, gl->tex);

	glBindVertexArray(gl->vertexArray);

	glEnableVertexAttribArray(GLFONS_VERTEX_ATTRIB);
	glBindBuffer(GL_ARRAY_BUFFER, gl->vertexBuffer);
	glBufferData(GL_ARRAY_BUFFER, nverts * 2 * sizeof(float), verts, GL_DYNAMIC_DRAW);
	glVertexAttribPointer(GLFONS_VERTEX_ATTRIB, 2, GL_FLOAT, GL_FALSE, 0, NULL);

	glEnableVertexAttribArray(GLFONS_TCOORD_ATTRIB);
	glBindBuffer(GL_ARRAY_BUFFER, gl->tcoordBuffer);
	glBufferData(GL_ARRAY_BUFFER, nverts * 2 * sizeof(float), tcoords, GL_DYNAMIC_DRAW);
	glVertexAttribPointer(GLFONS_TCOORD_ATTRIB, 2, GL_FLOAT, GL_FALSE, 0, NULL);

	glEnableVertexAttribArray(GLFONS_COLOR_ATTRIB);
	glBindBuffer(GL_ARRAY_BUFFER, gl->colorBuffer);
	glBufferData(GL_ARRAY_BUFFER, nverts * sizeof(unsigned int), colors, GL_DYNAMIC_DRAW);
	glVertexAttribPointer(GLFONS_COLOR_ATTRIB, 4, GL_UNSIGNED_BYTE, GL_FALSE, 0, NULL);

	glDrawArrays(GL_TRIANGLES, 0, nverts);

	glDisableVertexAttribArray(GLFONS_VERTEX_ATTRIB);
	glDisableVertexAttribArray(GLFONS_TCOORD_ATTRIB);
	glDisableVertexAttribArray(GLFONS_COLOR_ATTRIB);

	glBindVertexArray(0);
}

static void glfons__renderDelete(void* userPtr)
{
	GLFONScontext* gl = (GLFONScontext*)userPtr;
	if (gl->tex != 0) {
		glDeleteTextures(1, &gl->tex);
		gl->tex = 0;
	}

	glBindVertexArray(0);

	if (gl->vertexBuffer != 0) {
		glDeleteBuffers(1, &gl->vertexBuffer);
		gl->vertexBuffer = 0;
	}

	if (gl->tcoordBuffer != 0) {
		glDeleteBuffers(1, &gl->tcoordBuffer);
		gl->tcoordBuffer = 0;
	}

	if (gl->colorBuffer != 0) {
		glDeleteBuffers(1, &gl->colorBuffer);
		gl->colorBuffer = 0;
	}

	if (gl->vertexArray != 0) {
		glDeleteVertexArrays(1, &gl->vertexArray);
		gl->vertexArray = 0;
	}

	free(gl);
}

FONS_DEF FONScontext* glfonsCreate(int width, int height, int flags)
{
	FONSparams params;
	GLFONScontext* gl;

	gl = (GLFONScontext*)malloc(sizeof(GLFONScontext));
	if (gl == NULL) goto error;
	memset(gl, 0, sizeof(GLFONScontext));

	memset(&params, 0, sizeof(params));
	params.width = width;
	params.height = height;
	params.flags = (unsigned char)flags;
	params.renderCreate = glfons__renderCreate;
	params.renderResize = glfons__renderResize;
	params.renderUpdate = glfons__renderUpdate;
	params.renderDraw = glfons__renderDraw; 
	params.renderDelete = glfons__renderDelete;
	params.userPtr = gl;

	return fonsCreateInternal(&params);

error:
	if (gl != NULL) free(gl);
	return NULL;
}

FONS_DEF void glfonsDelete(FONScontext* ctx)
{
	fonsDeleteInternal(ctx);
}

FONS_DEF unsigned int glfonsRGBA(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
	return (r) | (g << 8) | (b << 16) | (a << 24);
}

#endif // GLFONTSTASH_IMPLEMENTATION
