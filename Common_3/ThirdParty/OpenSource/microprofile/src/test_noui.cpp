#define MICROPROFILE_IMPL

#include "microprofile.h"

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

	MicroProfileOnThreadExit();
}
