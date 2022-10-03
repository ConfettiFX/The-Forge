
#if defined(__ANDROID__) || defined(ANDROID)
#include <android_native_app_glue.h>
#define CATCH_CONFIG_RUNNER
#include "catch.hpp"
void android_main(struct android_app* stateMain)
{
	app_dummy();
	char* cmd[] = {"gainputtest"};
	Catch::Session().run( 1, cmd );
}
#else
#define CATCH_CONFIG_MAIN
#include "catch.hpp"
#endif


