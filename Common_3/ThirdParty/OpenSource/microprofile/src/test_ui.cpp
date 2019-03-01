#define MICROPROFILE_IMPL
#define MICROPROFILEUI_IMPL

#include "microprofile.h"
#include "microprofileui.h"

void MicroProfileDrawBox(int, int, int, int, unsigned int, MicroProfileBoxType)
{
}

void MicroProfileDrawText(int, int, unsigned int, char const*, unsigned int)
{
}

void MicroProfileDrawLine2D(unsigned int, float*, unsigned int)
{
}

int main()
{
	MicroProfileSetForceEnable(true);

	MicroProfileOnThreadCreate("Main");

	{
		MICROPROFILE_SCOPEI("Group", "Name", -1);
		MICROPROFILE_LABEL("Group", "Label");
		MICROPROFILE_LABELF("Group", "Label %d", 5);
	}

	MicroProfileFlip();

	MicroProfileDraw(128, 64);

	MicroProfileOnThreadExit();
}
