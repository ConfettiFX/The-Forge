#ifdef __APPLE__
#include <OpenGL/gl3.h>
#else
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#endif

#ifdef __linux__
void glQueryCounter (GLuint, GLenum) {}
void glGetQueryObjectui64v (GLuint, GLenum, GLuint64 *) {}
#endif

#define MICROPROFILE_IMPL
#define MICROPROFILEUI_IMPL
#define MICROPROFILEDRAW_IMPL

#define MICROPROFILE_GPU_TIMERS_GL 1

#include "microprofile.h"
#include "microprofileui.h"
#include "microprofiledraw.h"

int main()
{
	MicroProfileSetForceEnable(true);

	MicroProfileOnThreadCreate("Main");

	{
		MICROPROFILE_SCOPEI("Group", "Name", -1);
		MICROPROFILE_SCOPEGPUI("NameGpu", -1);

		MICROPROFILE_LABEL("Group", "Label");
		MICROPROFILE_LABELF("Group", "Label %d", 5);
	}

	MicroProfileFlip();

	MicroProfileDraw(128, 64);

	MicroProfileOnThreadExit();
}
